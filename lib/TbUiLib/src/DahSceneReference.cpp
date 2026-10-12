/*
 Copyright (C) 2026 CryptoEditor contributors

 This file is part of CryptoEditor.

 CryptoEditor is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 CryptoEditor is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with CryptoEditor. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ui/DahSceneReference.h"

#include <QFile>
#include <QtEndian>

#include "base/Color.h"
#include "base/PreferenceManager.h"
#include "gl/ActiveShader.h"
#include "gl/Camera.h"
#include "gl/GlInterface.h"
#include "gl/Material.h"
#include "gl/PrimType.h"
#include "gl/ResourceManager.h"
#include "gl/Shaders.h"
#include "gl/Texture.h"
#include "gl/TextureResource.h"
#include "gl/VertexArray.h"
#include "mdl/LoadImageTexture.h"
#include "prefs/Preferences.h"
#include "render/RenderContext.h"
#include "render/Renderable.h"

#include "vm/vec.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <ranges>
#include <stdexcept>
#include <unordered_map>

namespace tb::ui
{
namespace
{

QByteArray readBuffer(const std::filesystem::path& path)
{
  auto file = QFile{QString::fromStdWString(path.wstring())};
  if (!file.open(QIODevice::ReadOnly))
  {
    throw std::runtime_error{"Could not read DAH scene payload: " + path.string()};
  }
  return file.readAll();
}

std::uint32_t readU32(const QByteArray& buffer, const std::uint64_t offset)
{
  if (
    offset > static_cast<std::uint64_t>(buffer.size())
    || sizeof(std::uint32_t) > static_cast<std::uint64_t>(buffer.size()) - offset)
  {
    throw std::runtime_error{"DAH scene numeric read is out of range"};
  }
  return qFromLittleEndian<std::uint32_t>(
    reinterpret_cast<const unsigned char*>(buffer.constData() + offset));
}

float readF32(const QByteArray& buffer, const std::uint64_t offset)
{
  return std::bit_cast<float>(readU32(buffer, offset));
}

vm::vec3f transformPosition(
  const vm::vec3f& position, const std::array<double, 16>& matrix)
{
  return vm::vec3f{
    static_cast<float>(
      position.x() * matrix[0] + position.y() * matrix[4] + position.z() * matrix[8]
      + matrix[12]),
    static_cast<float>(
      position.x() * matrix[1] + position.y() * matrix[5] + position.z() * matrix[9]
      + matrix[13]),
    static_cast<float>(
      position.x() * matrix[2] + position.y() * matrix[6] + position.z() * matrix[10]
      + matrix[14])};
}

vm::vec3f faceNormal(const vm::vec3f& a, const vm::vec3f& b, const vm::vec3f& c)
{
  const auto edge1 = b - a;
  const auto edge2 = c - a;
  auto normal = vm::cross(edge1, edge2);
  const auto length = vm::length(normal);
  if (length <= 0.000001f)
  {
    return vm::vec3f{0.0f, 0.0f, 1.0f};
  }
  normal = normal / length;
  return normal.z() < 0.0f ? -normal : normal;
}

Color fallbackColor(const std::string& materialId)
{
  auto hash = std::uint32_t{2166136261u};
  for (const auto character : materialId)
  {
    hash = (hash ^ static_cast<unsigned char>(character)) * 16777619u;
  }
  const auto channel = [&](const unsigned int shift) {
    return 0.3f + 0.5f * static_cast<float>((hash >> shift) & 0xffu) / 255.0f;
  };
  return Color{RgbaF{channel(0), channel(8), channel(16), 1.0f}};
}

std::shared_ptr<gl::TextureResource> loadTextureResource(
  const std::filesystem::path& path, gl::ResourceManager& resourceManager)
{
  const auto bytes = readBuffer(path);
  auto textureResult = mdl::loadImageTextureFromMemory(
    reinterpret_cast<const std::uint8_t*>(bytes.constData()),
    static_cast<size_t>(bytes.size()));
  if (textureResult.is_error())
  {
    throw std::runtime_error{"Could not decode DAH texture preview: " + path.string()};
  }
  auto resource = gl::createTextureResource(std::move(textureResult).value());
  resourceManager.addResource(resource);
  return resource;
}

struct RenderBatch
{
  gl::VertexArray vertexArray;
  std::unique_ptr<gl::Material> material;
  Color fallback;

  RenderBatch(
    std::vector<DahSceneReferenceBatch::Vertex> vertices,
    std::string materialId,
    std::shared_ptr<gl::TextureResource> textureResource)
    : vertexArray{gl::VertexArray::move(std::move(vertices))}
    , material{textureResource ? std::make_unique<gl::Material>(materialId, std::move(textureResource)) : nullptr}
    , fallback{fallbackColor(materialId)}
  {
  }
};

class DahReferenceRenderer : public render::DirectRenderable
{
private:
  std::vector<RenderBatch> m_batches;

public:
  DahReferenceRenderer(DahSceneReference reference, gl::ResourceManager& resourceManager)
  {
    auto resources =
      std::unordered_map<std::string, std::shared_ptr<gl::TextureResource>>{};
    m_batches.reserve(reference.batches.size());
    for (auto& batch : reference.batches)
    {
      auto textureResource = std::shared_ptr<gl::TextureResource>{};
      if (batch.texturePreviewPath)
      {
        const auto key = batch.texturePreviewPath->generic_string();
        const auto [it, inserted] = resources.try_emplace(key);
        if (inserted)
        {
          it->second = loadTextureResource(*batch.texturePreviewPath, resourceManager);
        }
        textureResource = it->second;
      }
      m_batches.emplace_back(
        std::move(batch.vertices), std::move(batch.materialId), textureResource);
    }
  }

  void prepare(gl::Gl& gl, gl::VboManager& vboManager) override
  {
    for (auto& batch : m_batches)
    {
      batch.vertexArray.prepare(gl, vboManager);
    }
  }

  void render(render::RenderContext& context) override
  {
    auto& gl = context.gl();
    auto shader = gl::ActiveShader{gl, context.shaderManager(), gl::Shaders::FaceShader};
    auto& prefs = PreferenceManager::instance();

    gl.disable(GL_CULL_FACE);
    gl.polygonMode(GL_FRONT_AND_BACK, GL_FILL);
    gl.enable(GL_TEXTURE_2D);
    gl.activeTexture(GL_TEXTURE0);
    shader.set("Brightness", prefs.get(Preferences::Brightness));
    shader.set("RenderGrid", context.showGrid());
    shader.set("GridSize", static_cast<float>(context.gridSize()));
    shader.set("GridAlpha", prefs.get(Preferences::GridAlpha));
    shader.set("Material", 0);
    shader.set("ApplyTinting", false);
    shader.set("GrayScale", false);
    shader.set("CameraPosition", context.camera().position());
    shader.set("ShadeFaces", context.shadeFaces());
    shader.set("ShowFog", context.showFog());
    shader.set("Alpha", 1.0f);
    shader.set("EnableMasked", false);
    shader.set("AlphaFuncCompare", size_t{0});
    shader.set("AlphaFuncThreshold", 0.5f);
    shader.set("ShowSoftMapBounds", !context.softMapBounds().is_empty());
    shader.set("SoftMapBoundsMin", context.softMapBounds().min);
    shader.set("SoftMapBoundsMax", context.softMapBounds().max);
    shader.set(
      "SoftMapBoundsColor",
      RgbaF{prefs.get(Preferences::SoftMapBoundsColor).to<RgbF>(), 0.1f});

    for (auto& batch : m_batches)
    {
      const auto* texture = gl::getTexture(batch.material.get());
      const auto applyMaterial = context.showMaterials() && texture != nullptr;
      shader.set("ApplyMaterial", applyMaterial);
      shader.set("Color", texture ? texture->averageColor() : batch.fallback);
      if (applyMaterial)
      {
        batch.material->activate(gl, context.minFilterMode(), context.magFilterMode());
      }
      if (batch.vertexArray.setup(gl, shader.program()))
      {
        batch.vertexArray.render(gl, gl::PrimType::Triangles);
        batch.vertexArray.cleanup(gl, shader.program());
      }
      if (applyMaterial)
      {
        batch.material->deactivate(gl);
      }
    }
    gl.enable(GL_CULL_FACE);
  }
};

} // namespace

std::uint64_t DahSceneReference::vertexCount() const
{
  auto result = std::uint64_t{0};
  for (const auto& batch : batches)
  {
    result += batch.vertices.size();
  }
  return result;
}

DahSceneReferenceResult buildDahSceneReference(
  const DahSceneManifest& manifest, const std::uint64_t lodIndex)
{
  try
  {
    const auto buffer = readBuffer(manifest.bufferPath);
    auto geometries = std::unordered_map<std::string, const DahSceneGeometry*>{};
    for (const auto& geometry : manifest.geometries)
    {
      geometries.emplace(geometry.id, &geometry);
    }
    auto materials = std::unordered_map<std::string, const DahSceneMaterial*>{};
    for (const auto& material : manifest.materials)
    {
      materials.emplace(material.id, &material);
    }
    auto textures = std::unordered_map<std::string, std::filesystem::path>{};
    for (const auto& texture : manifest.textures)
    {
      textures.emplace(texture.id, texture.previewPath);
    }

    auto result = DahSceneReference{};
    result.lodIndex = lodIndex;
    auto batchIndices = std::unordered_map<std::string, size_t>{};

    auto haveBounds = false;
    auto boundsMin = vm::vec3d{};
    auto boundsMax = vm::vec3d{};
    auto addToBounds = [&](const vm::vec3f& position) {
      const auto point = vm::vec3d{position};
      if (!haveBounds)
      {
        boundsMin = point;
        boundsMax = point;
        haveBounds = true;
      }
      else
      {
        for (size_t axis = 0; axis < 3; ++axis)
        {
          boundsMin[axis] = std::min(boundsMin[axis], point[axis]);
          boundsMax[axis] = std::max(boundsMax[axis], point[axis]);
        }
      }
    };

    for (const auto& placement : manifest.placements)
    {
      const auto lodIt = std::ranges::find_if(
        placement.lods, [&](const auto& lod) { return lod.index == lodIndex; });
      if (lodIt == placement.lods.end())
      {
        throw std::runtime_error{
          "DAH scene placement lacks requested LOD: " + placement.id};
      }
      const auto geometryIt = geometries.find(lodIt->geometryId);
      if (geometryIt == geometries.end())
      {
        throw std::runtime_error{"DAH scene placement references missing geometry"};
      }
      const auto& geometry = *geometryIt->second;
      if (geometry.vertices.length != geometry.vertexCount * 24u)
      {
        throw std::runtime_error{"DAH scene vertex view does not match stride 24"};
      }
      if (geometry.triangles.length != geometry.triangleCount * 12u)
      {
        throw std::runtime_error{"DAH scene triangle view has an invalid length"};
      }

      auto positions = std::vector<vm::vec3f>{};
      auto uvs = std::vector<vm::vec2f>{};
      positions.reserve(static_cast<size_t>(geometry.vertexCount));
      uvs.reserve(static_cast<size_t>(geometry.vertexCount));
      for (std::uint64_t i = 0; i < geometry.vertexCount; ++i)
      {
        const auto offset = geometry.vertices.offset + i * 24u;
        positions.push_back(transformPosition(
          vm::vec3f{
            readF32(buffer, offset),
            readF32(buffer, offset + 4u),
            readF32(buffer, offset + 8u)},
          placement.matrix));
        uvs.emplace_back(readF32(buffer, offset + 12u), readF32(buffer, offset + 16u));
      }

      auto triangleCursor = std::uint64_t{0};
      for (const auto& segment : geometry.segmentRecords)
      {
        const auto& materialId = lodIt->materialSlots.at(segment.materialSlot);
        auto [batchIt, inserted] =
          batchIndices.try_emplace(materialId, result.batches.size());
        if (inserted)
        {
          const auto materialIt = materials.find(materialId);
          if (materialIt == materials.end())
          {
            throw std::runtime_error{"DAH scene segment references missing material"};
          }
          auto texturePreviewPath = std::optional<std::filesystem::path>{};
          if (materialIt->second->textureId)
          {
            const auto textureIt = textures.find(*materialIt->second->textureId);
            if (textureIt == textures.end())
            {
              throw std::runtime_error{"DAH scene material references missing texture"};
            }
            texturePreviewPath = textureIt->second;
            ++result.texturedMaterialCount;
          }
          result.batches.push_back(
            DahSceneReferenceBatch{materialId, texturePreviewPath, {}});
        }
        auto& batch = result.batches[batchIt->second];

        for (std::uint64_t i = 0; i < segment.triangleCount; ++i)
        {
          const auto offset = geometry.triangles.offset + triangleCursor * 12u;
          const auto index0 = readU32(buffer, offset);
          const auto index1 = readU32(buffer, offset + 4u);
          const auto index2 = readU32(buffer, offset + 8u);
          if (
            index0 >= positions.size() || index1 >= positions.size()
            || index2 >= positions.size())
          {
            throw std::runtime_error{"DAH scene triangle index is out of range"};
          }
          const auto& a = positions[index0];
          const auto& b = positions[index1];
          const auto& c = positions[index2];
          const auto normal = faceNormal(a, b, c);
          batch.vertices.emplace_back(a, normal, uvs[index0]);
          batch.vertices.emplace_back(b, normal, uvs[index1]);
          batch.vertices.emplace_back(c, normal, uvs[index2]);
          addToBounds(a);
          addToBounds(b);
          addToBounds(c);
          ++triangleCursor;
        }
      }
      if (triangleCursor != geometry.triangleCount)
      {
        throw std::runtime_error{"DAH scene segments do not cover all triangles"};
      }
      result.nativeTriangleCount += geometry.triangleCount;
      ++result.placementCount;
    }

    if (!haveBounds)
    {
      throw std::runtime_error{"DAH scene reference mesh is empty"};
    }
    result.bounds = vm::bbox3d{boundsMin, boundsMax};
    return result;
  }
  catch (const std::exception& error)
  {
    return DahSceneLoadError{error.what()};
  }
}

std::unique_ptr<render::DirectRenderable> createDahSceneReferenceRenderer(
  DahSceneReference reference, gl::ResourceManager& resourceManager)
{
  return std::make_unique<DahReferenceRenderer>(std::move(reference), resourceManager);
}

} // namespace tb::ui
