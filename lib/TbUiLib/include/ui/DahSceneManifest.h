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

#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace tb::ui
{

struct DahSceneBufferView
{
  std::uint64_t offset = 0;
  std::uint64_t length = 0;
  std::string sha256;
};

struct DahSceneGeometry
{
  std::string id;
  std::string kind;
  std::uint64_t vertexStride = 0;
  std::uint64_t vertexCount = 0;
  std::uint64_t triangleCount = 0;
  DahSceneBufferView vertices;
  DahSceneBufferView indexStrip;
  DahSceneBufferView triangles;
  DahSceneBufferView segments;
  struct Segment
  {
    std::uint64_t triangleCount = 0;
    std::uint64_t materialSlot = 0;
  };
  std::vector<Segment> segmentRecords;
};

struct DahSceneLod
{
  std::uint64_t index = 0;
  std::string geometryId;
  std::vector<std::string> materialSlots;
};

struct DahScenePlacement
{
  std::string id;
  std::string editorCollection;
  std::string nativeClass;
  std::string assetName;
  std::array<double, 16> matrix{};
  std::vector<DahSceneLod> lods;
};

struct DahSceneTexture
{
  std::string id;
  std::filesystem::path previewPath;
  std::string previewSha256;
};

struct DahSceneMaterial
{
  enum class AddressMode
  {
    Repeat,
    ClampToEdge,
  };

  std::string id;
  std::optional<std::string> textureId;
  AddressMode addressU = AddressMode::Repeat;
  AddressMode addressV = AddressMode::Repeat;
};

struct DahSceneCounts
{
  std::uint64_t terrainPlacements = 0;
  std::uint64_t terrainLods = 0;
  std::uint64_t terrainGeometries = 0;
  std::uint64_t worldBuckets = 0;
  std::uint64_t worldPlacements = 0;
  std::uint64_t buildingPlacements = 0;
  std::uint64_t buildingRenderablePlacements = 0;
  std::uint64_t buildingGeometries = 0;
  std::uint64_t geometries = 0;
  std::uint64_t materials = 0;
  std::uint64_t textures = 0;
  std::uint64_t lod0Vertices = 0;
  std::uint64_t lod0Triangles = 0;
  std::uint64_t lod1Vertices = 0;
  std::uint64_t lod1Triangles = 0;
};

struct DahSceneManifest
{
  std::filesystem::path sceneDirectory;
  std::filesystem::path bufferPath;
  std::string profileId;
  std::string mapName;
  std::string bufferSha256;
  std::uint64_t bufferByteLength = 0;
  DahSceneCounts counts;
  std::vector<DahSceneGeometry> geometries;
  std::vector<DahSceneMaterial> materials;
  std::vector<DahSceneTexture> textures;
  std::vector<DahScenePlacement> placements;
  std::vector<DahScenePlacement> objectPlacements;
};

struct DahSceneLoadError
{
  std::string message;
};

using DahSceneLoadResult = std::variant<DahSceneManifest, DahSceneLoadError>;

DahSceneLoadResult loadDahSceneManifest(const std::filesystem::path& sceneDirectory);

} // namespace tb::ui
