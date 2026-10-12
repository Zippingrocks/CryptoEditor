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

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtEndian>

#include "ui/CatchConfig.h"
#include "ui/DahSceneManifest.h"
#include "ui/DahSceneReference.h"

#include <bit>

#include <catch2/catch_test_macros.hpp>

namespace tb::ui
{
namespace
{

QString hash(const QByteArray& bytes)
{
  return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

void writeFile(const QString& path, const QByteArray& contents)
{
  auto file = QFile{path};
  REQUIRE(file.open(QIODevice::WriteOnly));
  REQUIRE(file.write(contents) == contents.size());
}

QJsonObject bufferView(
  const QByteArray& completeBuffer, const int offset, const int length)
{
  const auto payload = completeBuffer.mid(offset, length);
  return {
    {"offset", offset},
    {"length", length},
    {"sha256", hash(payload)},
  };
}

void appendU32(QByteArray& buffer, const std::uint32_t value)
{
  const auto littleEndian = qToLittleEndian(value);
  buffer.append(reinterpret_cast<const char*>(&littleEndian), sizeof(littleEndian));
}

void appendF32(QByteArray& buffer, const float value)
{
  appendU32(buffer, std::bit_cast<std::uint32_t>(value));
}

void appendVertex(
  QByteArray& buffer,
  const float x,
  const float y,
  const float z,
  const float u,
  const float v)
{
  appendF32(buffer, x);
  appendF32(buffer, y);
  appendF32(buffer, z);
  appendF32(buffer, u);
  appendF32(buffer, v);
  buffer.append(QByteArray{4, '\0'});
}

void writeTestScene(
  const QString& root,
  const int schemaVersion = 2,
  const bool badBufferHash = false,
  const QString& bufferPath = "buffers.bin")
{
  auto buffer = QByteArray{};
  appendVertex(buffer, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  appendVertex(buffer, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
  appendVertex(buffer, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);
  const auto triangleOffset = buffer.size();
  appendU32(buffer, 0);
  appendU32(buffer, 1);
  appendU32(buffer, 2);
  if (bufferPath == "buffers.bin")
  {
    writeFile(root + "/buffers.bin", buffer);
  }
  const auto preview = QByteArray{"verified preview payload"};
  writeFile(root + "/preview.png", preview);
  const auto vertices = bufferView(buffer, 0, triangleOffset);
  const auto triangles = bufferView(buffer, triangleOffset, 12);
  const auto empty = bufferView(buffer, buffer.size(), 0);
  auto matrix = QJsonArray{};
  for (auto i = 0; i < 16; ++i)
  {
    matrix.append(i % 5 == 0 ? 1.0 : 0.0);
  }
  const auto manifest = QJsonObject{
    {"schema", QJsonObject{{"name", "dah-editor-scene"}, {"version", schemaVersion}}},
    {"profile_id", "test-profile"},
    {"map_name", "test-map"},
    {"read_only", true},
    {"buffers",
     QJsonObject{
       {"file", bufferPath},
       {"byte_length", buffer.size()},
       {"sha256", badBufferHash ? QString{64, '0'} : hash(buffer)}}},
    {"counts",
     QJsonObject{
       {"terrain_placements", 1},
       {"terrain_lods", 1},
       {"terrain_geometries", 1},
       {"world_buckets", 0},
       {"world_placements", 0},
       {"building_placements", 0},
       {"building_renderable_placements", 0},
       {"building_geometries", 0},
       {"prop_placements", 0},
       {"prop_renderable_placements", 0},
       {"prop_geometries", 0},
       {"renderable_structure_placements", 0},
       {"geometries", 1},
       {"materials", 1},
       {"textures", 1},
       {"lod0_vertices", 3},
       {"lod0_triangles", 1},
       {"lod1_vertices", 0},
       {"lod1_triangles", 0}}},
    {"geometries",
     QJsonArray{QJsonObject{
       {"id", "geometry:test"},
       {"kind", "terrain"},
       {"counts", QJsonObject{{"vertices", 3}, {"triangles", 1}}},
       {"vertex_layout",
        QJsonObject{
          {"stride", 24},
          {"position",
           QJsonObject{
             {"offset", 0},
             {"format", "float32x3-little-endian"},
             {"status", "verified"}}},
          {"uv",
           QJsonObject{
             {"offset", 12},
             {"format", "float32x2-little-endian"},
             {"status", "verified"}}}}},
       {"buffers",
        QJsonObject{
          {"vertices", vertices},
          {"index_strip", empty},
          {"triangles", triangles},
          {"segments", empty}}},
       {"segments",
        QJsonArray{QJsonObject{{"triangle_count", 1}, {"material_slot", 0}}}}}}},
    {"materials",
     QJsonArray{QJsonObject{
       {"id", "material:test"},
       {"binding", QJsonObject{{"texture", "texture:test"}}},
       {"sampler",
        QJsonObject{
          {"address_u", "clamp"},
          {"address_v", "wrap"},
          {"status", "verified"}}}}}},
    {"textures",
     QJsonArray{QJsonObject{
       {"id", "texture:test"},
       {"preview", QJsonObject{{"file", "preview.png"}, {"sha256", hash(preview)}}}}}},
    {"placements",
     QJsonArray{QJsonObject{
       {"id", "terrain-placement:000"},
       {"matrix_row_major_f32", matrix},
       {"lods",
        QJsonArray{QJsonObject{
          {"index", 0},
          {"geometry", "geometry:test"},
          {"material_slots", QJsonArray{"material:test"}}}}}}}},
    {"object_placements", QJsonArray{}},
  };
  writeFile(root + "/scene.json", QJsonDocument{manifest}.toJson());
}

} // namespace

TEST_CASE("DahSceneManifest", "[DahSceneManifest]")
{
  SECTION("loads a verified read-only scene")
  {
    auto directory = QTemporaryDir{};
    REQUIRE(directory.isValid());
    writeTestScene(directory.path());

    const auto result = loadDahSceneManifest(directory.path().toStdWString());
    REQUIRE(std::holds_alternative<DahSceneManifest>(result));
    const auto& scene = std::get<DahSceneManifest>(result);
    CHECK(scene.profileId == "test-profile");
    CHECK(scene.geometries.size() == 1);
    CHECK(scene.placements.size() == 1);
    CHECK(scene.placements.front().lods.front().geometryId == "geometry:test");
    CHECK(
      scene.materials.front().addressU
      == DahSceneMaterial::AddressMode::ClampToEdge);
    CHECK(scene.materials.front().addressV == DahSceneMaterial::AddressMode::Repeat);
  }

  SECTION("builds a placed native triangle reference")
  {
    auto directory = QTemporaryDir{};
    REQUIRE(directory.isValid());
    writeTestScene(directory.path());
    const auto manifestResult = loadDahSceneManifest(directory.path().toStdWString());
    REQUIRE(std::holds_alternative<DahSceneManifest>(manifestResult));

    const auto referenceResult =
      buildDahSceneReference(std::get<DahSceneManifest>(manifestResult), 0);
    REQUIRE(std::holds_alternative<DahSceneReference>(referenceResult));
    const auto& reference = std::get<DahSceneReference>(referenceResult);
    CHECK(reference.placementCount == 1);
    CHECK(reference.nativeTriangleCount == 1);
    CHECK(reference.vertexCount() == 3);
    CHECK(reference.batches.size() == 1);
    CHECK(reference.texturedMaterialCount == 1);
    CHECK(reference.batches.front().materialId == "material:test");
    CHECK(reference.batches.front().texturePreviewPath.has_value());
    CHECK(
      reference.batches.front().addressU
      == DahSceneMaterial::AddressMode::ClampToEdge);
    CHECK(
      reference.batches.front().addressV == DahSceneMaterial::AddressMode::Repeat);
    CHECK(reference.bounds.min == vm::vec3d{0.0, 0.0, 0.0});
    CHECK(reference.bounds.max == vm::vec3d{1.0, 1.0, 0.0});
  }

  SECTION("rejects unsupported schemas")
  {
    auto directory = QTemporaryDir{};
    REQUIRE(directory.isValid());
    writeTestScene(directory.path(), 1);
    CHECK(std::holds_alternative<DahSceneLoadError>(
      loadDahSceneManifest(directory.path().toStdWString())));
  }

  SECTION("rejects changed buffers")
  {
    auto directory = QTemporaryDir{};
    REQUIRE(directory.isValid());
    writeTestScene(directory.path(), 2, true);
    const auto result = loadDahSceneManifest(directory.path().toStdWString());
    REQUIRE(std::holds_alternative<DahSceneLoadError>(result));
    CHECK(
      std::get<DahSceneLoadError>(result).message.find("SHA-256") != std::string::npos);
  }

  SECTION("rejects payload path escapes")
  {
    auto directory = QTemporaryDir{};
    REQUIRE(directory.isValid());
    writeTestScene(directory.path(), 2, false, "../buffers.bin");
    CHECK(std::holds_alternative<DahSceneLoadError>(
      loadDahSceneManifest(directory.path().toStdWString())));
  }

  SECTION("loads the externally supplied Area 42 export")
  {
    const auto path = qEnvironmentVariable("CRYPTOEDITOR_DAHSCENE");
    if (!path.isEmpty())
    {
      const auto result = loadDahSceneManifest(path.toStdWString());
      REQUIRE(std::holds_alternative<DahSceneManifest>(result));
      const auto& scene = std::get<DahSceneManifest>(result);
      CHECK(scene.profileId == "dah1-retail-xbox-us");
      CHECK(scene.mapName == "area42");
      CHECK(scene.counts.terrainPlacements == 92);
      CHECK(scene.counts.terrainLods == 184);
      CHECK(scene.counts.worldPlacements == 849);
      CHECK(scene.counts.buildingPlacements == 18);
      CHECK(scene.counts.buildingRenderablePlacements == 18);
      CHECK(scene.counts.propPlacements == 23);
      CHECK(scene.counts.propRenderablePlacements == 23);
      CHECK(scene.counts.renderableStructurePlacements == 41);
      CHECK(scene.counts.geometries == 203);
      CHECK(scene.geometries.size() == 203);
      CHECK(scene.materials.size() == 190);
      CHECK(scene.textures.size() == 124);
      CHECK(scene.placements.size() == 92);
      CHECK(scene.objectPlacements.size() == 849);

      const auto referenceResult = buildDahSceneReference(scene, 0);
      REQUIRE(std::holds_alternative<DahSceneReference>(referenceResult));
      const auto& reference = std::get<DahSceneReference>(referenceResult);
      CHECK(reference.placementCount == 133);
      CHECK(reference.nativeTriangleCount == 126885);
      CHECK(reference.vertexCount() == 380655);
      CHECK(reference.batches.size() == 157);
      CHECK(reference.texturedMaterialCount == 155);

      const auto lod1Result = buildDahSceneReference(scene, 1);
      REQUIRE(std::holds_alternative<DahSceneReference>(lod1Result));
      const auto& lod1Reference = std::get<DahSceneReference>(lod1Result);
      CHECK(lod1Reference.placementCount == 133);
      CHECK(lod1Reference.nativeTriangleCount == 31371);
      CHECK(lod1Reference.vertexCount() == 94113);
      CHECK(lod1Reference.batches.size() == 86);
      CHECK(lod1Reference.texturedMaterialCount == 84);
    }
  }
}

} // namespace tb::ui
