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

namespace tb::ui
{
namespace
{

using JsonError = std::runtime_error;

QJsonObject objectField(const QJsonObject& object, const char* key)
{
  const auto value = object.value(key);
  if (!value.isObject())
  {
    throw JsonError{std::string{"Expected object field: "} + key};
  }
  return value.toObject();
}

QJsonArray arrayField(const QJsonObject& object, const char* key)
{
  const auto value = object.value(key);
  if (!value.isArray())
  {
    throw JsonError{std::string{"Expected array field: "} + key};
  }
  return value.toArray();
}

std::string stringField(const QJsonObject& object, const char* key)
{
  const auto value = object.value(key);
  if (!value.isString() || value.toString().isEmpty())
  {
    throw JsonError{std::string{"Expected non-empty string field: "} + key};
  }
  return value.toString().toStdString();
}

std::string optionalStringField(const QJsonObject& object, const char* key)
{
  const auto value = object.value(key);
  if (value.isUndefined() || value.isNull())
  {
    return {};
  }
  if (!value.isString())
  {
    throw JsonError{std::string{"Expected string field: "} + key};
  }
  return value.toString().toStdString();
}

template <size_t N>
std::array<float, N> finiteArrayField(const QJsonObject& object, const char* key)
{
  const auto values = arrayField(object, key);
  if (values.size() != N)
    throw JsonError{"Unexpected native numeric array length"};
  auto result = std::array<float, N>{};
  for (size_t i = 0; i < N; ++i)
  {
    const auto v = values[static_cast<qsizetype>(i)];
    if (
      !v.isDouble() || !std::isfinite(v.toDouble())
      || std::abs(v.toDouble()) > std::numeric_limits<float>::max())
      throw JsonError{"Native numeric array must contain finite floats"};
    result[i] = static_cast<float>(v.toDouble());
  }
  return result;
}

DahSceneMaterial::AddressMode addressModeField(const QJsonObject& object, const char* key)
{
  const auto value = stringField(object, key);
  if (value == "wrap")
  {
    return DahSceneMaterial::AddressMode::Repeat;
  }
  if (value == "clamp")
  {
    return DahSceneMaterial::AddressMode::ClampToEdge;
  }
  throw JsonError{std::string{"Unsupported texture address mode: "} + value};
}

std::uint64_t unsignedField(const QJsonObject& object, const char* key)
{
  const auto value = object.value(key);
  if (!value.isDouble())
  {
    throw JsonError{std::string{"Expected unsigned integer field: "} + key};
  }
  const auto number = value.toDouble();
  if (
    number < 0.0 || !std::isfinite(number) || std::floor(number) != number
    || number > static_cast<double>(std::numeric_limits<std::uint64_t>::max()))
  {
    throw JsonError{std::string{"Invalid unsigned integer field: "} + key};
  }
  return static_cast<std::uint64_t>(number);
}

QByteArray readFile(const std::filesystem::path& path)
{
  auto file = QFile{QString::fromStdWString(path.wstring())};
  if (!file.open(QIODevice::ReadOnly))
  {
    throw JsonError{"Could not read " + path.string()};
  }
  return file.readAll();
}

std::string sha256(const QByteArray& bytes)
{
  return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256)
    .toHex()
    .toStdString();
}

std::filesystem::path safeChildPath(
  const std::filesystem::path& root, const std::string& relative)
{
  const auto candidate = std::filesystem::path{relative};
  if (candidate.empty() || candidate.is_absolute())
  {
    throw JsonError{"Scene payload path must be relative"};
  }
  for (const auto& part : candidate)
  {
    if (part == "..")
    {
      throw JsonError{"Scene payload path escapes the scene directory"};
    }
  }
  return (root / candidate).lexically_normal();
}

DahSceneBufferView parseBufferView(
  const QJsonObject& object, const QByteArray& buffer, const std::string& label)
{
  auto view = DahSceneBufferView{
    unsignedField(object, "offset"),
    unsignedField(object, "length"),
    stringField(object, "sha256")};
  const auto bufferSize = static_cast<std::uint64_t>(buffer.size());
  if (view.offset > bufferSize || view.length > bufferSize - view.offset)
  {
    throw JsonError{label + " buffer view is out of range"};
  }
  const auto bytes =
    buffer.mid(static_cast<qsizetype>(view.offset), static_cast<qsizetype>(view.length));
  if (sha256(bytes) != view.sha256)
  {
    throw JsonError{label + " buffer view SHA-256 mismatch"};
  }
  return view;
}

DahSceneCounts parseCounts(const QJsonObject& object)
{
  return {
    unsignedField(object, "terrain_placements"),
    unsignedField(object, "terrain_lods"),
    unsignedField(object, "terrain_geometries"),
    unsignedField(object, "world_buckets"),
    unsignedField(object, "world_placements"),
    unsignedField(object, "building_placements"),
    unsignedField(object, "building_renderable_placements"),
    unsignedField(object, "building_geometries"),
    unsignedField(object, "prop_placements"),
    unsignedField(object, "prop_renderable_placements"),
    unsignedField(object, "prop_geometries"),
    unsignedField(object, "renderable_structure_placements"),
    unsignedField(object, "geometries"),
    unsignedField(object, "materials"),
    unsignedField(object, "textures"),
    unsignedField(object, "lod0_vertices"),
    unsignedField(object, "lod0_triangles"),
    unsignedField(object, "lod1_vertices"),
    unsignedField(object, "lod1_triangles")};
}

void parseExpansionCounts(const QJsonObject& object, DahSceneCounts& counts)
{
  counts.scatterBatches = unsignedField(object, "scatter_batches");
  counts.scatterPlacements = unsignedField(object, "scatter_placements");
  counts.scatterGeometries = unsignedField(object, "scatter_geometries");
  counts.humanPlacements = unsignedField(object, "human_placements");
  counts.humanRenderablePlacements = unsignedField(object, "human_renderable_placements");
  counts.humanGeometries = unsignedField(object, "human_geometries");
  counts.sceneMarkers = unsignedField(object, "scene_markers");
  counts.landingSites = unsignedField(object, "landing_sites");
  counts.spawnDefinitions = unsignedField(object, "spawn_definitions");
}

void validateArea42Counts(const DahSceneManifest& manifest)
{
  if (manifest.profileId != "dah1-retail-xbox-us" || manifest.mapName != "area42")
  {
    return;
  }
  const auto& counts = manifest.counts;
  if (manifest.schemaVersion >= 4)
  {
    if (
      counts.terrainPlacements != 92 || counts.terrainLods != 184
      || counts.terrainGeometries != 184 || counts.worldBuckets != 41
      || counts.worldPlacements != 2674 || counts.buildingPlacements != 79
      || counts.buildingRenderablePlacements != 79 || counts.propPlacements != 1468
      || counts.propRenderablePlacements != 1468
      || counts.renderableStructurePlacements != 1547 || counts.lod0Vertices != 122956
      || counts.lod0Triangles != 103634 || counts.lod1Vertices != 16240
      || counts.lod1Triangles != 8120)
      throw JsonError{"Area 42 v4 scene has unexpected native counts"};
    return;
  }
  if (
    counts.terrainPlacements != 92 || counts.terrainLods != 184
    || counts.terrainGeometries != 184 || counts.worldBuckets != 41
    || counts.worldPlacements != 849 || counts.buildingPlacements != 18
    || counts.buildingRenderablePlacements != 18 || counts.buildingGeometries != 8
    || counts.propPlacements != 23 || counts.propRenderablePlacements != 23
    || counts.propGeometries != 11 || counts.renderableStructurePlacements != 41
    || counts.geometries != 203 + counts.scatterGeometries + counts.humanGeometries
    || counts.lod0Vertices != 122956 || counts.lod0Triangles != 103634
    || counts.lod1Vertices != 16240 || counts.lod1Triangles != 8120)
  {
    throw JsonError{"Area 42 scene has unexpected native geometry counts"};
  }
}

} // namespace

DahSceneLoadResult loadDahSceneManifest(const std::filesystem::path& sceneDirectory)
{
  try
  {
    const auto root = std::filesystem::absolute(sceneDirectory).lexically_normal();
    const auto document = QJsonDocument::fromJson(readFile(root / "scene.json"));
    if (!document.isObject())
    {
      throw JsonError{"Scene manifest root must be an object"};
    }
    const auto json = document.object();
    const auto schema = objectField(json, "schema");
    const auto version = unsignedField(schema, "version");
    if (
      stringField(schema, "name") != "dah-editor-scene"
      || (version != 2 && version != 3 && version != 4))
    {
      throw JsonError{"Unsupported DAH editor scene schema"};
    }
    if (json.value("read_only") != true)
    {
      throw JsonError{"DAH editor scene must be explicitly read-only"};
    }

    auto result = DahSceneManifest{};
    result.schemaVersion = version;
    result.sceneDirectory = root;
    result.profileId = stringField(json, "profile_id");
    result.mapName = stringField(json, "map_name");
    result.counts = parseCounts(objectField(json, "counts"));
    if (version >= 3)
    {
      parseExpansionCounts(objectField(json, "counts"), result.counts);
    }

    const auto buffers = objectField(json, "buffers");
    result.bufferPath = safeChildPath(root, stringField(buffers, "file"));
    result.bufferByteLength = unsignedField(buffers, "byte_length");
    result.bufferSha256 = stringField(buffers, "sha256");
    const auto buffer = readFile(result.bufferPath);
    if (static_cast<std::uint64_t>(buffer.size()) != result.bufferByteLength)
    {
      throw JsonError{"Scene buffer length mismatch"};
    }
    if (sha256(buffer) != result.bufferSha256)
    {
      throw JsonError{"Scene buffer SHA-256 mismatch"};
    }

    auto geometryIds = std::unordered_set<std::string>{};
    for (const auto& value : arrayField(json, "geometries"))
    {
      if (!value.isObject())
      {
        throw JsonError{"Scene geometry entry must be an object"};
      }
      const auto geometryJson = value.toObject();
      auto geometry = DahSceneGeometry{};
      geometry.id = stringField(geometryJson, "id");
      geometry.kind = stringField(geometryJson, "kind");
      if (!geometryIds.insert(geometry.id).second)
      {
        throw JsonError{"Duplicate scene geometry id: " + geometry.id};
      }
      const auto geometryCounts = objectField(geometryJson, "counts");
      geometry.vertexCount = unsignedField(geometryCounts, "vertices");
      geometry.triangleCount = unsignedField(geometryCounts, "triangles");
      const auto vertexLayout = objectField(geometryJson, "vertex_layout");
      geometry.vertexStride = unsignedField(vertexLayout, "stride");
      const auto positionLayout = objectField(vertexLayout, "position");
      const auto uvLayout = objectField(vertexLayout, "uv");
      geometry.positionOffset = unsignedField(positionLayout, "offset");
      geometry.uvOffset = unsignedField(uvLayout, "offset");
      if ((geometry.kind != "terrain" &&
           geometry.kind != "static-world-object" &&
           geometry.kind != "static-scatter" &&
           geometry.kind != "human-reference-pose") ||
          (geometry.kind == "terrain" && geometry.vertexStride != 24) ||
          (geometry.kind == "static-world-object" &&
           geometry.vertexStride != 28) ||
          (geometry.kind == "static-scatter" && geometry.vertexStride != 24) ||
          (geometry.kind == "human-reference-pose" && geometry.vertexStride != 40) ||
          geometry.positionOffset + 12 > geometry.vertexStride ||
          geometry.uvOffset + 8 > geometry.vertexStride ||
          stringField(positionLayout, "format") != "float32x3-little-endian" ||
          stringField(positionLayout, "status") != "verified" ||
          stringField(uvLayout, "format") != "float32x2-little-endian" ||
          (stringField(uvLayout, "status") != "candidate" &&
           stringField(uvLayout, "status") != "verified"))
      {
        throw JsonError{"Unsupported DAH native vertex layout"};
      }
      if (
        geometry.kind == "static-world-object" || geometry.kind == "human-reference-pose")
      {
        const auto normal = objectField(vertexLayout, "normal");
        const auto human = geometry.kind == "human-reference-pose";
        if (
          unsignedField(normal, "offset") != (human ? 12u : 20u)
          || stringField(normal, "format")
               != (human ? "float32x3-little-endian" : "snorm16x4-little-endian"))
        {
          throw JsonError{"Unsupported DAH native normal layout"};
        }
      }
      const auto views = objectField(geometryJson, "buffers");
      geometry.vertices = parseBufferView(
        objectField(views, "vertices"), buffer, geometry.id + " vertices");
      geometry.indexStrip = parseBufferView(
        objectField(views, "index_strip"), buffer, geometry.id + " index strip");
      geometry.triangles = parseBufferView(
        objectField(views, "triangles"), buffer, geometry.id + " triangles");
      geometry.segments = parseBufferView(
        objectField(views, "segments"), buffer, geometry.id + " segments");
      for (auto it = views.begin(); it != views.end(); ++it)
      {
        if (!it.value().isObject())
        {
          throw JsonError{"Geometry buffer view must be an object"};
        }
        parseBufferView(
          it.value().toObject(), buffer, geometry.id + " " + it.key().toStdString());
      }
      auto segmentTriangleCount = std::uint64_t{0};
      for (const auto& segmentValue : arrayField(geometryJson, "segments"))
      {
        if (!segmentValue.isObject())
        {
          throw JsonError{"Scene geometry segment must be an object"};
        }
        const auto segmentJson = segmentValue.toObject();
        auto segment = DahSceneGeometry::Segment{
          unsignedField(segmentJson, "triangle_count"),
          unsignedField(segmentJson, "material_slot")};
        segmentTriangleCount += segment.triangleCount;
        geometry.segmentRecords.push_back(segment);
      }
      if (segmentTriangleCount != geometry.triangleCount)
      {
        throw JsonError{geometry.id + " segment triangle counts do not match geometry"};
      }
      result.geometries.push_back(std::move(geometry));
    }

    auto materialIds = std::unordered_set<std::string>{};
    for (const auto& value : arrayField(json, "materials"))
    {
      if (!value.isObject())
      {
        throw JsonError{"Scene material entry must be an object"};
      }
      const auto materialJson = value.toObject();
      auto material = DahSceneMaterial{};
      material.id = stringField(materialJson, "id");
      if (!materialIds.insert(material.id).second)
      {
        throw JsonError{"Duplicate scene material id: " + material.id};
      }
      const auto binding = materialJson.value("binding");
      if (binding.isObject())
      {
        material.textureId = stringField(binding.toObject(), "texture");
      }
      else if (!binding.isNull())
      {
        throw JsonError{"Scene material binding must be an object or null"};
      }
      const auto sampler = materialJson.value("sampler");
      if (sampler.isObject())
      {
        const auto samplerObject = sampler.toObject();
        if (stringField(samplerObject, "status") != "verified")
        {
          throw JsonError{"Scene material sampler must be verified"};
        }
        material.addressU = addressModeField(samplerObject, "address_u");
        material.addressV = addressModeField(samplerObject, "address_v");
      }
      else if (!sampler.isUndefined() && !sampler.isNull())
      {
        throw JsonError{"Scene material sampler must be an object"};
      }
      const auto state = materialJson.value("render_state");
      if (
        state.isObject()
        && optionalStringField(state.toObject(), "evidence_status")
             == "retail-code-verified")
      {
        const auto stateObject = state.toObject();
        material.alphaMode = stringField(stateObject, "alpha_mode");
        const auto reference = unsignedField(stateObject, "alpha_reference_u8");
        if (
          (material.alphaMode != "opaque" && material.alphaMode != "mask"
           && material.alphaMode != "blend")
          || reference > 255 || stringField(stateObject, "alpha_compare") != "greater")
        {
          throw JsonError{"Unsupported native material alpha state"};
        }
        material.alphaReference = static_cast<float>(reference) / 255.0f;
      }
      const auto actorState = materialJson.value("actor_render_state");
      if (!actorState.isUndefined() && !actorState.isNull())
      {
        if (!actorState.isObject())
          throw JsonError{"Actor render state must be an object"};
        const auto s = actorState.toObject();
        if (optionalStringField(s, "evidence_status") == "retail-code-verified")
        {
          auto parsed = DahSceneRenderState{};
          parsed.alphaMode = stringField(s, "alpha_mode");
          const auto alpha = unsignedField(s, "alpha_reference_u8");
          parsed.alphaReference = static_cast<float>(alpha) / 255.0f;
          parsed.blendSource = stringField(s, "blend_source");
          parsed.blendDestination = stringField(s, "blend_destination");
          parsed.blendEquation = stringField(s, "blend_equation");
          if (!s.value("depth_write").isBool() || !s.value("alpha_test_enabled").isBool()
              || alpha > 255
              || (parsed.alphaMode != "opaque" && parsed.alphaMode != "mask" && parsed.alphaMode != "blend")
              || (parsed.blendSource != "one" && parsed.blendSource != "src_alpha")
              || (parsed.blendDestination != "zero" && parsed.blendDestination != "one" && parsed.blendDestination != "one_minus_src_alpha")
              || (parsed.blendEquation != "add" && parsed.blendEquation != "subtract" && parsed.blendEquation != "reverse_subtract")
              || stringField(s, "alpha_compare") != "greater"
              || stringField(s, "depth_compare") != "less_equal")
            throw JsonError{"Unsupported native actor render state"};
          parsed.depthWrite = s.value("depth_write").toBool();
          parsed.alphaTest = s.value("alpha_test_enabled").toBool();
          if (s.contains("material_opacity"))
          {
            const auto opacity = s.value("material_opacity");
            if (!opacity.isDouble() || !std::isfinite(opacity.toDouble()))
              throw JsonError{"Native material opacity must be finite"};
            const auto shader = unsignedField(s, "native_pixel_shader_index");
            if (shader == 0 || shader == 1)
              parsed.rigidOpacity =
                static_cast<float>(std::clamp(opacity.toDouble(), 0.0, 1.0));
          }
          material.actorState = std::move(parsed);
        }
      }
      if (materialJson.contains("actor_material"))
      {
        const auto a = objectField(materialJson, "actor_material");
        if (
          optionalStringField(a, "uv_transform_status")
          == "retail-initial-material-load-preserved")
        {
          material.actorUvTransform = finiteArrayField<8>(a, "effective_uv_transform");
          material.diffuse = finiteArrayField<3>(a, "native_diffuse_color");
          material.additive = finiteArrayField<3>(a, "native_additive_color");
          const auto pixelShader = unsignedField(a, "native_pixel_shader_index");
          if (pixelShader > 255)
            throw JsonError{"Invalid native pixel shader index"};
          material.pixelShader = static_cast<int>(pixelShader);
          material.actorParametersVerified = true;
        }
      }
      if (materialJson.contains("native_sampler"))
      {
        const auto s = objectField(materialJson, "native_sampler");
        if (optionalStringField(s, "evidence_status") == "retail-code-verified")
        {
          const auto mag = stringField(s, "mag_filter");
          const auto mip = stringField(s, "mip_filter");
          const auto bias = s.value("lod_bias");
          if (
            stringField(s, "min_filter") != "linear"
            || (mag != "linear" && mag != "nearest") || (mip != "linear" && mip != "none")
            || !bias.isDouble() || !std::isfinite(bias.toDouble())
            || std::abs(bias.toDouble()) > 100.0 || !s.value("generate_mipmaps").isBool()
            || s.value("generate_mipmaps").toBool())
            throw JsonError{"Unsupported native sampler state"};
          material.nativeSampler = true;
          material.nearestMagnification = mag == "nearest";
          material.mipFilter = mip == "linear";
          material.lodBias = static_cast<float>(bias.toDouble());
        }
      }
      result.materials.push_back(std::move(material));
    }

    auto textureIds = std::unordered_set<std::string>{};
    for (const auto& value : arrayField(json, "textures"))
    {
      if (!value.isObject())
      {
        throw JsonError{"Scene texture entry must be an object"};
      }
      const auto textureJson = value.toObject();
      auto texture = DahSceneTexture{};
      texture.id = stringField(textureJson, "id");
      if (!textureIds.insert(texture.id).second)
      {
        throw JsonError{"Duplicate scene texture id: " + texture.id};
      }
      const auto preview = objectField(textureJson, "preview");
      texture.previewPath = safeChildPath(root, stringField(preview, "file"));
      texture.previewSha256 = stringField(preview, "sha256");
      if (sha256(readFile(texture.previewPath)) != texture.previewSha256)
      {
        throw JsonError{"Scene texture preview SHA-256 mismatch: " + texture.id};
      }
      if (preview.contains("native_mips"))
      {
        const auto mips = arrayField(preview, "native_mips");
        const auto native = objectField(textureJson, "native");
        auto width = unsignedField(native, "width");
        auto height = unsignedField(native, "height");
        if (
          mips.isEmpty() || mips.size() > 16
          || mips.size() != unsignedField(native, "mip_levels") || width == 0
          || height == 0 || width > 16384 || height > 16384)
          throw JsonError{"Invalid native texture mip chain"};
        auto index = std::uint64_t{0};
        for (const auto& value : mips)
        {
          if (!value.isObject())
            throw JsonError{"Texture mip must be an object"};
          const auto mipJson = value.toObject();
          auto mip = DahSceneTextureMip{
            safeChildPath(root, stringField(mipJson, "file")),
            stringField(mipJson, "sha256"),
            unsignedField(mipJson, "width"),
            unsignedField(mipJson, "height")};
          if (
            unsignedField(mipJson, "level") != index || mip.width != width
            || mip.height != height || sha256(readFile(mip.path)) != mip.sha256)
            throw JsonError{"Native texture mip dimensions/order/hash mismatch"};
          if (
            index == 0
            && (mip.path != texture.previewPath || mip.sha256 != texture.previewSha256))
            throw JsonError{
              "Texture preview must be the unmodified highest-resolution native mip"};
          if (
            width == 1 && height == 1
            && index + 1 != static_cast<std::uint64_t>(mips.size()))
            throw JsonError{"Native texture mip chain extends past 1x1"};
          texture.nativeMips.push_back(std::move(mip));
          width = std::max(std::uint64_t{1}, width / 2);
          height = std::max(std::uint64_t{1}, height / 2);
          ++index;
        }
      }
      result.textures.push_back(std::move(texture));
    }

    for (const auto& material : result.materials)
    {
      if (material.textureId && !textureIds.contains(*material.textureId))
      {
        throw JsonError{
          "Scene material references missing texture: " + *material.textureId};
      }
    }

    const auto parsePlacements = [&](
                                   const QJsonArray& values,
                                   std::vector<DahScenePlacement>& destination) {
      for (const auto& value : values)
      {
        if (!value.isObject())
        {
          throw JsonError{"Scene placement entry must be an object"};
        }
        const auto placementJson = value.toObject();
        auto placement = DahScenePlacement{};
        placement.id = stringField(placementJson, "id");
        placement.editorCollection =
          optionalStringField(placementJson, "editor_collection");
        placement.previewStatus = optionalStringField(placementJson, "preview_status");
        if (
          placementJson.contains("native_light_role")
          && !placementJson.value("native_light_role").isNull())
        {
          placement.lightRole =
            stringField(objectField(placementJson, "native_light_role"), "role");
          if (placement.lightRole != "static" && placement.lightRole != "dynamic")
            throw JsonError{"Unsupported native visual light role"};
        }
        if (const auto native = placementJson.value("native"); native.isObject())
        {
          placement.nativeLayer = optionalStringField(native.toObject(), "bucket_hash");
          if (native.toObject().contains("list_pointer_field"))
            placement.nativeList =
              "+0x"
              + (unsignedField(native.toObject(), "list_pointer_field") == 12 ? std::string{"0C"} : std::string{"10"});
        }
        const auto instanceColor = placementJson.value("instance_color_rgb_f32");
        if (!instanceColor.isUndefined())
        {
          if (!instanceColor.isArray() || instanceColor.toArray().size() != 3)
          {
            throw JsonError{"Scene instance color must contain three numbers"};
          }
          const auto color = instanceColor.toArray();
          for (auto i = 0; i < 3; ++i)
          {
            if (!color[i].isDouble() || !std::isfinite(color[i].toDouble()))
            {
              throw JsonError{"Scene instance color must be finite"};
            }
            placement.color[static_cast<size_t>(i)] =
              static_cast<float>(color[i].toDouble());
          }
        }
        const auto archetype = placementJson.value("archetype");
        if (archetype.isObject())
        {
          const auto archetypeObject = archetype.toObject();
          placement.nativeClass = optionalStringField(archetypeObject, "native_class");
          placement.assetName = optionalStringField(archetypeObject, "asset_name");
        }
        else if (!archetype.isUndefined() && !archetype.isNull())
        {
          throw JsonError{"Scene placement archetype must be an object or null"};
        }
        const auto matrix = arrayField(placementJson, "matrix_row_major_f32");
        if (matrix.size() != 16)
        {
          throw JsonError{"Scene placement matrix must contain 16 numbers"};
        }
        for (auto i = 0; i < 16; ++i)
        {
          if (!matrix[i].isDouble() || !std::isfinite(matrix[i].toDouble()))
          {
            throw JsonError{"Scene placement matrix contains a non-number"};
          }
          placement.matrix[static_cast<size_t>(i)] = matrix[i].toDouble();
        }
        for (const auto& lodValue : arrayField(placementJson, "lods"))
        {
          if (!lodValue.isObject())
          {
            throw JsonError{"Scene LOD entry must be an object"};
          }
          const auto lodJson = lodValue.toObject();
          auto lod = DahSceneLod{};
          lod.index = unsignedField(lodJson, "index");
          lod.geometryId = stringField(lodJson, "geometry");
          if (!geometryIds.contains(lod.geometryId))
          {
            throw JsonError{"Scene LOD references missing geometry: " + lod.geometryId};
          }
          for (const auto& slot : arrayField(lodJson, "material_slots"))
          {
            if (!slot.isString())
            {
              throw JsonError{"Scene material slot must be a string"};
            }
            auto materialId = slot.toString().toStdString();
            if (!materialIds.contains(materialId))
            {
              throw JsonError{"Scene LOD references missing material: " + materialId};
            }
            lod.materialSlots.push_back(std::move(materialId));
          }
          const auto geometryIt = std::ranges::find_if(
            result.geometries,
            [&](const auto& geometry) { return geometry.id == lod.geometryId; });
          for (const auto& segment : geometryIt->segmentRecords)
          {
            if (segment.materialSlot >= lod.materialSlots.size())
            {
              throw JsonError{
                "Scene geometry segment references an "
                "out-of-range material slot"};
            }
          }
          if (lodJson.contains("parts"))
          {
            for (const auto& partValue : arrayField(lodJson, "parts"))
            {
              if (!partValue.isObject())
                throw JsonError{"LOD component must be an object"};
              const auto partJson = partValue.toObject();
              auto part = DahScenePart{};
              part.geometryId = stringField(partJson, "geometry");
              if (!geometryIds.contains(part.geometryId))
                throw JsonError{"Component geometry missing"};
              const auto partMatrix = arrayField(partJson, "matrix_row_major_f32");
              if (partMatrix.size() != 16)
                throw JsonError{"Component matrix must have 16 values"};
              for (auto i = 0; i < 16; ++i)
              {
                if (!partMatrix[i].isDouble() || !std::isfinite(partMatrix[i].toDouble()))
                  throw JsonError{"Component matrix must be finite"};
                part.matrix[static_cast<size_t>(i)] = partMatrix[i].toDouble();
              }
              for (const auto& slot : arrayField(partJson, "material_slots"))
              {
                if (
                  !slot.isString()
                  || !materialIds.contains(slot.toString().toStdString()))
                  throw JsonError{"Component material missing"};
                part.materialSlots.push_back(slot.toString().toStdString());
              }
              const auto partGeometry = std::ranges::find_if(
                result.geometries,
                [&](const auto& g) { return g.id == part.geometryId; });
              for (const auto& segment : partGeometry->segmentRecords)
                if (segment.materialSlot >= part.materialSlots.size())
                  throw JsonError{"Component material slot out of range"};
              lod.parts.push_back(std::move(part));
            }
            if (lod.parts.empty())
              throw JsonError{"LOD component array must not be empty"};
          }
          if (lodJson.contains("segment_render_states"))
          {
            if (!lod.parts.empty())
              throw std::runtime_error{
                "Multipart LODs cannot use shared segment render states"};
            const auto states = arrayField(lodJson, "segment_render_states");
            if (
              states.size() != static_cast<qsizetype>(geometryIt->segmentRecords.size()))
              throw JsonError{"Terrain segment state count mismatch"};
            for (const auto& stateValue : states)
            {
              if (!stateValue.isObject())
                throw JsonError{"Segment state must be an object"};
              const auto s = stateValue.toObject();
              auto state = DahSceneRenderState{};
              state.alphaMode = stringField(s, "alpha_mode");
              const auto alpha = unsignedField(s, "alpha_reference_u8");
              state.alphaReference = static_cast<float>(alpha) / 255.0f;
              state.blendSource = stringField(s, "blend_source");
              state.blendDestination = stringField(s, "blend_destination");
              state.blendEquation = stringField(s, "blend_equation");
              state.nativeTerrain = true;
              state.rgbScale =
                static_cast<float>(s.value("vertex_rgb_scale").toDouble(-1));
              state.terrainLayer = s.value("terrain_layer").toInt(-999);
              if (!s.value("depth_write").isBool() || alpha > 255
                    || (state.rgbScale != 1.0f && state.rgbScale != 2.0f)
                    || state.terrainLayer < -128 || state.terrainLayer > 127
                    || (state.alphaMode != "opaque" && state.alphaMode != "mask" && state.alphaMode != "blend")
                    || (state.blendSource != "one" && state.blendSource != "src_alpha")
                    || (state.blendDestination != "zero" && state.blendDestination != "one" && state.blendDestination != "one_minus_src_alpha")
                    || (state.blendEquation != "add" && state.blendEquation != "subtract" && state.blendEquation != "reverse_subtract")
                    || stringField(s, "depth_compare") != "less_equal")
                throw JsonError{"Unsupported native terrain render state"};
              state.depthWrite = s.value("depth_write").toBool();
              if (s.contains("native_pixel_shader_index"))
              {
                const auto index = unsignedField(s, "native_pixel_shader_index");
                if (index <= 1)
                  state.pixelShader = static_cast<int>(index);
              }
              lod.segmentStates.push_back(std::move(state));
            }
          }
          if (lodJson.contains("sky_render_states"))
          {
            if (
              placement.editorCollection != "environment-sky"
              || !lod.segmentStates.empty() || !lod.parts.empty())
              throw JsonError{"Native sky states require a single sky mesh"};
            const auto states = arrayField(lodJson, "sky_render_states");
            if (states.size() != geometryIt->segmentRecords.size())
              throw JsonError{"Sky segment state count mismatch"};
            for (const auto& value : states)
            {
              if (!value.isObject())
                throw JsonError{"Sky segment state must be an object"};
              const auto s = value.toObject();
              auto state = DahSceneRenderState{};
              state.sky = true;
              state.lightingMode = 2;
              state.depthWrite = false;
              state.alphaTest = true;
              state.alphaMode = stringField(s, "alpha_mode");
              state.blendSource = stringField(s, "blend_source");
              state.blendDestination = stringField(s, "blend_destination");
              const auto alpha = unsignedField(s, "alpha_reference_u8");
              if (
                (state.alphaMode != "opaque" && state.alphaMode != "blend") || alpha > 255
                || state.blendSource != "one"
                || (state.blendDestination != "one" && state.blendDestination != "zero")
                || stringField(s, "blend_equation") != "add"
                || stringField(s, "alpha_compare") != "greater"
                || s.value("depth_write").toBool(true)
                || !s.value("alpha_test_enabled").toBool()
                || s.value("vertex_rgb_scale").toDouble() != 2.0)
                throw JsonError{"Unsupported native sky state"};
              state.alphaReference = static_cast<float>(alpha) / 255.0f;
              state.rgbScale = 2.0f;
              state.uvTransform = finiteArrayField<8>(s, "effective_uv_transform");
              const auto rgba = finiteArrayField<4>(s, "initial_vertex_rgba");
              state.materialAdditive = {rgba[0], rgba[1], rgba[2]};
              state.rigidOpacity = rgba[3];
              lod.segmentStates.push_back(std::move(state));
            }
          }
          placement.lods.push_back(std::move(lod));
        }
        destination.push_back(std::move(placement));
      }
    };
    parsePlacements(arrayField(json, "placements"), result.placements);
    parsePlacements(arrayField(json, "object_placements"), result.objectPlacements);
    if (json.contains("sky_placements"))
    {
      const auto environment = objectField(json, "environment");
      const auto sky = objectField(objectField(environment, "sky"), "draw_contract");
      if (
        !sky.value("first_visual_enabled").toBool()
        || sky.value("second_visual_enabled").toBool()
        || sky.value("depth_write").toBool(true)
        || sky.value("force_far_depth").toBool(true) || !sky.value("depth_test").toBool()
        || stringField(sky, "depth_compare") != "less_equal"
        || stringField(sky, "draw_order") != "before native world geometry")
        throw JsonError{"Unsupported native sky draw contract"};
      parsePlacements(arrayField(json, "sky_placements"), result.skyPlacements);
      if (
        result.skyPlacements.size() != 1
        || result.skyPlacements.front().editorCollection != "environment-sky")
        throw JsonError{"Expected one verified camera-relative native sky visual"};
    }
    if (json.value("environment").isObject())
    {
      const auto environment = json.value("environment").toObject();
      if (environment.value("lighting_contract").isObject())
      {
        const auto lighting = environment.value("lighting_contract").toObject();
        for (const auto* role : {"static", "dynamic"})
        {
          const auto light = objectField(lighting, role);
          if (
            optionalStringField(light, "evidence_status")
            == "retail-authored-light-verified")
            result.suns.push_back(DahSceneSun{
              role,
              finiteArrayField<3>(light, "direction_world"),
              finiteArrayField<3>(light, "diffuse_rgb"),
              finiteArrayField<3>(light, "ambient_rgb")});
        }
      }
    }
    if (version >= 4)
    {
      parsePlacements(arrayField(json, "landing_placements"), result.landingPlacements);
      if (
        result.landingPlacements.size()
        != unsignedField(objectField(json, "counts"), "landing_visual_placements"))
        throw JsonError{"Landing visual count mismatch"};
    }
    if (version >= 3)
    {
      parsePlacements(arrayField(json, "scatter_placements"), result.scatterPlacements);
      const auto scatterBatches = arrayField(json, "scatter_batches");
      auto nativeInstanceCount = std::uint64_t{0};
      for (const auto& value : scatterBatches)
      {
        if (!value.isObject())
        {
          throw JsonError{"Scatter batch must be an object"};
        }
        const auto batch = value.toObject();
        const auto count = unsignedField(batch, "instance_count");
        const auto view = parseBufferView(
          objectField(batch, "instance_buffer"), buffer, "scatter instances");
        if (view.length / 80 != count || view.length % 80 != 0)
        {
          throw JsonError{"Scatter instance view does not match native stride"};
        }
        nativeInstanceCount += count;
      }
      if (
        static_cast<std::uint64_t>(scatterBatches.size()) != result.counts.scatterBatches
        || nativeInstanceCount != result.counts.scatterPlacements
        || static_cast<std::uint64_t>(arrayField(json, "spawn_definitions").size())
             != result.counts.spawnDefinitions)
      {
        throw JsonError{"Scene native system counts do not match the manifest"};
      }
      auto markerIds = std::unordered_set<std::string>{};
      for (const auto& value : arrayField(json, "scene_markers"))
      {
        if (!value.isObject())
        {
          throw JsonError{"Scene marker must be an object"};
        }
        const auto entry = value.toObject();
        auto marker = DahSceneMarker{};
        marker.id = stringField(entry, "id");
        marker.kind = stringField(entry, "kind");
        marker.label = stringField(entry, "label");
        marker.status = stringField(entry, "status");
        if (!markerIds.insert(marker.id).second)
        {
          throw JsonError{"Duplicate scene marker id"};
        }
        const auto position = arrayField(entry, "position_f32");
        if (position.size() != 3)
        {
          throw JsonError{"Scene marker position must contain three numbers"};
        }
        for (auto i = 0; i < 3; ++i)
        {
          if (!position[i].isDouble() || !std::isfinite(position[i].toDouble()))
          {
            throw JsonError{"Scene marker position must be finite"};
          }
          marker.position[static_cast<size_t>(i)] = position[i].toDouble();
        }
        result.markers.push_back(std::move(marker));
      }
      if (
        result.scatterPlacements.size() != result.counts.scatterPlacements
        || result.markers.size() != result.counts.sceneMarkers
        || static_cast<std::uint64_t>(std::ranges::count_if(
             result.markers, [](const auto& m) { return m.kind == "landing-site"; }))
             != result.counts.landingSites
        || static_cast<std::uint64_t>(std::ranges::count_if(
             result.geometries, [](const auto& g) { return g.kind == "static-scatter"; }))
             != result.counts.scatterGeometries
        || static_cast<std::uint64_t>(std::ranges::count_if(
             result.geometries,
             [](const auto& g) { return g.kind == "human-reference-pose"; }))
             != result.counts.humanGeometries
        || static_cast<std::uint64_t>(std::ranges::count_if(
             result.objectPlacements,
             [](const auto& p) { return p.editorCollection == "humans"; }))
             != result.counts.humanPlacements
        || static_cast<std::uint64_t>(std::ranges::count_if(
             result.objectPlacements,
             [](const auto& p) {
               return p.editorCollection == "humans" && !p.lods.empty();
             }))
             != result.counts.humanRenderablePlacements)
      {
        throw JsonError{"Scene expansion counts do not match the manifest"};
      }
    }

    auto terrainLodCount = std::uint64_t{0};
    for (const auto& placement : result.placements)
    {
      terrainLodCount += placement.lods.size();
    }
    const auto buildingPlacementCount = static_cast<std::uint64_t>(std::ranges::count_if(
      result.objectPlacements,
      [](const auto& placement) { return placement.editorCollection == "buildings"; }));
    const auto renderableBuildingCount = static_cast<std::uint64_t>(
      std::ranges::count_if(result.objectPlacements, [](const auto& placement) {
        return placement.editorCollection == "buildings" && !placement.lods.empty();
      }));
    const auto propPlacementCount = static_cast<std::uint64_t>(std::ranges::count_if(
      result.objectPlacements,
      [](const auto& placement) { return placement.editorCollection == "props"; }));
    const auto renderablePropCount = static_cast<std::uint64_t>(
      std::ranges::count_if(result.objectPlacements, [](const auto& placement) {
        return placement.editorCollection == "props" && !placement.lods.empty();
      }));

    if (
      result.geometries.size() != result.counts.geometries
      || result.materials.size() != result.counts.materials
      || result.textures.size() != result.counts.textures
      || result.placements.size() != result.counts.terrainPlacements
      || result.objectPlacements.size() != result.counts.worldPlacements
      || terrainLodCount != result.counts.terrainLods
      || buildingPlacementCount != result.counts.buildingPlacements
      || renderableBuildingCount != result.counts.buildingRenderablePlacements
      || propPlacementCount != result.counts.propPlacements
      || renderablePropCount != result.counts.propRenderablePlacements
      || renderableBuildingCount + renderablePropCount
           != result.counts.renderableStructurePlacements)
    {
      throw JsonError{"Scene collection counts do not match the manifest"};
    }
    if (json.contains("objective_definitions"))
    {
      auto ids = std::unordered_set<std::string>{};
      for (const auto& value : arrayField(json, "objective_definitions"))
      {
        if (!value.isObject())
          throw JsonError{"Objective definition must be an object"};
        const auto entry = value.toObject();
        auto objective = DahSceneObjective{};
        objective.id = stringField(entry, "id");
        objective.mission = stringField(entry, "mission_table");
        objective.name = stringField(entry, "name");
        objective.type = stringField(entry, "type");
        objective.status = stringField(entry, "activity");
        objective.source = stringField(entry, "source_path");
        if (!ids.insert(objective.id).second)
          throw JsonError{"Duplicate objective definition id"};
        for (const auto& targetValue : arrayField(entry, "targets"))
        {
          if (!targetValue.isObject())
            throw JsonError{"Objective target must be an object"};
          const auto targetEntry = targetValue.toObject();
          auto target = DahSceneObjectiveTarget{};
          target.kind = stringField(targetEntry, "kind");
          target.name = stringField(targetEntry, "name");
          target.status = stringField(targetEntry, "status");
          if (targetEntry.contains("position"))
          {
            const auto values = arrayField(targetEntry, "position");
            if (values.size() != 3)
              throw JsonError{"Objective target position needs three numbers"};
            auto position = std::array<double, 3>{};
            for (auto axis = 0; axis < 3; ++axis)
            {
              if (!values[axis].isDouble() || !std::isfinite(values[axis].toDouble()))
                throw JsonError{"Objective target position must be finite"};
              position[static_cast<size_t>(axis)] = values[axis].toDouble();
            }
            target.position = position;
          }
          if (targetEntry.contains("native_layer_hash"))
            target.nativeLayers.push_back(stringField(targetEntry, "native_layer_hash"));
          if (targetEntry.contains("placement_refs"))
          {
            auto links = std::unordered_set<std::string>{};
            for (const auto& linkValue : arrayField(targetEntry, "placement_refs"))
            {
              if (!linkValue.isObject())
                throw JsonError{"Objective placement reference must be an object"};
              const auto link = linkValue.toObject();
              const auto id = stringField(link, "world_placement_id");
              const auto native = std::ranges::find_if(
                result.objectPlacements,
                [&](const auto& placement) { return placement.id == id; });
              if (native == result.objectPlacements.end() || !links.insert(id).second)
                throw JsonError{
                  "Objective has missing or duplicate world placement link"};
              target.worldPlacementIds.push_back(id);
              const auto layer = stringField(link, "native_layer_hash");
              if (
                std::ranges::find(target.nativeLayers, layer)
                == target.nativeLayers.end())
                target.nativeLayers.push_back(layer);
            }
          }
          objective.targets.push_back(std::move(target));
        }
        if (entry.contains("layer_references"))
        {
          for (const auto& referenceValue : arrayField(entry, "layer_references"))
          {
            if (!referenceValue.isObject())
              throw JsonError{"Objective layer reference must be an object"};
            const auto reference = referenceValue.toObject();
            const auto phase = stringField(reference, "phase");
            for (const auto& layerValue : arrayField(reference, "layers"))
            {
              if (!layerValue.isObject())
                throw JsonError{"Objective layer command must be an object"};
              const auto layer = layerValue.toObject();
              objective.layerCommands.push_back(
                phase + ": " + stringField(layer, "operation") + " "
                + stringField(layer, "name") + " ("
                + stringField(layer, "native_layer_hash") + ")");
            }
          }
        }
        result.objectives.push_back(std::move(objective));
      }
      if (
        result.objectives.size()
        != unsignedField(objectField(json, "counts"), "objective_definitions"))
        throw JsonError{"Objective definition count mismatch"};
    }
    validateArea42Counts(result);
    return result;
  }
  catch (const std::exception& error)
  {
    return DahSceneLoadError{error.what()};
  }
}

} // namespace tb::ui
