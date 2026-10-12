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
  std::uint64_t positionOffset = 0;
  std::uint64_t uvOffset = 12;
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

struct DahSceneRenderState
{
  std::string alphaMode = "opaque";
  float alphaReference = 0.0f;
  std::string blendSource = "src_alpha";
  std::string blendDestination = "one_minus_src_alpha";
  std::string blendEquation = "add";
  bool depthWrite = true;
  bool alphaTest = false;
  bool nativeTerrain = false;
  float rgbScale = 1.0f;
  float rigidOpacity = 1.0f;
  int terrainLayer = 0;
  bool sky = false;
  int lightingMode = 0; // 0: native vertex color, 1: authored sun, 2: sky material color
  int pixelShader = 0;
  std::array<float, 3> materialDiffuse{1.0f, 1.0f, 1.0f};
  std::array<float, 3> materialAdditive{};
  std::array<float, 8> uvTransform{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
};

struct DahScenePart
{
  std::string geometryId;
  std::vector<std::string> materialSlots;
  std::array<double, 16> matrix{};
};

struct DahSceneLod
{
  std::uint64_t index = 0;
  std::string geometryId;
  std::vector<std::string> materialSlots;
  std::vector<DahScenePart> parts;
  std::vector<DahSceneRenderState> segmentStates;
};

struct DahScenePlacement
{
  std::string id;
  std::string editorCollection;
  std::string nativeClass;
  std::string assetName;
  std::array<double, 16> matrix{};
  std::array<float, 3> color{1.0f, 1.0f, 1.0f};
  std::string previewStatus;
  std::vector<DahSceneLod> lods;
  std::string nativeLayer;
  std::string nativeList;
  std::string lightRole;
  std::optional<std::int64_t> entityId;
  std::vector<std::string> tagIds;
};

struct DahSceneRegion
{
  std::string id;
  std::string name;
  std::string nameHash;
  std::string nativeLayerHash;
  std::array<double, 3> position{};
  std::array<double, 16> worldToRegionMatrix{};
  std::array<double, 16> regionToWorldMatrix{};
};

struct DahSceneLayer
{
  std::string hash;
  std::string name;
  std::uint64_t index = 0;
  std::uint64_t loadGroup = 0;
  std::uint64_t placementCount = 0;
  std::uint64_t scriptCallCount = 0;
};

struct DahSceneEntityTag
{
  std::string id;
  std::string name;
  std::string nameHash;
  std::vector<std::int64_t> entityIds;
};

struct DahSceneMarker
{
  std::string id;
  std::string kind;
  std::string label;
  std::string status;
  std::array<double, 3> position{};
};

struct DahSceneObjectiveTarget
{
  std::string kind;
  std::string name;
  std::string status;
  std::optional<std::array<double, 3>> position;
  std::vector<std::string> worldPlacementIds;
  std::vector<std::string> nativeLayers;
};

struct DahSceneObjective
{
  std::string id;
  std::string mission;
  std::string name;
  std::string type;
  std::string status;
  std::string source;
  std::vector<DahSceneObjectiveTarget> targets;
  std::vector<std::string> layerCommands;
};

struct DahSceneTextureMip
{
  std::filesystem::path path;
  std::string sha256;
  std::uint64_t width = 0;
  std::uint64_t height = 0;
};

struct DahSceneTexture
{
  std::string id;
  std::filesystem::path previewPath;
  std::string previewSha256;
  std::vector<DahSceneTextureMip> nativeMips;
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
  std::string alphaMode = "opaque";
  float alphaReference = 0.0f;
  std::optional<DahSceneRenderState> actorState;
  bool actorParametersVerified = false;
  std::array<float, 8> actorUvTransform{1, 0, 0, 0, 0, 1, 0, 0};
  std::array<float, 3> diffuse{1, 1, 1};
  std::array<float, 3> additive{};
  int pixelShader = 0;
  bool nativeSampler = false;
  bool nearestMagnification = false;
  bool mipFilter = true;
  float lodBias = 0.0f;
};

struct DahSceneSun
{
  std::string role;
  std::array<float, 3> direction{};
  std::array<float, 3> diffuse{};
  std::array<float, 3> ambient{};
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
  std::uint64_t propPlacements = 0;
  std::uint64_t propRenderablePlacements = 0;
  std::uint64_t propGeometries = 0;
  std::uint64_t renderableStructurePlacements = 0;
  std::uint64_t geometries = 0;
  std::uint64_t materials = 0;
  std::uint64_t textures = 0;
  std::uint64_t lod0Vertices = 0;
  std::uint64_t lod0Triangles = 0;
  std::uint64_t lod1Vertices = 0;
  std::uint64_t lod1Triangles = 0;
  std::uint64_t scatterBatches = 0;
  std::uint64_t scatterPlacements = 0;
  std::uint64_t scatterGeometries = 0;
  std::uint64_t humanPlacements = 0;
  std::uint64_t humanRenderablePlacements = 0;
  std::uint64_t humanGeometries = 0;
  std::uint64_t sceneMarkers = 0;
  std::uint64_t landingSites = 0;
  std::uint64_t spawnDefinitions = 0;
};

struct DahSceneManifest
{
  std::filesystem::path sceneDirectory;
  std::filesystem::path bufferPath;
  std::string profileId;
  std::string mapName;
  std::string bufferSha256;
  std::uint64_t bufferByteLength = 0;
  std::uint64_t schemaVersion = 2;
  DahSceneCounts counts;
  std::vector<DahSceneGeometry> geometries;
  std::vector<DahSceneMaterial> materials;
  std::vector<DahSceneTexture> textures;
  std::vector<DahScenePlacement> placements;
  std::vector<DahScenePlacement> objectPlacements;
  std::vector<DahScenePlacement> scatterPlacements;
  std::vector<DahScenePlacement> landingPlacements;
  std::vector<DahScenePlacement> skyPlacements;
  std::vector<DahSceneSun> suns;
  std::vector<DahSceneMarker> markers;
  std::vector<DahSceneObjective> objectives;
  std::vector<DahSceneRegion> regions;
  std::vector<DahSceneLayer> layers;
  std::vector<DahSceneEntityTag> entityTags;
  std::vector<std::string> unresolvedSystems;
};

struct DahSceneLoadError
{
  std::string message;
};

using DahSceneLoadResult = std::variant<DahSceneManifest, DahSceneLoadError>;

DahSceneLoadResult loadDahSceneManifest(const std::filesystem::path& sceneDirectory);

} // namespace tb::ui
