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
#include "gl/GlInterface.h"
#include "gl/PrimType.h"
#include "gl/VertexArray.h"
#include "render/RenderContext.h"
#include "render/Renderable.h"
#include "render/TriangleRenderer.h"

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
    throw std::runtime_error{"Could not read DAH scene buffer: " + path.string()};
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

class DahReferenceRenderer : public render::DirectRenderable
{
private:
  render::TriangleRenderer m_triangles;

public:
  explicit DahReferenceRenderer(std::vector<DahSceneReference::Vertex> vertices)
    : m_triangles{gl::VertexArray::move(std::move(vertices)), gl::PrimType::Triangles}
  {
    m_triangles.setUseColor(true);
    m_triangles.setColor(Color{RgbaF{0.32f, 0.66f, 0.82f, 1.0f}});
  }

  void prepare(gl::Gl& gl, gl::VboManager& vboManager) override
  {
    m_triangles.prepare(gl, vboManager);
  }

  void render(render::RenderContext& context) override
  {
    auto& gl = context.gl();
    gl.disable(GL_CULL_FACE);
    gl.polygonMode(GL_FRONT_AND_BACK, GL_FILL);
    m_triangles.render(context);
    gl.enable(GL_CULL_FACE);
  }
};

} // namespace

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

    auto result = DahSceneReference{};
    result.lodIndex = lodIndex;
    result.vertices.reserve(
      static_cast<size_t>(
        lodIndex == 0 ? manifest.counts.lod0Triangles : manifest.counts.lod1Triangles)
      * 3u);

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
      positions.reserve(static_cast<size_t>(geometry.vertexCount));
      for (std::uint64_t i = 0; i < geometry.vertexCount; ++i)
      {
        const auto offset = geometry.vertices.offset + i * 24u;
        positions.push_back(transformPosition(
          vm::vec3f{
            readF32(buffer, offset),
            readF32(buffer, offset + 4u),
            readF32(buffer, offset + 8u)},
          placement.matrix));
      }

      for (std::uint64_t i = 0; i < geometry.triangleCount; ++i)
      {
        const auto offset = geometry.triangles.offset + i * 12u;
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
        result.vertices.emplace_back(a, normal);
        result.vertices.emplace_back(b, normal);
        result.vertices.emplace_back(c, normal);
        addToBounds(a);
        addToBounds(b);
        addToBounds(c);
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
  DahSceneReference reference)
{
  return std::make_unique<DahReferenceRenderer>(std::move(reference.vertices));
}

} // namespace tb::ui
