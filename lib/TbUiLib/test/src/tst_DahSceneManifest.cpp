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

#include "ui/CatchConfig.h"
#include "ui/DahSceneManifest.h"

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

QJsonObject bufferView(const QByteArray& buffer)
{
  return {
    {"offset", 0},
    {"length", buffer.size()},
    {"sha256", hash(buffer)},
  };
}

void writeTestScene(
  const QString& root,
  const int schemaVersion = 1,
  const bool badBufferHash = false,
  const QString& bufferPath = "buffers.bin")
{
  const auto buffer = QByteArray{"native-bytes"};
  if (bufferPath == "buffers.bin")
  {
    writeFile(root + "/buffers.bin", buffer);
  }
  const auto view = bufferView(buffer);
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
          {"vertices", view},
          {"index_strip", view},
          {"triangles", view},
          {"segments", view}}}}}},
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
    }
  }
}

} // namespace tb::ui
