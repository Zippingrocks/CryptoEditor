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

#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include "mdl/Map.h"
#include "mdl/WorldNode.h"
#include "render/Renderable.h"
#include "ui/DahMapElements.h"
#include "ui/MapDocument.h"
#include "ui/MapDocumentFixture.h"

#include <algorithm>
#include <array>
#include <set>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

namespace tb::ui
{
namespace
{
class EmptyObjectivePreview : public render::DirectRenderable
{
public:
  void prepare(gl::Gl&, gl::VboManager&) override {}
  void render(render::RenderContext&) override {}
};
} // namespace

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

  SECTION("objectives have explicit targets but no invented origin placement")
  {
    auto fixture = MapDocumentFixture{};
    auto& document = fixture.create();
    auto info = DahSceneInfo{};
    info.mapName = "area42";
    info.bounds = vm::bbox3d{8192.0};
    info.pickupPlacements.push_back(DahObjectPlacementInfo{
      "world-placement:00:0940",
      "PowerupType",
      "pu_probe",
      {12, 34, 56},
      true,
      "authored pickup",
      "Default",
      "+0x10"});
    info.pickupPlacements.push_back(DahObjectPlacementInfo{
      "world-placement:02:0130",
      "PowerupType",
      "pu_ufo_debris",
      {78, 90, 12},
      true,
      "authored pickup",
      "m1_part1",
      "+0x10"});
    auto objective = DahObjectiveInfo{};
    objective.id = "objective:test";
    objective.mission = "b1";
    objective.name = "kill_mutants";
    objective.type = "TagDestroyed";
    objective.status = "activation not simulated";
    objective.source = "b1_objectives.lua";
    objective.targets.push_back(DahObjectiveTargetInfo{
      "tag", "player_alien", "unresolved-runtime-or-absent-native-target", {}, {}, {}});
    objective.targets.push_back(DahObjectiveTargetInfo{
      "region",
      "b1_building",
      "resolved-native-region",
      std::array<double, 3>{25, 50, 75},
      {},
      {}});
    objective.targets.push_back(DahObjectiveTargetInfo{
      "tag",
      "b1_psimutants",
      "resolved-native-tag",
      {},
      {"world-placement:14:0001"},
      {"0xC4F70679"}});
    info.objectives.push_back(std::move(objective));
    info.regions.push_back(DahRegionInfo{
      "region:test", "b1_building", "0x00000001", "0xC4F70679", {25, 50, 75}});
    info.nativeLayers.push_back(DahLayerInfo{"0xC4F70679", "m1_part1", 2, 0, 37, 4});
    info.entityTags.push_back(DahEntityTagInfo{
      "tag:test", "b1_psimutants", "0x00000002", {44140}, {"world-placement:14:0001"}});
    info.unresolvedSystems.push_back("Collision remains unresolved.");
    auto* layer = document.map().worldNode().defaultLayer();
    document.setDahSceneReference(
      std::move(info),
      layer,
      std::make_unique<EmptyObjectivePreview>(),
      layer,
      std::make_unique<EmptyObjectivePreview>(),
      nullptr,
      nullptr);
    auto inspector = DahMapElements{document};
    auto* tree = inspector.findChild<QTreeWidget*>("DahMapElements_GameplayTree");
    REQUIRE(tree != nullptr);
    auto named = [&](const QString& label) {
      auto matches = std::vector<QTreeWidgetItem*>{};
      for (auto item = QTreeWidgetItemIterator{tree}; *item; ++item)
        if ((*item)->text(0) == label)
          matches.push_back(*item);
      return matches;
    };
    REQUIRE(named("kill_mutants").size() == 1);
    CHECK(named("kill_mutants").front()->text(1).contains("no intrinsic map position"));
    REQUIRE(named("tag: player_alien").size() == 1);
    CHECK(
      named("tag: player_alien").front()->text(1).contains("no resolved map position"));
    CHECK_FALSE(named("tag: player_alien").front()->text(1).contains("x 0"));
    CHECK_FALSE(named("kill_mutants").front()->data(0, Qt::UserRole + 2).isValid());
    CHECK_FALSE(named("tag: player_alien").front()->data(0, Qt::UserRole + 2).isValid());
    REQUIRE(named("region: b1_building").size() == 1);
    const auto focusPosition =
      named("region: b1_building").front()->data(0, Qt::UserRole + 2).toList();
    REQUIRE(focusPosition.size() == 3);
    CHECK(focusPosition[0].toDouble() == 25.0);
    CHECK(focusPosition[1].toDouble() == 50.0);
    CHECK(focusPosition[2].toDouble() == 75.0);
    CHECK(named("world-placement:14:0001").size() == 2);
    CHECK(named("world-placement:00:0940").size() == 1);
    CHECK(
      named("world-placement:00:0940")
        .front()
        ->data(0, Qt::UserRole + 2)
        .toList()[0]
        .toDouble()
      == 12.0);
    CHECK(
      named("world-placement:14:0001").front()->data(0, Qt::UserRole + 3).toString()
      == "world-placement:14:0001");
    CHECK(named("world-placement:00:0940").front()->toolTip(0).contains("Double-click"));
    CHECK(named("world-placement:02:0130").size() == 1);
    CHECK(named("Collectible Probes (authored placements)").size() == 1);
    CHECK(named("Mission Debris Pickups (authored placements)").size() == 1);
    CHECK(named("Native Mission / Load Layers").size() == 1);
    CHECK(named("m1_part1").front()->text(1).contains("37 placements"));
    CHECK(named("Native Entity Tags").size() == 1);
    CHECK(named("b1_psimutants").front()->text(1).contains("1 resolved"));

    auto* worldTree = inspector.findChild<QTreeWidget*>("DahMapElements_WorldTree");
    REQUIRE(worldTree != nullptr);
    auto worldNamed = [&](const QString& label) {
      auto matches = std::vector<QTreeWidgetItem*>{};
      for (auto item = QTreeWidgetItemIterator{worldTree}; *item; ++item)
        if ((*item)->text(0) == label)
          matches.push_back(*item);
      return matches;
    };
    CHECK(worldNamed("Native Spatial Regions").size() == 1);
    CHECK(worldNamed("b1_building").size() == 1);

    auto* systemsTree = inspector.findChild<QTreeWidget*>("DahMapElements_SystemsTree");
    REQUIRE(systemsTree != nullptr);
    auto systemNamed = [&](const QString& label) {
      auto matches = std::vector<QTreeWidgetItem*>{};
      for (auto item = QTreeWidgetItemIterator{systemsTree}; *item; ++item)
        if ((*item)->text(0) == label)
          matches.push_back(*item);
      return matches;
    };
    CHECK(systemNamed("Area 42 Support Coverage").size() == 1);
    CHECK(
      systemNamed("Known unresolved systems").front()->text(1).contains("1 explicit"));
  }
}

} // namespace tb::ui
