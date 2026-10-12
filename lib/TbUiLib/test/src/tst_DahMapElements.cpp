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

#include "ui/DahMapElements.h"

#include <algorithm>
#include <array>
#include <set>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

namespace tb::ui
{

TEST_CASE("DahMapElements", "[DahMapElements]")
{
  const auto& groups = dahMapElementGroups();

  SECTION("assigns unique stable group ids")
  {
    auto ids = std::set<std::string_view>{};
    for (const auto& group : groups)
    {
      CHECK(!group.id.empty());
      CHECK(!group.title.empty());
      CHECK(!group.contents.empty());
      CHECK(ids.insert(group.id).second);
    }
  }

  SECTION("keeps human structures in the Buildings tab")
  {
    auto humanStructureGroups = 0u;
    for (const auto& group : groups)
    {
      if (group.humanStructure)
      {
        ++humanStructureGroups;
        CHECK(group.tab == DahMapElementTab::Buildings);
        CHECK(group.collection == "Human Structures");
      }
    }
    CHECK(humanStructureGroups == 6u);
  }

  SECTION("covers every map element domain")
  {
    const auto tabs = std::array{
      DahMapElementTab::Buildings,
      DahMapElementTab::World,
      DahMapElementTab::Props,
      DahMapElementTab::Actors,
      DahMapElementTab::Gameplay,
      DahMapElementTab::Systems,
    };
    for (const auto tab : tabs)
    {
      CHECK(!dahMapElementTabTitle(tab).empty());
      CHECK(std::ranges::any_of(
        groups, [tab](const auto& group) { return group.tab == tab; }));
    }
  }
}

} // namespace tb::ui
