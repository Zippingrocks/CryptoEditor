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

#include "ui/DahSceneManifest.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <unordered_set>

namespace tb::ui {
namespace {

using JsonError = std::runtime_error;

QJsonObject objectField(const QJsonObject &object, const char *key) {
  const auto value = object.value(key);
  if (!value.isObject()) {
    throw JsonError{std::string{"Expected object field: "} + key};
  }
  return value.toObject();
}

QJsonArray arrayField(const QJsonObject &object, const char *key) {
  const auto value = object.value(key);
  if (!value.isArray()) {
    throw JsonError{std::string{"Expected array field: "} + key};
  }
  return value.toArray();
}

std::string stringField(const QJsonObject &object, const char *key) {
  const auto value = object.value(key);
  if (!value.isString() || value.toString().isEmpty()) {
    throw JsonError{std::string{"Expected non-empty string field: "} + key};
  }
  return value.toString().toStdString();
}

std::string optionalStringField(const QJsonObject &object, const char *key) {
  const auto value = object.value(key);
  if (value.isUndefined() || value.isNull()) {
    return {};
  }
  if (!value.isString()) {
    throw JsonError{std::string{"Expected string field: "} + key};
  }
  return value.toString().toStdString();
}

DahSceneMaterial::AddressMode addressModeField(const QJsonObject &object,
                                               const char *key) {
  const auto value = stringField(object, key);
  if (value == "wrap") {
    return DahSceneMaterial::AddressMode::Repeat;
  }
  if (value == "clamp") {
    return DahSceneMaterial::AddressMode::ClampToEdge;
  }
  throw JsonError{std::string{"Unsupported texture address mode: "} + value};
}

std::uint64_t unsignedField(const QJsonObject &object, const char *key) {
  const auto value = object.value(key);
  if (!value.isDouble()) {
    throw JsonError{std::string{"Expected unsigned integer field: "} + key};
  }
  const auto number = value.toDouble();
  if (number < 0.0 || !std::isfinite(number) || std::floor(number) != number ||
      number > static_cast<double>(std::numeric_limits<std::uint64_t>::max())) {
    throw JsonError{std::string{"Invalid unsigned integer field: "} + key};
  }
  return static_cast<std::uint64_t>(number);
}

QByteArray readFile(const std::filesystem::path &path) {
  auto file = QFile{QString::fromStdWString(path.wstring())};
  if (!file.open(QIODevice::ReadOnly)) {
    throw JsonError{"Could not read " + path.string()};
  }
  return file.readAll();
}

std::string sha256(const QByteArray &bytes) {
  return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256)
      .toHex()
      .toStdString();
}

std::filesystem::path safeChildPath(const std::filesystem::path &root,
                                    const std::string &relative) {
  const auto candidate = std::filesystem::path{relative};
  if (candidate.empty() || candidate.is_absolute()) {
    throw JsonError{"Scene payload path must be relative"};
  }
  for (const auto &part : candidate) {
    if (part == "..") {
      throw JsonError{"Scene payload path escapes the scene directory"};
    }
  }
  return (root / candidate).lexically_normal();
}

DahSceneBufferView parseBufferView(const QJsonObject &object,
                                   const QByteArray &buffer,
                                   const std::string &label) {
  auto view = DahSceneBufferView{unsignedField(object, "offset"),
                                 unsignedField(object, "length"),
                                 stringField(object, "sha256")};
  const auto bufferSize = static_cast<std::uint64_t>(buffer.size());
  if (view.offset > bufferSize || view.length > bufferSize - view.offset) {
    throw JsonError{label + " buffer view is out of range"};
  }
  const auto bytes = buffer.mid(static_cast<qsizetype>(view.offset),
                                static_cast<qsizetype>(view.length));
  if (sha256(bytes) != view.sha256) {
    throw JsonError{label + " buffer view SHA-256 mismatch"};
  }
  return view;
}

DahSceneCounts parseCounts(const QJsonObject &object) {
  return {unsignedField(object, "terrain_placements"),
          unsignedField(object, "terrain_lods"),
          unsignedField(object, "terrain_geometries"),
          unsignedField(object, "world_buckets"),
          unsignedField(object, "world_placements"),
          unsignedField(object, "building_placements"),
          unsignedField(object, "building_renderable_placements"),
          unsignedField(object, "building_geometries"),
          unsignedField(object, "geometries"),
          unsignedField(object, "materials"),
          unsignedField(object, "textures"),
          unsignedField(object, "lod0_vertices"),
          unsignedField(object, "lod0_triangles"),
          unsignedField(object, "lod1_vertices"),
          unsignedField(object, "lod1_triangles")};
}

void validateArea42Counts(const DahSceneManifest &manifest) {
  if (manifest.profileId != "dah1-retail-xbox-us" ||
      manifest.mapName != "area42") {
    return;
  }
  const auto &counts = manifest.counts;
  if (counts.terrainPlacements != 92 || counts.terrainLods != 184 ||
      counts.terrainGeometries != 184 || counts.worldBuckets != 41 ||
      counts.worldPlacements != 849 || counts.buildingPlacements != 18 ||
      counts.buildingRenderablePlacements != 17 ||
      counts.buildingGeometries != 7 || counts.geometries != 191 ||
      counts.lod0Vertices != 122956 || counts.lod0Triangles != 103634 ||
      counts.lod1Vertices != 16240 || counts.lod1Triangles != 8120) {
    throw JsonError{"Area 42 scene has unexpected native geometry counts"};
  }
}

} // namespace

DahSceneLoadResult
loadDahSceneManifest(const std::filesystem::path &sceneDirectory) {
  try {
    const auto root =
        std::filesystem::absolute(sceneDirectory).lexically_normal();
    const auto document =
        QJsonDocument::fromJson(readFile(root / "scene.json"));
    if (!document.isObject()) {
      throw JsonError{"Scene manifest root must be an object"};
    }
    const auto json = document.object();
    const auto schema = objectField(json, "schema");
    if (stringField(schema, "name") != "dah-editor-scene" ||
        unsignedField(schema, "version") != 2) {
      throw JsonError{"Unsupported DAH editor scene schema"};
    }
    if (json.value("read_only") != true) {
      throw JsonError{"DAH editor scene must be explicitly read-only"};
    }

    auto result = DahSceneManifest{};
    result.sceneDirectory = root;
    result.profileId = stringField(json, "profile_id");
    result.mapName = stringField(json, "map_name");
    result.counts = parseCounts(objectField(json, "counts"));

    const auto buffers = objectField(json, "buffers");
    result.bufferPath = safeChildPath(root, stringField(buffers, "file"));
    result.bufferByteLength = unsignedField(buffers, "byte_length");
    result.bufferSha256 = stringField(buffers, "sha256");
    const auto buffer = readFile(result.bufferPath);
    if (static_cast<std::uint64_t>(buffer.size()) != result.bufferByteLength) {
      throw JsonError{"Scene buffer length mismatch"};
    }
    if (sha256(buffer) != result.bufferSha256) {
      throw JsonError{"Scene buffer SHA-256 mismatch"};
    }

    auto geometryIds = std::unordered_set<std::string>{};
    for (const auto &value : arrayField(json, "geometries")) {
      if (!value.isObject()) {
        throw JsonError{"Scene geometry entry must be an object"};
      }
      const auto geometryJson = value.toObject();
      auto geometry = DahSceneGeometry{};
      geometry.id = stringField(geometryJson, "id");
      geometry.kind = stringField(geometryJson, "kind");
      if (!geometryIds.insert(geometry.id).second) {
        throw JsonError{"Duplicate scene geometry id: " + geometry.id};
      }
      const auto geometryCounts = objectField(geometryJson, "counts");
      geometry.vertexCount = unsignedField(geometryCounts, "vertices");
      geometry.triangleCount = unsignedField(geometryCounts, "triangles");
      const auto vertexLayout = objectField(geometryJson, "vertex_layout");
      geometry.vertexStride = unsignedField(vertexLayout, "stride");
      const auto positionLayout = objectField(vertexLayout, "position");
      const auto uvLayout = objectField(vertexLayout, "uv");
      if ((geometry.kind != "terrain" &&
           geometry.kind != "static-world-object") ||
          (geometry.kind == "terrain" && geometry.vertexStride != 24) ||
          (geometry.kind == "static-world-object" &&
           geometry.vertexStride != 28) ||
          unsignedField(positionLayout, "offset") != 0 ||
          stringField(positionLayout, "format") != "float32x3-little-endian" ||
          stringField(positionLayout, "status") != "verified" ||
          unsignedField(uvLayout, "offset") != 12 ||
          stringField(uvLayout, "format") != "float32x2-little-endian" ||
          (stringField(uvLayout, "status") != "candidate" &&
           stringField(uvLayout, "status") != "verified")) {
        throw JsonError{"Unsupported DAH native vertex layout"};
      }
      const auto views = objectField(geometryJson, "buffers");
      geometry.vertices = parseBufferView(objectField(views, "vertices"),
                                          buffer, geometry.id + " vertices");
      geometry.indexStrip =
          parseBufferView(objectField(views, "index_strip"), buffer,
                          geometry.id + " index strip");
      geometry.triangles = parseBufferView(objectField(views, "triangles"),
                                           buffer, geometry.id + " triangles");
      geometry.segments = parseBufferView(objectField(views, "segments"),
                                          buffer, geometry.id + " segments");
      auto segmentTriangleCount = std::uint64_t{0};
      for (const auto &segmentValue : arrayField(geometryJson, "segments")) {
        if (!segmentValue.isObject()) {
          throw JsonError{"Scene geometry segment must be an object"};
        }
        const auto segmentJson = segmentValue.toObject();
        auto segment = DahSceneGeometry::Segment{
            unsignedField(segmentJson, "triangle_count"),
            unsignedField(segmentJson, "material_slot")};
        segmentTriangleCount += segment.triangleCount;
        geometry.segmentRecords.push_back(segment);
      }
      if (segmentTriangleCount != geometry.triangleCount) {
        throw JsonError{geometry.id +
                        " segment triangle counts do not match geometry"};
      }
      result.geometries.push_back(std::move(geometry));
    }

    auto materialIds = std::unordered_set<std::string>{};
    for (const auto &value : arrayField(json, "materials")) {
      if (!value.isObject()) {
        throw JsonError{"Scene material entry must be an object"};
      }
      const auto materialJson = value.toObject();
      auto material = DahSceneMaterial{};
      material.id = stringField(materialJson, "id");
      if (!materialIds.insert(material.id).second) {
        throw JsonError{"Duplicate scene material id: " + material.id};
      }
      const auto binding = materialJson.value("binding");
      if (binding.isObject()) {
        material.textureId = stringField(binding.toObject(), "texture");
      } else if (!binding.isNull()) {
        throw JsonError{"Scene material binding must be an object or null"};
      }
      const auto sampler = materialJson.value("sampler");
      if (sampler.isObject()) {
        const auto samplerObject = sampler.toObject();
        if (stringField(samplerObject, "status") != "verified") {
          throw JsonError{"Scene material sampler must be verified"};
        }
        material.addressU = addressModeField(samplerObject, "address_u");
        material.addressV = addressModeField(samplerObject, "address_v");
      } else if (!sampler.isUndefined() && !sampler.isNull()) {
        throw JsonError{"Scene material sampler must be an object"};
      }
      result.materials.push_back(std::move(material));
    }

    auto textureIds = std::unordered_set<std::string>{};
    for (const auto &value : arrayField(json, "textures")) {
      if (!value.isObject()) {
        throw JsonError{"Scene texture entry must be an object"};
      }
      const auto textureJson = value.toObject();
      auto texture = DahSceneTexture{};
      texture.id = stringField(textureJson, "id");
      if (!textureIds.insert(texture.id).second) {
        throw JsonError{"Duplicate scene texture id: " + texture.id};
      }
      const auto preview = objectField(textureJson, "preview");
      texture.previewPath = safeChildPath(root, stringField(preview, "file"));
      texture.previewSha256 = stringField(preview, "sha256");
      if (sha256(readFile(texture.previewPath)) != texture.previewSha256) {
        throw JsonError{"Scene texture preview SHA-256 mismatch: " +
                        texture.id};
      }
      result.textures.push_back(std::move(texture));
    }

    for (const auto &material : result.materials) {
      if (material.textureId && !textureIds.contains(*material.textureId)) {
        throw JsonError{"Scene material references missing texture: " +
                        *material.textureId};
      }
    }

    const auto parsePlacements =
        [&](const QJsonArray &values,
            std::vector<DahScenePlacement> &destination) {
          for (const auto &value : values) {
            if (!value.isObject()) {
              throw JsonError{"Scene placement entry must be an object"};
            }
            const auto placementJson = value.toObject();
            auto placement = DahScenePlacement{};
            placement.id = stringField(placementJson, "id");
            placement.editorCollection =
                optionalStringField(placementJson, "editor_collection");
            const auto archetype = placementJson.value("archetype");
            if (archetype.isObject()) {
              const auto archetypeObject = archetype.toObject();
              placement.nativeClass =
                  optionalStringField(archetypeObject, "native_class");
              placement.assetName =
                  optionalStringField(archetypeObject, "asset_name");
            } else if (!archetype.isUndefined() && !archetype.isNull()) {
              throw JsonError{
                  "Scene placement archetype must be an object or null"};
            }
            const auto matrix =
                arrayField(placementJson, "matrix_row_major_f32");
            if (matrix.size() != 16) {
              throw JsonError{"Scene placement matrix must contain 16 numbers"};
            }
            for (auto i = 0; i < 16; ++i) {
              if (!matrix[i].isDouble()) {
                throw JsonError{"Scene placement matrix contains a non-number"};
              }
              placement.matrix[static_cast<size_t>(i)] = matrix[i].toDouble();
            }
            for (const auto &lodValue : arrayField(placementJson, "lods")) {
              if (!lodValue.isObject()) {
                throw JsonError{"Scene LOD entry must be an object"};
              }
              const auto lodJson = lodValue.toObject();
              auto lod = DahSceneLod{};
              lod.index = unsignedField(lodJson, "index");
              lod.geometryId = stringField(lodJson, "geometry");
              if (!geometryIds.contains(lod.geometryId)) {
                throw JsonError{"Scene LOD references missing geometry: " +
                                lod.geometryId};
              }
              for (const auto &slot : arrayField(lodJson, "material_slots")) {
                if (!slot.isString()) {
                  throw JsonError{"Scene material slot must be a string"};
                }
                auto materialId = slot.toString().toStdString();
                if (!materialIds.contains(materialId)) {
                  throw JsonError{"Scene LOD references missing material: " +
                                  materialId};
                }
                lod.materialSlots.push_back(std::move(materialId));
              }
              const auto geometryIt = std::ranges::find_if(
                  result.geometries, [&](const auto &geometry) {
                    return geometry.id == lod.geometryId;
                  });
              for (const auto &segment : geometryIt->segmentRecords) {
                if (segment.materialSlot >= lod.materialSlots.size()) {
                  throw JsonError{"Scene geometry segment references an "
                                  "out-of-range material slot"};
                }
              }
              placement.lods.push_back(std::move(lod));
            }
            destination.push_back(std::move(placement));
          }
        };
    parsePlacements(arrayField(json, "placements"), result.placements);
    parsePlacements(arrayField(json, "object_placements"),
                    result.objectPlacements);

    auto terrainLodCount = std::uint64_t{0};
    for (const auto &placement : result.placements) {
      terrainLodCount += placement.lods.size();
    }
    const auto buildingPlacementCount =
        static_cast<std::uint64_t>(std::ranges::count_if(
            result.objectPlacements, [](const auto &placement) {
              return placement.editorCollection == "buildings";
            }));
    const auto renderableBuildingCount =
        static_cast<std::uint64_t>(std::ranges::count_if(
            result.objectPlacements, [](const auto &placement) {
              return placement.editorCollection == "buildings" &&
                     !placement.lods.empty();
            }));

    if (result.geometries.size() != result.counts.geometries ||
        result.materials.size() != result.counts.materials ||
        result.textures.size() != result.counts.textures ||
        result.placements.size() != result.counts.terrainPlacements ||
        result.objectPlacements.size() != result.counts.worldPlacements ||
        terrainLodCount != result.counts.terrainLods ||
        buildingPlacementCount != result.counts.buildingPlacements ||
        renderableBuildingCount != result.counts.buildingRenderablePlacements) {
      throw JsonError{"Scene collection counts do not match the manifest"};
    }
    validateArea42Counts(result);
    return result;
  } catch (const std::exception &error) {
    return DahSceneLoadError{error.what()};
  }
}

} // namespace tb::ui
