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
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLShaderProgram>
#include <QOpenGLVersionFunctionsFactory>
#include <QSurfaceFormat>
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
  appendU32(buffer, 0x80402010u); // Native D3DCOLOR: alpha, red, green, blue.
}

void writeTestScene(
  const QString& root,
  const int schemaVersion = 2,
  const bool badBufferHash = false,
  const QString& bufferPath = "buffers.bin",
  const int vertexStride = 24)
{
  auto buffer = QByteArray{};
  appendVertex(buffer, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  buffer.append(vertexStride - 24, '\0');
  appendVertex(buffer, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
  buffer.append(vertexStride - 24, '\0');
  appendVertex(buffer, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);
  buffer.append(vertexStride - 24, '\0');
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
       {"human_placements", 0},
       {"human_renderable_placements", 0},
       {"human_geometries", 0},
       {"scatter_batches", 0},
       {"scatter_placements", 0},
       {"scatter_geometries", 0},
       {"scene_markers", 0},
       {"landing_sites", 0},
       {"landing_visual_placements", 0},
       {"spawn_definitions", 0},
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
          {"stride", vertexStride},
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
          {"address_u", "clamp"}, {"address_v", "wrap"}, {"status", "verified"}}}}}},
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
    {"landing_placements", QJsonArray{}},
    {"scatter_placements", QJsonArray{}},
    {"scatter_batches", QJsonArray{}},
    {"scene_markers", QJsonArray{}},
    {"spawn_definitions", QJsonArray{}},
  };
  writeFile(root + "/scene.json", QJsonDocument{manifest}.toJson());
}

QJsonObject readTestScene(const QString& root)
{
  auto file = QFile{root + "/scene.json"};
  REQUIRE(file.open(QIODevice::ReadOnly));
  return QJsonDocument::fromJson(file.readAll()).object();
}

QJsonArray translationMatrix(const double x, const double y, const double z)
{
  return {1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, x, y, z, 1.0};
}

} // namespace

TEST_CASE("DahSceneManifest", "[DahSceneManifest]")
{
  SECTION("loads a verified read-only scene")
  {
    auto directory = QTemporaryDir{};
    REQUIRE(directory.isValid());
    writeTestScene(directory.path());

    auto json = readTestScene(directory.path());
    json["regions"] = QJsonArray{QJsonObject{
      {"id", "region:test"},
      {"name_candidates", QJsonArray{"test_region"}},
      {"name_hash", "0x00000001"},
      {"native_layer_hash", "0x00000002"},
      {"position", QJsonArray{1.0, 2.0, 3.0}},
      {"world_to_region_matrix",
       QJsonArray{
         QJsonArray{1.0, 0.0, 0.0, 0.0},
         QJsonArray{0.0, 1.0, 0.0, 0.0},
         QJsonArray{0.0, 0.0, 1.0, 0.0},
         QJsonArray{0.0, 0.0, 0.0, 1.0}}},
      {"region_to_world_matrix",
       QJsonArray{
         QJsonArray{1.0, 0.0, 0.0, 0.0},
         QJsonArray{0.0, 1.0, 0.0, 0.0},
         QJsonArray{0.0, 0.0, 1.0, 0.0},
         QJsonArray{0.0, 0.0, 0.0, 1.0}}}}};
    json["spawn_layers"] = QJsonArray{QJsonObject{
      {"hash", "0x00000002"},
      {"name_candidates", QJsonArray{"test_layer"}},
      {"index", 0},
      {"load_group", 0},
      {"placements", QJsonArray{QJsonObject{}}},
      {"script_calls", QJsonArray{QJsonObject{}, QJsonObject{}}}}};
    json["entity_tags"] = QJsonArray{QJsonObject{
      {"id", "tag:test"},
      {"name_candidates", QJsonArray{"test_tag"}},
      {"name_hash", "0x00000003"},
      {"entity_ids", QJsonArray{42}}}};
    json["unknowns"] = QJsonArray{"collision remains unresolved"};
    writeFile(directory.path() + "/scene.json", QJsonDocument{json}.toJson());

    const auto result = loadDahSceneManifest(directory.path().toStdWString());
    REQUIRE(std::holds_alternative<DahSceneManifest>(result));
    const auto& scene = std::get<DahSceneManifest>(result);
    CHECK(scene.profileId == "test-profile");
    CHECK(scene.geometries.size() == 1);
    CHECK(scene.placements.size() == 1);
    CHECK(scene.placements.front().lods.front().geometryId == "geometry:test");
    CHECK(scene.materials.front().addressU == DahSceneMaterial::AddressMode::ClampToEdge);
    CHECK(scene.materials.front().addressV == DahSceneMaterial::AddressMode::Repeat);
    REQUIRE(scene.regions.size() == 1);
    CHECK(scene.regions.front().position == std::array<double, 3>{1.0, 2.0, 3.0});
    const auto regionReference = buildDahSceneRegionReference(scene);
    CHECK(regionReference.vertexCount() == 36);
    CHECK(regionReference.nativeTriangleCount == 0);
    CHECK(regionReference.placementCount == 0);
    REQUIRE(regionReference.batches.size() == 1);
    CHECK(regionReference.batches.front().alphaMode == "blend");
    CHECK_FALSE(regionReference.batches.front().state.depthWrite);
    REQUIRE(scene.layers.size() == 1);
    CHECK(scene.layers.front().placementCount == 1);
    CHECK(scene.layers.front().scriptCallCount == 2);
    REQUIRE(scene.entityTags.size() == 1);
    CHECK(scene.entityTags.front().entityIds == std::vector<std::int64_t>{42});
    CHECK(
      scene.unresolvedSystems
      == std::vector<std::string>{"collision remains unresolved"});
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
      reference.batches.front().addressU == DahSceneMaterial::AddressMode::ClampToEdge);
    CHECK(reference.batches.front().addressV == DahSceneMaterial::AddressMode::Repeat);
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

  SECTION("v4 multipart visuals compose each component before the placed actor")
  {
    auto directory = QTemporaryDir{};
    REQUIRE(directory.isValid());
    writeTestScene(directory.path(), 4);
    auto json = readTestScene(directory.path());
    auto placement = json["placements"].toArray().first().toObject();
    placement["id"] = "world-placement:test";
    placement["editor_collection"] = "vehicles";
    placement["native"] =
      QJsonObject{{"bucket_hash", "0x12345678"}, {"list_pointer_field", 16}};
    // Row-vector +90 degree Z rotation and translation, deliberately
    // non-commutative with the two component-local translations.
    placement["matrix_row_major_f32"] = QJsonArray{
      0.0, 1.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 10.0, 20.0, 30.0, 1.0};
    auto lod = placement["lods"].toArray().first().toObject();
    auto parts = QJsonArray{};
    for (const auto x : {2.0, -2.0})
    {
      parts.append(QJsonObject{
        {"geometry", "geometry:test"},
        {"material_slots", QJsonArray{"material:test"}},
        {"matrix_row_major_f32", translationMatrix(x, 0.0, 0.0)}});
    }
    lod["parts"] = parts;
    placement["lods"] = QJsonArray{lod};
    json["placements"] = QJsonArray{};
    json["object_placements"] = QJsonArray{placement};
    auto counts = json["counts"].toObject();
    counts["terrain_placements"] = 0;
    counts["terrain_lods"] = 0;
    counts["world_placements"] = 1;
    json["counts"] = counts;
    writeFile(directory.path() + "/scene.json", QJsonDocument{json}.toJson());

    const auto result = loadDahSceneManifest(directory.path().toStdWString());
    REQUIRE(std::holds_alternative<DahSceneManifest>(result));
    const auto& scene = std::get<DahSceneManifest>(result);
    CHECK(scene.schemaVersion == 4);
    REQUIRE(scene.objectPlacements.size() == 1);
    CHECK(scene.objectPlacements.front().nativeLayer == "0x12345678");
    CHECK(scene.objectPlacements.front().nativeList == "+0x10");
    REQUIRE(scene.objectPlacements.front().lods.front().parts.size() == 2);
    const auto referenceResult = buildDahSceneReference(scene, 0);
    REQUIRE(std::holds_alternative<DahSceneReference>(referenceResult));
    const auto& reference = std::get<DahSceneReference>(referenceResult);
    CHECK(reference.placementCount == 1);      // Components are not authored entities.
    CHECK(reference.nativeTriangleCount == 2); // No extra first-mesh duplicate.
    CHECK(reference.vertexCount() == 6);
    CHECK(reference.bounds.min == vm::vec3d{9.0, 18.0, 30.0});
    CHECK(reference.bounds.max == vm::vec3d{10.0, 23.0, 30.0});
    REQUIRE(reference.batches.size() == 1);
    CHECK(reference.batches.front().vertices.front().attr == vm::vec3f{10, 22, 30});

    // Segment state belongs to one geometry; it cannot silently attach to a
    // multipart wrapper and then disappear while the components are expanded.
    auto mixedLod = lod;
    mixedLod["segment_render_states"] = QJsonArray{QJsonObject{
      {"alpha_mode", "opaque"},
      {"alpha_reference_u8", 64},
      {"blend_source", "one"},
      {"blend_destination", "zero"},
      {"blend_equation", "add"},
      {"depth_write", true},
      {"depth_compare", "less_equal"},
      {"vertex_rgb_scale", 2.0},
      {"terrain_layer", 0}}};
    auto mixedPlacement = placement;
    mixedPlacement["lods"] = QJsonArray{mixedLod};
    json["object_placements"] = QJsonArray{mixedPlacement};
    writeFile(directory.path() + "/scene.json", QJsonDocument{json}.toJson());
    CHECK(std::holds_alternative<DahSceneLoadError>(
      loadDahSceneManifest(directory.path().toStdWString())));

    auto malformedPart = parts.first().toObject();
    malformedPart["matrix_row_major_f32"] = QJsonArray{1.0, 0.0};
    parts[0] = malformedPart;
    lod["parts"] = parts;
    placement["lods"] = QJsonArray{lod};
    json["object_placements"] = QJsonArray{placement};
    writeFile(directory.path() + "/scene.json", QJsonDocument{json}.toJson());
    CHECK(std::holds_alternative<DahSceneLoadError>(
      loadDahSceneManifest(directory.path().toStdWString())));
  }

  SECTION("v4 native terrain state preserves vertex alpha and overrides material state")
  {
    auto directory = QTemporaryDir{};
    REQUIRE(directory.isValid());
    writeTestScene(directory.path(), 4);
    auto json = readTestScene(directory.path());
    auto material = json["materials"].toArray().first().toObject();
    material["render_state"] = QJsonObject{
      {"evidence_status", "retail-code-verified"},
      {"alpha_mode", "mask"},
      {"alpha_reference_u8", 128},
      {"alpha_compare", "greater"}};
    json["materials"] = QJsonArray{material};
    auto placement = json["placements"].toArray().first().toObject();
    auto lod = placement["lods"].toArray().first().toObject();
    auto state = QJsonObject{
      {"alpha_mode", "blend"},
      {"alpha_reference_u8", 64},
      {"blend_source", "src_alpha"},
      {"blend_destination", "one_minus_src_alpha"},
      {"blend_equation", "reverse_subtract"},
      {"depth_write", false},
      {"depth_compare", "less_equal"},
      {"vertex_rgb_scale", 2.0},
      {"terrain_layer", 1}};
    lod["segment_render_states"] = QJsonArray{state};
    placement["lods"] = QJsonArray{lod};
    json["placements"] = QJsonArray{placement};
    writeFile(directory.path() + "/scene.json", QJsonDocument{json}.toJson());
    const auto result = loadDahSceneManifest(directory.path().toStdWString());
    REQUIRE(std::holds_alternative<DahSceneManifest>(result));
    const auto& scene = std::get<DahSceneManifest>(result);
    CHECK(scene.materials.front().alphaMode == "mask");
    CHECK(scene.materials.front().alphaReference == 128.0f / 255.0f);
    const auto referenceResult = buildDahSceneReference(scene, 0);
    REQUIRE(std::holds_alternative<DahSceneReference>(referenceResult));
    const auto& reference = std::get<DahSceneReference>(referenceResult);
    REQUIRE(reference.batches.size() == 1);
    const auto& batch = reference.batches.front();
    CHECK(batch.state.nativeTerrain);
    CHECK(batch.alphaMode == "blend");
    CHECK(batch.alphaReference == 64.0f / 255.0f);
    CHECK(batch.state.blendSource == "src_alpha");
    CHECK(batch.state.blendDestination == "one_minus_src_alpha");
    CHECK(batch.state.blendEquation == "reverse_subtract");
    CHECK_FALSE(batch.state.depthWrite);
    CHECK(batch.state.terrainLayer == 1);
    CHECK(batch.state.rgbScale == 2.0f);
    const auto& color = batch.vertices.front().rest.rest.rest.attr;
    CHECK(
      color
      == vm::vec4f{
        64.0f * (1.0f / 255.0f),
        32.0f * (1.0f / 255.0f),
        16.0f * (1.0f / 255.0f),
        128.0f * (1.0f / 255.0f)});
    CHECK(batch.vertices.front().rest.rest.attr == vm::vec2f{0.0f, 0.0f});

    // Same material and terrain layer may still have different native draw
    // state. Each state difference must survive batching independently.
    for (auto field = 0; field < 6; ++field)
    {
      auto distinct = scene;
      auto second = distinct.placements.front();
      second.id = "terrain-placement:state-variant";
      auto& variant = second.lods.front().segmentStates.front();
      switch (field)
      {
      case 0:
        variant.alphaReference = 32.0f / 255.0f;
        break;
      case 1:
        variant.blendSource = "one";
        break;
      case 2:
        variant.blendDestination = "one";
        break;
      case 3:
        variant.depthWrite = true;
        break;
      case 4:
        variant.rgbScale = 1.0f;
        break;
      case 5:
        variant.nativeTerrain = false;
        break;
      }
      distinct.placements.push_back(std::move(second));
      const auto separate = buildDahSceneReference(distinct, 0);
      REQUIRE(std::holds_alternative<DahSceneReference>(separate));
      CAPTURE(field);
      CHECK(std::get<DahSceneReference>(separate).batches.size() == 2);
    }

    state["alpha_reference_u8"] = 256;
    lod["segment_render_states"] = QJsonArray{state};
    placement["lods"] = QJsonArray{lod};
    json["placements"] = QJsonArray{placement};
    writeFile(directory.path() + "/scene.json", QJsonDocument{json}.toJson());
    CHECK(std::holds_alternative<DahSceneLoadError>(
      loadDahSceneManifest(directory.path().toStdWString())));
  }

  SECTION("verified actor raster state does not leak into terrain or scatter")
  {
    auto directory = QTemporaryDir{};
    REQUIRE(directory.isValid());
    auto json = QJsonObject{};
    auto material = QJsonObject{};
    auto state = QJsonObject{
      {"evidence_status", "retail-code-verified"},
      {"alpha_mode", "blend"},
      {"alpha_reference_u8", 1},
      {"alpha_compare", "greater"},
      {"alpha_test_enabled", true},
      {"blend_source", "one"},
      {"blend_destination", "one"},
      {"blend_equation", "add"},
      {"depth_write", false},
      {"depth_compare", "less_equal"},
      {"material_opacity", 0.7},
      {"native_pixel_shader_index", 1}};
    for (const auto& kind :
         {"terrain", "static-scatter", "human-reference-pose", "static-world-object"})
    {
      const auto human = std::string{kind} == "human-reference-pose";
      const auto rigid = std::string{kind} == "static-world-object";
      writeTestScene(
        directory.path(),
        4,
        false,
        "buffers.bin",
        human   ? 40
        : rigid ? 28
                : 24);
      json = readTestScene(directory.path());
      material = json["materials"].toArray().first().toObject();
      material["actor_render_state"] = state;
      json["materials"] = QJsonArray{material};
      auto geometry = json["geometries"].toArray().first().toObject();
      geometry["kind"] = kind;
      auto layout = geometry["vertex_layout"].toObject();
      if (human || rigid)
        layout["normal"] = QJsonObject{
          {"offset", human ? 12 : 20},
          {"format", human ? "float32x3-little-endian" : "snorm16x4-little-endian"}};
      geometry["vertex_layout"] = layout;
      json["geometries"] = QJsonArray{geometry};
      auto counts = json["counts"].toObject();
      counts["human_geometries"] = human ? 1 : 0;
      counts["scatter_geometries"] = std::string{kind} == "static-scatter" ? 1 : 0;
      json["counts"] = counts;
      writeFile(directory.path() + "/scene.json", QJsonDocument{json}.toJson());
      auto result = loadDahSceneManifest(directory.path().toStdWString());
      CAPTURE(kind);
      if (auto* error = std::get_if<DahSceneLoadError>(&result))
        FAIL(error->message);
      auto scene = std::get<DahSceneManifest>(result);
      REQUIRE(scene.materials.front().actorState);
      const auto referenceResult = buildDahSceneReference(scene, 0);
      REQUIRE(std::holds_alternative<DahSceneReference>(referenceResult));
      const auto& batch = std::get<DahSceneReference>(referenceResult).batches.front();
      CAPTURE(kind);
      if (
        scene.geometries.front().kind == "human-reference-pose"
        || scene.geometries.front().kind == "static-world-object")
      {
        CHECK(batch.alphaMode == "blend");
        CHECK(batch.state.blendSource == "one");
        CHECK(batch.state.blendDestination == "one");
        CHECK(batch.state.alphaTest);
        CHECK_FALSE(batch.state.depthWrite);
        CHECK(batch.alphaReference == 1.0f / 255.0f);
        CHECK(batch.state.rgbScale == 1.0f);
        CHECK(
          batch.vertices.front().rest.rest.rest.attr.w()
          == (scene.geometries.front().kind == "static-world-object" ? 0.7f : 1.0f));
      }
      else
      {
        CHECK(batch.alphaMode == "opaque");
        CHECK_FALSE(batch.state.alphaTest);
        CHECK(batch.state.depthWrite);
      }
    }
    state["alpha_reference_u8"] = 256;
    material["actor_render_state"] = state;
    json["materials"] = QJsonArray{material};
    writeFile(directory.path() + "/scene.json", QJsonDocument{json}.toJson());
    CHECK(std::holds_alternative<DahSceneLoadError>(
      loadDahSceneManifest(directory.path().toStdWString())));
  }

  SECTION("v3 landing annotations are separate from native triangles")
  {
    auto directory = QTemporaryDir{};
    REQUIRE(directory.isValid());
    writeTestScene(directory.path(), 3);
    auto file = QFile{directory.path() + "/scene.json"};
    REQUIRE(file.open(QIODevice::ReadOnly));
    auto json = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    auto counts = json["counts"].toObject();
    counts["scene_markers"] = 1;
    counts["landing_sites"] = 1;
    json["counts"] = counts;
    json["scene_markers"] = QJsonArray{QJsonObject{
      {"id", "landing:test"},
      {"kind", "landing-site"},
      {"label", "test site"},
      {"status", "authored; runtime activation conditional"},
      {"position_f32", QJsonArray{10.0, 20.0, 30.0}}}};
    writeFile(directory.path() + "/scene.json", QJsonDocument{json}.toJson());
    const auto result = loadDahSceneManifest(directory.path().toStdWString());
    REQUIRE(std::holds_alternative<DahSceneManifest>(result));
    const auto& scene = std::get<DahSceneManifest>(result);
    REQUIRE(scene.markers.size() == 1);
    CHECK(scene.markers.front().position == std::array<double, 3>{10.0, 20.0, 30.0});
    const auto markers = buildDahSceneMarkerReference(scene);
    CHECK(markers.vertexCount() == 24);
    CHECK(markers.nativeTriangleCount == 0);
    CHECK(markers.placementCount == 0);
    counts["landing_sites"] = 0;
    json["counts"] = counts;
    writeFile(directory.path() + "/scene.json", QJsonDocument{json}.toJson());
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
      CHECK(scene.schemaVersion == 4);
      CHECK(scene.profileId == "dah1-retail-xbox-us");
      CHECK(scene.mapName == "area42");
      CHECK(scene.counts.terrainPlacements == 92);
      CHECK(scene.counts.terrainLods == 184);
      CHECK(scene.counts.worldPlacements == 2674);
      CHECK(scene.counts.buildingPlacements == 79);
      CHECK(scene.counts.buildingRenderablePlacements == 79);
      CHECK(scene.counts.propPlacements == 1468);
      CHECK(scene.counts.propRenderablePlacements == 1468);
      CHECK(scene.counts.renderableStructurePlacements == 1547);
      CHECK(scene.counts.geometries == 641);
      CHECK(scene.geometries.size() == 641);
      CHECK(scene.materials.size() == 792);
      CHECK(scene.textures.size() == 348);
      CHECK(scene.counts.humanPlacements == 295);
      CHECK(scene.counts.humanRenderablePlacements == 295);
      CHECK(scene.counts.scatterPlacements == 15121);
      CHECK(scene.counts.scatterBatches == 654);
      CHECK(scene.counts.landingSites == 3);
      CHECK(scene.counts.spawnDefinitions == 7);
      CHECK(scene.markers.size() == 148);
      CHECK(scene.objectives.size() == 146);
      REQUIRE(scene.skyPlacements.size() == 1);
      CHECK(scene.suns.size() == 2);
      for (const auto& texture : scene.textures)
        CHECK_FALSE(texture.nativeMips.empty());
      CHECK(scene.placements.size() == 92);
      CHECK(scene.objectPlacements.size() == 2674);
      REQUIRE(scene.landingPlacements.size() == 3);
      CHECK(scene.landingPlacements.front().lods.front().parts.size() == 11);
      CHECK(scene.objectPlacements.front().nativeLayer == "0x8F651465");
      CHECK(scene.objectPlacements.front().nativeList == "+0x0C");
      REQUIRE(scene.objectPlacements.front().entityId);
      CHECK(*scene.objectPlacements.front().entityId == 39662);
      CHECK(scene.regions.size() == 92);
      CHECK(scene.layers.size() == 41);
      CHECK(scene.entityTags.size() == 135);
      CHECK(scene.unresolvedSystems.size() == 9);
      CHECK(scene.objectPlacements.front().lods.front().parts.size() == 13);

      const auto referenceResult = buildDahSceneReference(scene, 0);
      REQUIRE(std::holds_alternative<DahSceneReference>(referenceResult));
      const auto& reference = std::get<DahSceneReference>(referenceResult);
      CHECK(reference.placementCount == 17751);
      CHECK(reference.nativeTriangleCount == 1790050);
      CHECK(reference.vertexCount() == 5370534);
      CHECK(reference.skyTriangleCount == 128);
      CHECK_FALSE(reference.batches.empty());
      CHECK(reference.texturedMaterialCount <= reference.batches.size());

      const auto lod1Result = buildDahSceneReference(scene, 1);
      REQUIRE(std::holds_alternative<DahSceneReference>(lod1Result));
      const auto& lod1Reference = std::get<DahSceneReference>(lod1Result);
      CHECK(lod1Reference.placementCount == 17751);
      CHECK(lod1Reference.nativeTriangleCount == 932771);
      CHECK(lod1Reference.vertexCount() == 2798697);
      CHECK(lod1Reference.skyTriangleCount == 128);
      CHECK_FALSE(lod1Reference.batches.empty());
      CHECK(lod1Reference.texturedMaterialCount <= lod1Reference.batches.size());
    }
  }
}

TEST_CASE(
  "DahSceneReference sorts sky, native terrain layers, then transparent instances")
{
  const auto batches = std::vector<DahSceneBatchSortInfo>{
    {{0, 0, 10}, false, true, false, 0},
    {{0, 0, 100}, false, true, false, 0},
    {{}, false, true, true, 1},
    {{}, false, false, false, 0},
    {{}, true, true, false, 0},
    {{}, false, true, true, -1},
    {{}, false, false, true, 0}};
  CHECK(
    dahSceneDrawOrder(batches, {}, {0, 0, 1})
    == std::vector<size_t>{4, 6, 3, 5, 2, 1, 0});
  CHECK(
    dahSceneDrawOrder(batches, {}, {0, 0, -1})
    == std::vector<size_t>{4, 6, 3, 5, 2, 0, 1});
}

TEST_CASE("DahSceneManifest validates native mip sidecars")
{
  auto directory = QTemporaryDir{};
  REQUIRE(directory.isValid());
  writeTestScene(directory.path());
  auto json = readTestScene(directory.path());
  auto textures = json["textures"].toArray();
  auto texture = textures[0].toObject();
  auto preview = texture["preview"].toObject();
  const auto second = QByteArray{"second native mip"};
  writeFile(directory.path() + "/mip1.png", second);
  auto mips = QJsonArray{
    QJsonObject{
      {"level", 0},
      {"width", 2},
      {"height", 2},
      {"file", "preview.png"},
      {"sha256", preview["sha256"]}},
    QJsonObject{
      {"level", 1},
      {"width", 1},
      {"height", 1},
      {"file", "mip1.png"},
      {"sha256", hash(second)}}};
  SECTION("valid chain remains full resolution") {}
  SECTION("rejects wrong dimensions")
  {
    auto m = mips[1].toObject();
    m["width"] = 2;
    mips[1] = m;
  }
  SECTION("rejects reordered chain")
  {
    auto m = mips[1].toObject();
    m["level"] = 0;
    mips[1] = m;
  }
  SECTION("rejects escaping sidecar")
  {
    auto m = mips[1].toObject();
    m["file"] = "../mip1.png";
    mips[1] = m;
  }
  SECTION("rejects changed sidecar")
  {
    writeFile(directory.path() + "/mip1.png", "bad");
  }
  preview["native_mips"] = mips;
  texture["preview"] = preview;
  texture["native"] = QJsonObject{{"width", 2}, {"height", 2}, {"mip_levels", 2}};
  textures[0] = texture;
  json["textures"] = textures;
  writeFile(directory.path() + "/scene.json", QJsonDocument{json}.toJson());
  const auto result = loadDahSceneManifest(directory.path().toStdWString());
  if (
    mips[1].toObject()["width"].toInt() == 1 && mips[1].toObject()["level"].toInt() == 1
    && mips[1].toObject()["file"] == "mip1.png")
  {
    auto file = QFile{directory.path() + "/mip1.png"};
    REQUIRE(file.open(QIODevice::ReadOnly));
    if (file.readAll() == second)
    {
      REQUIRE(std::holds_alternative<DahSceneManifest>(result));
      CHECK(std::get<DahSceneManifest>(result).textures.front().nativeMips.size() == 2);
      return;
    }
  }
  CHECK(std::holds_alternative<DahSceneLoadError>(result));
}

TEST_CASE("DahSceneReference compiles the native GLSL program on a real GL context")
{
  const auto root = qEnvironmentVariable("CRYPTOEDITOR_SHADER_DIR");
  if (root.isEmpty())
    return; // Optional driver-backed integration test.
  auto format = QSurfaceFormat{};
  format.setVersion(2, 1);
  format.setProfile(QSurfaceFormat::CompatibilityProfile);
  auto context = QOpenGLContext{};
  context.setFormat(format);
  REQUIRE(context.create());
  auto surface = QOffscreenSurface{};
  surface.setFormat(context.format());
  surface.create();
  REQUIRE(surface.isValid());
  REQUIRE(context.makeCurrent(&surface));
  auto program = QOpenGLShaderProgram{};
  const auto vertex =
    program.addShaderFromSourceFile(QOpenGLShader::Vertex, root + "/DahReference.vertsh");
  INFO(program.log().toStdString());
  REQUIRE(vertex);
  for (const auto* name :
       {"AlphaFunc.fragsh",
        "GridCommon.fragsh",
        "Grid.fragsh",
        "MapBounds.fragsh",
        "DahReference.fragsh"})
  {
    const auto compiled =
      program.addShaderFromSourceFile(QOpenGLShader::Fragment, root + "/" + name);
    INFO(program.log().toStdString());
    REQUIRE(compiled);
  }
  const auto linked = program.link();
  INFO(program.log().toStdString());
  REQUIRE(linked);
  CHECK(program.uniformLocation("NativeRgbScale") >= 0);
  CHECK(program.uniformLocation("CameraRelativeSky") >= 0);
  CHECK(program.uniformLocation("SunDirection") >= 0);
  CHECK(program.uniformLocation("UvRow0") >= 0);
  auto* functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(&context);
  REQUIRE(functions != nullptr);
  REQUIRE(functions->initializeOpenGLFunctions());
  auto framebuffer = QOpenGLFramebufferObject{32, 32};
  REQUIRE(framebuffer.isValid());
  REQUIRE(framebuffer.bind());
  REQUIRE(program.bind());
  functions->glViewport(0, 0, 32, 32);
  functions->glDisable(GL_DEPTH_TEST);
  functions->glDisable(GL_BLEND);
  functions->glDisable(GL_DITHER);
  functions->glMatrixMode(GL_PROJECTION);
  functions->glLoadIdentity();
  functions->glMatrixMode(GL_MODELVIEW);
  functions->glLoadIdentity();
  GLuint texture = 0;
  functions->glGenTextures(1, &texture);
  functions->glBindTexture(GL_TEXTURE_2D, texture);
  const std::array<unsigned char, 4> texel{128, 64, 32, 128};
  functions->glTexImage2D(
    GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel.data());
  functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  program.setUniformValue("Material", 0);
  program.setUniformValue("ApplyMaterial", true);
  program.setUniformValue("Color", QVector4D{1, 1, 1, 1});
  program.setUniformValue("UvRow0", QVector4D{1, 0, 0, 0});
  program.setUniformValue("UvRow1", QVector4D{0, 1, 0, 0});
  program.setUniformValue("NativeRgbScale", 2.0f);
  program.setUniformValue("LightingMode", 0);
  program.setUniformValue("RenderGrid", false);
  program.setUniformValue("ShowSoftMapBounds", false);
  program.setUniformValue("CameraRelativeSky", false);
  program.setUniformValue("EnableMasked", true);
  program.setUniformValue("AlphaFuncCompare", 2);
  const auto drawPixel = [&]() {
    functions->glClearColor(0, 0, 0, 0);
    functions->glClear(GL_COLOR_BUFFER_BIT);
    functions->glBegin(GL_TRIANGLES);
    functions->glColor4f(0.75f, 0.5f, 0.25f, 0.5f);
    functions->glTexCoord2f(0.5f, 0.5f);
    functions->glNormal3f(0, 0, 1);
    functions->glVertex3f(-1, -1, 0);
    functions->glVertex3f(3, -1, 0);
    functions->glVertex3f(-1, 3, 0);
    functions->glEnd();
    auto pixel = std::array<unsigned char, 4>{};
    functions->glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    return pixel;
  };
  program.setUniformValue("AlphaFuncThreshold", 0.2f);
  const auto sampled = drawPixel();
  for (size_t i = 0; i < 4; ++i)
  {
    const auto expected = std::array<int, 4>{192, 64, 16, 64}[i];
    CHECK(std::abs(static_cast<int>(sampled[i]) - expected) <= 1);
  }
  program.setUniformValue("AlphaFuncThreshold", 0.3f);
  CHECK(drawPixel() == std::array<unsigned char, 4>{0, 0, 0, 0});
  program.setUniformValue("AlphaFuncThreshold", 0.2f);
  program.setUniformValue("LightingMode", 2);
  program.setUniformValue("MaterialAdditive", QVector3D{0.25f, 0.5f, 0.75f});
  functions->glTranslatef(10, 0, 0);
  CHECK(drawPixel() == std::array<unsigned char, 4>{0, 0, 0, 0});
  program.setUniformValue("CameraRelativeSky", true);
  const auto sky = drawPixel();
  for (size_t i = 0; i < 4; ++i)
    CHECK(
      std::abs(static_cast<int>(sky[i]) - std::array<int, 4>{64, 64, 48, 64}[i]) <= 1);
  functions->glLoadIdentity();
  program.setUniformValue("CameraRelativeSky", false);
  program.setUniformValue("LightingMode", 1);
  program.setUniformValue("SunDirection", QVector3D{0, 0, 0.5f});
  program.setUniformValue("SunDiffuse", QVector3D{0.5f, 0.4f, 0.3f});
  program.setUniformValue("SunAmbient", QVector3D{0.1f, 0.2f, 0.3f});
  program.setUniformValue("MaterialDiffuse", QVector3D{0.5f, 1, 0.25f});
  program.setUniformValue("MaterialAdditive", QVector3D{0.1f, 0.05f, 0.2f});
  const auto lit = drawPixel();
  for (size_t i = 0; i < 4; ++i)
    CHECK(
      std::abs(static_cast<int>(lit[i]) - std::array<int, 4>{70, 58, 20, 64}[i]) <= 1);
  functions->glDeleteTextures(1, &texture);
  program.release();
  framebuffer.release();
}

} // namespace tb::ui
