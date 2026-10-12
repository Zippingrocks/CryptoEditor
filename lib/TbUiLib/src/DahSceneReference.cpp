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
#include <QOpenGLContext>
#include <QOpenGLFunctions>
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
#include <functional>
#include <ranges>
#include <stdexcept>
#include <tuple>
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

std::int16_t readI16(const QByteArray& buffer, const std::uint64_t offset)
{
  if (
    offset > static_cast<std::uint64_t>(buffer.size())
    || sizeof(std::int16_t) > static_cast<std::uint64_t>(buffer.size()) - offset)
  {
    throw std::runtime_error{"DAH scene numeric read is out of range"};
  }
  return qFromLittleEndian<std::int16_t>(
    reinterpret_cast<const unsigned char*>(buffer.constData() + offset));
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

vm::vec3f transformNormal(const vm::vec3f& normal, const std::array<double, 16>& matrix)
{
  auto transformed = vm::vec3f{
    static_cast<float>(
      normal.x() * matrix[0] + normal.y() * matrix[4] + normal.z() * matrix[8]),
    static_cast<float>(
      normal.x() * matrix[1] + normal.y() * matrix[5] + normal.z() * matrix[9]),
    static_cast<float>(
      normal.x() * matrix[2] + normal.y() * matrix[6] + normal.z() * matrix[10])};
  const auto length = vm::length(transformed);
  return length > 0.000001f ? transformed / length : vm::vec3f{0.0f, 0.0f, 1.0f};
}

vm::vec4f terrainDiffuseColor(const QByteArray& buffer, const std::uint64_t offset)
{
  const auto value = readU32(buffer, offset);
  constexpr auto Scale = 1.0f / 255.0f;
  // Native D3DCOLOR is 0xAARRGGBB; overlay segments consume the alpha channel.
  return vm::vec4f{
    static_cast<float>((value >> 16u) & 0xffu) * Scale,
    static_cast<float>((value >> 8u) & 0xffu) * Scale,
    static_cast<float>(value & 0xffu) * Scale,
    static_cast<float>((value >> 24u) & 0xffu) * Scale};
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
  if (materialId.starts_with("editor-marker:"))
  {
    return Color{RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
  }
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
  DahSceneMaterial::AddressMode addressU;
  DahSceneMaterial::AddressMode addressV;
  std::string alphaMode;
  float alphaReference;
  DahSceneRenderState state;

  RenderBatch(
    std::vector<DahSceneReferenceBatch::Vertex> vertices,
    std::string materialId,
    std::shared_ptr<gl::TextureResource> textureResource,
    const DahSceneMaterial::AddressMode addressU,
    const DahSceneMaterial::AddressMode addressV,
    std::string alphaMode,
    const float alphaReference,
    DahSceneRenderState state)
    : vertexArray{gl::VertexArray::move(std::move(vertices))}
    , material{textureResource ? std::make_unique<gl::Material>(materialId, std::move(textureResource)) : nullptr}
    , fallback{fallbackColor(materialId)}
    , addressU{addressU}
    , addressV{addressV}
    , alphaMode{std::move(alphaMode)}
    , alphaReference{alphaReference}
    , state{std::move(state)}
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
        std::move(batch.vertices),
        std::move(batch.materialId),
        textureResource,
        batch.addressU,
        batch.addressV,
        std::move(batch.alphaMode),
        batch.alphaReference,
        batch.state);
    }
    std::stable_sort(
      m_batches.begin(), m_batches.end(), [](const auto& a, const auto& b) {
        const auto order = [](const auto& batch) {
          return batch.alphaMode != "blend" ? 0 : batch.state.nativeTerrain ? 1 : 2;
        };
        const auto key = [&](const auto& batch) {
          return std::tuple{
            order(batch),
            batch.state.nativeTerrain ? 0 : 1,
            batch.state.nativeTerrain ? batch.state.terrainLayer : 0};
        };
        return key(a) < key(b);
      });
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
    auto shader =
      gl::ActiveShader{gl, context.shaderManager(), gl::Shaders::DahReferenceShader};
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
    shader.set("ApplyVertexColor", true);
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
      const auto blend = context.showMaterials() && batch.alphaMode == "blend";
      shader.set(
        "Brightness",
        batch.state.nativeTerrain ? 1.0f : prefs.get(Preferences::Brightness));
      shader.set("ShadeFaces", !batch.state.nativeTerrain && context.shadeFaces());
      // Editor fog is not DAH's environment fog and must not masquerade as it.
      shader.set("ShowFog", false);
      gl.depthFunc(GL_LEQUAL);
      auto* functions = QOpenGLContext::currentContext()->functions();
      const auto equation = batch.state.blendEquation == "reverse_subtract"
                              ? GL_FUNC_REVERSE_SUBTRACT
                            : batch.state.blendEquation == "subtract" ? GL_FUNC_SUBTRACT
                                                                      : GL_FUNC_ADD;
      functions->glBlendEquation(equation);
      if (blend)
      {
        gl.enable(GL_BLEND);
        gl.blendFunc(
          batch.state.blendSource == "one" ? GL_ONE : GL_SRC_ALPHA,
          batch.state.blendDestination == "one"    ? GL_ONE
          : batch.state.blendDestination == "zero" ? GL_ZERO
                                                   : GL_ONE_MINUS_SRC_ALPHA);
      }
      else
      {
        gl.disable(GL_BLEND);
      }
      gl.depthMask(
        !context.showMaterials() || batch.state.depthWrite ? GL_TRUE : GL_FALSE);
      // Native opaque terrain still runs GREATER alpha testing (default 64/255).
      shader.set(
        "EnableMasked",
        context.showMaterials()
          && (batch.state.nativeTerrain || batch.alphaMode != "opaque"));
      shader.set("AlphaFuncCompare", size_t{2}); // Native GREATER comparison.
      shader.set("AlphaFuncThreshold", batch.alphaReference);
      shader.set("ApplyMaterial", applyMaterial);
      shader.set(
        "Color", applyMaterial ? Color{RgbaF{1.0f, 1.0f, 1.0f, 1.0f}} : batch.fallback);
      if (applyMaterial)
      {
        batch.material->activate(gl, context.minFilterMode(), context.magFilterMode());
        gl.texParameteri(
          GL_TEXTURE_2D,
          GL_TEXTURE_WRAP_S,
          batch.addressU == DahSceneMaterial::AddressMode::ClampToEdge ? GL_CLAMP_TO_EDGE
                                                                       : GL_REPEAT);
        gl.texParameteri(
          GL_TEXTURE_2D,
          GL_TEXTURE_WRAP_T,
          batch.addressV == DahSceneMaterial::AddressMode::ClampToEdge ? GL_CLAMP_TO_EDGE
                                                                       : GL_REPEAT);
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
    gl.depthMask(GL_TRUE);
    QOpenGLContext::currentContext()->functions()->glBlendEquation(GL_FUNC_ADD);
    gl.enable(GL_BLEND);
    gl.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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

    std::function<void(const DahScenePlacement&, bool)> appendPlacement;
    appendPlacement = [&](
                        const DahScenePlacement& placement,
                        const bool requireRequestedLod) {
      auto lodIt = std::ranges::find_if(
        placement.lods, [&](const auto& lod) { return lod.index == lodIndex; });
      if (
        lodIt == placement.lods.end() && !requireRequestedLod && !placement.lods.empty())
      {
        lodIt = placement.lods.begin();
      }
      if (lodIt == placement.lods.end())
      {
        if (!requireRequestedLod)
        {
          return;
        }
        throw std::runtime_error{
          "DAH scene placement lacks requested LOD: " + placement.id};
      }
      if (!lodIt->parts.empty())
      {
        for (const auto& part : lodIt->parts)
        {
          auto child = placement;
          child.lods = {DahSceneLod{lodIt->index, part.geometryId, part.materialSlots}};
          child.matrix.fill(0.0);
          for (size_t row = 0; row < 4; ++row)
            for (size_t column = 0; column < 4; ++column)
              for (size_t k = 0; k < 4; ++k)
                child.matrix[row * 4 + column] +=
                  part.matrix[row * 4 + k] * placement.matrix[k * 4 + column];
          appendPlacement(child, false);
        }
        // Components are not additional authored map entities.
        result.placementCount -= lodIt->parts.size() - 1;
        return;
      }
      const auto geometryIt = geometries.find(lodIt->geometryId);
      if (geometryIt == geometries.end())
      {
        throw std::runtime_error{"DAH scene placement references missing geometry"};
      }
      const auto& geometry = *geometryIt->second;
      if (geometry.vertices.length != geometry.vertexCount * geometry.vertexStride)
      {
        throw std::runtime_error{"DAH scene vertex view does not match its stride"};
      }
      if (geometry.triangles.length != geometry.triangleCount * 12u)
      {
        throw std::runtime_error{"DAH scene triangle view has an invalid length"};
      }

      auto positions = std::vector<vm::vec3f>{};
      auto uvs = std::vector<vm::vec2f>{};
      auto normals = std::vector<vm::vec3f>{};
      auto colors = std::vector<vm::vec4f>{};
      positions.reserve(static_cast<size_t>(geometry.vertexCount));
      uvs.reserve(static_cast<size_t>(geometry.vertexCount));
      normals.reserve(static_cast<size_t>(geometry.vertexCount));
      colors.reserve(static_cast<size_t>(geometry.vertexCount));
      for (std::uint64_t i = 0; i < geometry.vertexCount; ++i)
      {
        const auto offset = geometry.vertices.offset + i * geometry.vertexStride;
        const auto positionOffset = offset + geometry.positionOffset;
        const auto uvOffset = offset + geometry.uvOffset;
        positions.push_back(transformPosition(
          vm::vec3f{
            readF32(buffer, positionOffset),
            readF32(buffer, positionOffset + 4u),
            readF32(buffer, positionOffset + 8u)},
          placement.matrix));
        uvs.emplace_back(readF32(buffer, uvOffset), readF32(buffer, uvOffset + 4u));
        if (geometry.kind == "static-world-object")
        {
          const auto snorm = [](const std::int16_t value) {
            return std::max(-1.0f, static_cast<float>(value) / 32767.0f);
          };
          normals.push_back(transformNormal(
            vm::vec3f{
              snorm(readI16(buffer, offset + 20u)),
              snorm(readI16(buffer, offset + 22u)),
              snorm(readI16(buffer, offset + 24u))},
            placement.matrix));
          colors.emplace_back(1.0f, 1.0f, 1.0f, 1.0f);
        }
        else if (geometry.kind == "human-reference-pose")
        {
          normals.push_back(transformNormal(
            vm::vec3f{
              readF32(buffer, offset + 12u),
              readF32(buffer, offset + 16u),
              readF32(buffer, offset + 20u)},
            placement.matrix));
          colors.emplace_back(1.0f, 1.0f, 1.0f, 1.0f);
        }
        else if (geometry.kind == "terrain")
        {
          normals.emplace_back(0.0f, 0.0f, 1.0f);
          colors.push_back(terrainDiffuseColor(buffer, offset + 20u));
        }
        else
        {
          normals.emplace_back(0.0f, 0.0f, 1.0f);
          colors.emplace_back(
            placement.color[0], placement.color[1], placement.color[2], 1.0f);
        }
      }

      auto triangleCursor = std::uint64_t{0};
      auto segmentIndex = size_t{0};
      for (const auto& segment : geometry.segmentRecords)
      {
        const auto& materialId = lodIt->materialSlots.at(segment.materialSlot);
        const auto materialIt = materials.find(materialId);
        if (materialIt == materials.end())
          throw std::runtime_error{"DAH scene material missing"};
        auto state = DahSceneRenderState{};
        state.alphaMode = materialIt->second->alphaMode;
        state.alphaReference = materialIt->second->alphaReference;
        state.depthWrite = state.alphaMode != "blend";
        if (!lodIt->segmentStates.empty())
          state = lodIt->segmentStates.at(segmentIndex);
        ++segmentIndex;
        const auto batchKey =
          materialId + ":" + geometry.kind + ":" + std::to_string(state.terrainLayer)
          + ":" + state.alphaMode + ":" + state.blendEquation + ":" + state.blendSource
          + ":" + state.blendDestination + ":" + std::to_string(state.depthWrite) + ":"
          + std::to_string(state.alphaReference) + ":" + std::to_string(state.rgbScale)
          + ":" + std::to_string(state.nativeTerrain);
        auto [batchIt, inserted] =
          batchIndices.try_emplace(batchKey, result.batches.size());
        if (inserted)
        {
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
          result.batches.push_back(DahSceneReferenceBatch{
            materialId,
            texturePreviewPath,
            materialIt->second->addressU,
            materialIt->second->addressV,
            {},
            state.alphaMode,
            state.alphaReference,
            state});
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
          const auto flatNormal = faceNormal(a, b, c);
          const auto& normal0 = (geometry.kind == "static-world-object"
                                 || geometry.kind == "human-reference-pose")
                                  ? normals[index0]
                                  : flatNormal;
          const auto& normal1 = (geometry.kind == "static-world-object"
                                 || geometry.kind == "human-reference-pose")
                                  ? normals[index1]
                                  : flatNormal;
          const auto& normal2 = (geometry.kind == "static-world-object"
                                 || geometry.kind == "human-reference-pose")
                                  ? normals[index2]
                                  : flatNormal;
          const auto color = [&](const size_t index) {
            const auto& native = colors[index];
            return vm::vec4f{
              native.x() * state.rgbScale,
              native.y() * state.rgbScale,
              native.z() * state.rgbScale,
              native.w()};
          };
          batch.vertices.emplace_back(a, normal0, uvs[index0], color(index0));
          batch.vertices.emplace_back(b, normal1, uvs[index1], color(index1));
          batch.vertices.emplace_back(c, normal2, uvs[index2], color(index2));
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
    };

    for (const auto& placement : manifest.placements)
    {
      appendPlacement(placement, true);
    }
    for (const auto& placement : manifest.objectPlacements)
    {
      appendPlacement(placement, false);
    }
    for (const auto& placement : manifest.scatterPlacements)
    {
      appendPlacement(placement, false);
    }
    for (const auto& placement : manifest.landingPlacements)
    {
      appendPlacement(placement, false);
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

DahSceneReference buildDahSceneMarkerReference(const DahSceneManifest& manifest)
{
  auto result = DahSceneReference{};
  auto batch = DahSceneReferenceBatch{};
  batch.materialId = "editor-marker:spawn-and-landing";
  for (const auto& marker : manifest.markers)
  {
    const auto landing = marker.kind == "landing-site";
    const auto radius = landing ? 2.5f : 0.6f;
    const auto center = vm::vec3f{
      static_cast<float>(marker.position[0]),
      static_cast<float>(marker.position[1]),
      static_cast<float>(marker.position[2]) + radius};
    const auto color =
      landing ? vm::vec4f{0.2f, 1.0f, 0.9f, 1.0f} : vm::vec4f{1.0f, 0.55f, 0.15f, 1.0f};
    const std::array<vm::vec3f, 6> points = {
      center + vm::vec3f{radius, 0, 0},
      center + vm::vec3f{0, radius, 0},
      center + vm::vec3f{-radius, 0, 0},
      center + vm::vec3f{0, -radius, 0},
      center + vm::vec3f{0, 0, radius},
      center + vm::vec3f{0, 0, -radius}};
    for (size_t i = 0; i < 4; ++i)
    {
      for (const auto tip : {size_t{4}, size_t{5}})
      {
        const auto normal = faceNormal(points[i], points[(i + 1) % 4], points[tip]);
        for (const auto vertex : {i, (i + 1) % 4, tip})
        {
          batch.vertices.emplace_back(points[vertex], normal, vm::vec2f{}, color);
        }
      }
    }
  }
  if (!batch.vertices.empty())
  {
    result.batches.push_back(std::move(batch));
  }
  return result;
}

} // namespace tb::ui
