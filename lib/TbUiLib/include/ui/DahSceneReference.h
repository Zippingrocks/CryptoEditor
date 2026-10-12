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
#include <memory>
#include <variant>
#include <vector>

namespace tb::ui
{

struct DahSceneReference
{
  using Vertex = gl::VertexTypes::P3N::Vertex;

  std::vector<Vertex> vertices;
  vm::bbox3d bounds;
  std::uint64_t lodIndex = 0;
  std::uint64_t placementCount = 0;
  std::uint64_t nativeTriangleCount = 0;
};

using DahSceneReferenceResult = std::variant<DahSceneReference, DahSceneLoadError>;

DahSceneReferenceResult buildDahSceneReference(
  const DahSceneManifest& manifest, std::uint64_t lodIndex);

std::unique_ptr<render::DirectRenderable> createDahSceneReferenceRenderer(
  DahSceneReference reference);

} // namespace tb::ui
