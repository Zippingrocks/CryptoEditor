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

void appendVertex(QByteArray& buffer, const float x, const float y, const float z)
{
  appendF32(buffer, x);
  appendF32(buffer, y);
  appendF32(buffer, z);
  buffer.append(QByteArray{12, '\0'});
}

void writeTestScene(
  const QString& root,
  const int schemaVersion = 1,
  const bool badBufferHash = false,
  const QString& bufferPath = "buffers.bin")
{
  auto buffer = QByteArray{};
  appendVertex(buffer, 0.0f, 0.0f, 0.0f);
  appendVertex(buffer, 1.0f, 0.0f, 0.0f);
  appendVertex(buffer, 0.0f, 1.0f, 0.0f);
  const auto triangleOffset = buffer.size();
  appendU32(buffer, 0);
  appendU32(buffer, 1);
  appendU32(buffer, 2);
  if (bufferPath == "buffers.bin")
  {
    writeFile(root + "/buffers.bin", buffer);
  }
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
       {"placements", 1},
       {"lods", 1},
       {"geometries", 1},
       {"materials", 0},
       {"textures", 0},
       {"lod0_vertices", 3},
       {"lod0_triangles", 1},
       {"lod1_vertices", 0},
       {"lod1_triangles", 0}}},
    {"geometries",
     QJsonArray{QJsonObject{
       {"id", "geometry:test"},
       {"counts", QJsonObject{{"vertices", 3}, {"triangles", 1}}},
       {"buffers",
        QJsonObject{
          {"vertices", vertices},
          {"index_strip", empty},
          {"triangles", triangles},
          {"segments", empty}}}}}},
    {"materials", QJsonArray{}},
    {"textures", QJsonArray{}},
    {"placements",
     QJsonArray{QJsonObject{
       {"id", "terrain-placement:000"},
       {"matrix_row_major_f32", matrix},
       {"lods",
        QJsonArray{QJsonObject{
          {"index", 0},
          {"geometry", "geometry:test"},
          {"material_slots", QJsonArray{}}}}}}}},
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
    CHECK(reference.vertices.size() == 3);
    CHECK(reference.bounds.min == vm::vec3d{0.0, 0.0, 0.0});
    CHECK(reference.bounds.max == vm::vec3d{1.0, 1.0, 0.0});
  }

  SECTION("rejects unsupported schemas")
  {
    auto directory = QTemporaryDir{};
    REQUIRE(directory.isValid());
    writeTestScene(directory.path(), 2);
    CHECK(std::holds_alternative<DahSceneLoadError>(
      loadDahSceneManifest(directory.path().toStdWString())));
  }

  SECTION("rejects changed buffers")
  {
    auto directory = QTemporaryDir{};
    REQUIRE(directory.isValid());
    writeTestScene(directory.path(), 1, true);
    const auto result = loadDahSceneManifest(directory.path().toStdWString());
    REQUIRE(std::holds_alternative<DahSceneLoadError>(result));
    CHECK(
      std::get<DahSceneLoadError>(result).message.find("SHA-256") != std::string::npos);
  }

  SECTION("rejects payload path escapes")
  {
    auto directory = QTemporaryDir{};
    REQUIRE(directory.isValid());
    writeTestScene(directory.path(), 1, false, "../buffers.bin");
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
      CHECK(scene.counts.placements == 92);
      CHECK(scene.counts.lods == 184);
      CHECK(scene.counts.geometries == 184);
      CHECK(scene.geometries.size() == 184);
      CHECK(scene.materialIds.size() == 91);
      CHECK(scene.textures.size() == 67);
      CHECK(scene.placements.size() == 92);

      const auto referenceResult = buildDahSceneReference(scene, 0);
      REQUIRE(std::holds_alternative<DahSceneReference>(referenceResult));
      const auto& reference = std::get<DahSceneReference>(referenceResult);
      CHECK(reference.placementCount == 92);
      CHECK(reference.nativeTriangleCount == 103634);
      CHECK(reference.vertices.size() == 310902);
    }
  }
}

} // namespace tb::ui
