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

#include "gl/VertexType.h"
#include "render/Renderable.h"
#include "ui/DahSceneManifest.h"

#include "vm/bbox.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <unordered_set>

namespace tb::gl
{
class ResourceManager;
}

namespace tb::ui
{

struct DahSceneReferenceBatch
{
  using Vertex = gl::VertexTypes::P3NT2C4::Vertex;

  std::string materialId;
  std::optional<std::filesystem::path> texturePreviewPath;
  DahSceneMaterial::AddressMode addressU = DahSceneMaterial::AddressMode::Repeat;
  DahSceneMaterial::AddressMode addressV = DahSceneMaterial::AddressMode::Repeat;
  std::vector<Vertex> vertices;
  std::string alphaMode = "opaque";
  float alphaReference = 0.0f;
  DahSceneRenderState state;
  std::vector<DahSceneTextureMip> textureMips;
  bool nativeSampler = false;
  bool nearestMagnification = false;
  bool mipFilter = true;
  float lodBias = 0.0f;
  std::string lightRole;
  std::string nativeLayer;
};

struct DahSceneBatchSortInfo
{
  vm::vec3f center;
  bool sky = false;
  bool blended = false;
  bool terrain = false;
  int terrainLayer = 0;
};

std::vector<size_t> dahSceneDrawOrder(
  const std::vector<DahSceneBatchSortInfo>& batches,
  const vm::vec3f& cameraPosition,
  const vm::vec3f& cameraDirection);

struct DahSceneReference
{
  std::vector<DahSceneReferenceBatch> batches;
  vm::bbox3d bounds;
  std::uint64_t lodIndex = 0;
  std::uint64_t placementCount = 0;
  std::uint64_t nativeTriangleCount = 0;
  std::uint64_t texturedMaterialCount = 0;
  std::uint64_t skyTriangleCount = 0;
  std::vector<DahSceneSun> suns;
  DahSceneFog fog;

  std::uint64_t vertexCount() const;
};

using DahSceneReferenceResult = std::variant<DahSceneReference, DahSceneLoadError>;

DahSceneReferenceResult buildDahSceneReference(
  const DahSceneManifest& manifest, std::uint64_t lodIndex);

// Editor annotations only; these triangles are never native game geometry.
DahSceneReference buildDahSceneMarkerReference(const DahSceneManifest& manifest);

// Proven retail region membership volumes; editor visualization only.
DahSceneReference buildDahSceneRegionReference(const DahSceneManifest& manifest);

std::unique_ptr<render::DirectRenderable> createDahSceneReferenceRenderer(
  DahSceneReference reference, gl::ResourceManager& resourceManager);

void setDahSceneReferenceLayerFilter(
  render::DirectRenderable& renderer,
  std::optional<std::unordered_set<std::string>> visibleNativeLayers);

} // namespace tb::ui
