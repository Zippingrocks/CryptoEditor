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

#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QVariant>

#include "gl/Camera.h"
#include "ui/MapDocument.h"
#include "ui/MapViewBase.h"
#include "ui/DahSceneReference.h"
#include "ui/ViewConstants.h"

#include <array>
#include <cmath>
#include <map>
#include <ranges>
#include <unordered_set>

namespace tb::ui
{
namespace
{

const auto Tabs = std::array{
  DahMapElementTab::Buildings,
  DahMapElementTab::World,
  DahMapElementTab::Props,
  DahMapElementTab::Actors,
  DahMapElementTab::Gameplay,
  DahMapElementTab::Systems,
};

const auto Groups = std::vector<DahMapElementGroup>{
  {DahMapElementTab::Buildings,
   "Human Structures",
   "human-residential",
   "Residential",
   "Occupied or domestic structures; keep interiors and destruction links explicit.",
   {"Houses", "Farmhouses", "Trailers", "Apartments", "Garages"},
   true},
  {DahMapElementTab::Buildings,
   "Human Structures",
   "human-civic-commercial",
   "Civic & Commercial",
   "Public-facing buildings and town services.",
   {"Shops",
    "Diners",
    "Motels",
    "Schools",
    "Hospitals",
    "Police & fire",
    "Government offices"},
   true},
  {DahMapElementTab::Buildings,
   "Human Structures",
   "human-industrial-agricultural",
   "Industrial & Agricultural",
   "Production, storage, farming, and utility buildings.",
   {"Barns", "Silos", "Warehouses", "Factories", "Power facilities", "Workshops"},
   true},
  {DahMapElementTab::Buildings,
   "Human Structures",
   "human-military-government",
   "Military & Government",
   "Restricted human facilities; gameplay ownership remains in the Gameplay tab.",
   {"Barracks", "Hangars", "Bunkers", "Laboratories", "Checkpoints", "Guard towers"},
   true},
  {DahMapElementTab::Buildings,
   "Human Structures",
   "human-infrastructure",
   "Infrastructure",
   "Large constructed structures that shape traversal and map boundaries.",
   {"Bridges", "Tunnels", "Stations", "Utility structures", "Walls", "Gates"},
   true},
  {DahMapElementTab::Buildings,
   "Human Structures",
   "human-modular",
   "Modular Construction",
   "Composable building pieces. Native whole-building assets must not be silently "
   "converted into brushes.",
   {"Shells", "Walls", "Roofs", "Floors", "Doors", "Windows", "Interior modules"},
   true},
  {DahMapElementTab::Buildings,
   "Furon Structures",
   "furon-structures",
   "Furon Architecture",
   "Alien structures and devices stay separate from human building families.",
   {"Mission structures", "Landing structures", "Furon devices", "Energy installations"}},

  {DahMapElementTab::World,
   "World Geometry",
   "world-terrain",
   "Terrain",
   "Native terrain meshes, editable terrain, heightfield projections, materials, and LOD "
   "relationships.",
   {"Native terrain",
    "Editable terrain",
    "Heightfield overlays",
    "Terrain materials",
    "Terrain LODs"}},
  {DahMapElementTab::World,
   "Environment",
   "world-environment",
   "Water, Atmosphere & Lighting",
   "Global presentation systems remain independent of physical geometry.",
   {"Water", "Sky", "Fog", "Weather", "Sun & ambient light", "Local lights"}},
  {DahMapElementTab::World,
   "Map Partition",
   "world-partition",
   "Bounds & Streaming",
   "World limits and load partitions that determine what the game keeps resident.",
   {"Map bounds", "Streaming cells", "Load groups", "Portals", "Occlusion regions"}},

  {DahMapElementTab::Props,
   "Set Dressing",
   "props-set-dressing",
   "Human Props",
   "Movable and static human-scale scenery, separate from buildings.",
   {"Street furniture",
    "Domestic props",
    "Commercial props",
    "Industrial props",
    "Military props"}},
  {DahMapElementTab::Props,
   "Natural Dressing",
   "props-natural",
   "Vegetation & Natural Props",
   "Non-terrain environmental meshes and scatter sets.",
   {"Trees", "Bushes", "Crops", "Rocks", "Debris", "Scatter groups"}},
  {DahMapElementTab::Props,
   "Interactive Objects",
   "props-interactive",
   "Pickups & Interactables",
   "Objects whose behavior or rewards are defined by gameplay data.",
   {"Pickups", "Collectibles", "Usable devices", "Physics props", "Explosive props"}},

  {DahMapElementTab::Actors,
   "Characters",
   "actors-characters",
   "Characters & Creatures",
   "Placed or spawned actors; mission behavior is referenced rather than embedded here.",
   {"Humans", "Agents & military", "Crypto", "Furon", "Animals", "Ambient populations"}},
  {DahMapElementTab::Actors,
   "Vehicles",
   "actors-vehicles",
   "Vehicles",
   "Vehicle assets, initial placements, and spawn definitions.",
   {"Civilian vehicles",
    "Military vehicles",
    "Aircraft",
    "Boats",
    "Saucer",
    "Vehicle spawners"}},

  {DahMapElementTab::Gameplay,
   "Player & Encounters",
   "gameplay-spawns",
   "Spawns & Encounters",
   "Runtime placement rules rather than visual asset ownership.",
   {"Player starts",
    "NPC spawns",
    "Vehicle spawns",
    "Encounter groups",
    "Reinforcements"}},
  {DahMapElementTab::Gameplay,
   "Mission Logic",
   "gameplay-missions",
   "Missions, Triggers & Regions",
   "Objectives and scripted state with stable references to world objects.",
   {"Mission scripts",
    "Objectives",
    "Triggers",
    "Regions",
    "Checkpoints",
    "Failure conditions"}},
  {DahMapElementTab::Gameplay,
   "Movement",
   "gameplay-navigation",
   "Paths & Navigation",
   "AI, vehicle, pedestrian, and camera movement networks.",
   {"Navigation graphs",
    "Patrol paths",
    "Traffic paths",
    "Pedestrian routes",
    "Camera rails"}},

  {DahMapElementTab::Systems,
   "Physical Systems",
   "systems-physical",
   "Collision & Destruction",
   "Collision shapes and destruction graphs link to assets but remain independently "
   "inspectable.",
   {"Static collision",
    "Dynamic collision",
    "Damage volumes",
    "Destruction stages",
    "Debris sets",
    "Physics constraints"}},
  {DahMapElementTab::Systems,
   "Presentation",
   "systems-presentation",
   "Effects, Audio & Cameras",
   "Non-geometric presentation resources and their emitters.",
   {"Particle effects",
    "Decals",
    "Audio emitters",
    "Music regions",
    "Cameras",
    "Cinematics"}},
  {DahMapElementTab::Systems,
   "Native Data",
   "systems-native",
   "Resources & Unknown Records",
   "Preserve identity, packaging, relocations, and undecoded fields without forcing them "
   "into visual categories.",
   {"Resource identities",
    "Load groups",
    "Relocations",
    "Package ownership",
    "Unknown records",
    "Validation issues"}},
};

QString text(const std::string_view value)
{
  return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

bool matchesFilter(const QTreeWidgetItem& item, const QString& filter)
{
  return item.text(0).contains(filter, Qt::CaseInsensitive)
         || item.text(1).contains(filter, Qt::CaseInsensitive);
}

constexpr auto AuthoringTaxonomyRole = Qt::UserRole + 1;
constexpr auto NativeFocusPositionRole = Qt::UserRole + 2;
constexpr auto NativePlacementLinkRole = Qt::UserRole + 3;

void setNativeFocusPosition(QTreeWidgetItem& item, const std::array<double, 3>& position)
{
  item.setData(
    0, NativeFocusPositionRole, QVariantList{position[0], position[1], position[2]});
  const auto hint = QStringLiteral("Double-click to focus this genuine native location.");
  item.setToolTip(0, item.toolTip(0) + "\n" + hint);
  item.setToolTip(1, item.toolTip(1) + "\n" + hint);
}

std::optional<std::array<double, 3>> nativeFocusPosition(
  const QTreeWidgetItem& item, const DahSceneInfo& scene)
{
  const auto point = item.data(0, NativeFocusPositionRole).toList();
  if (point.size() == 3)
  {
    auto result = std::array<double, 3>{};
    for (auto axis = 0; axis < 3; ++axis)
    {
      auto valid = false;
      result[static_cast<size_t>(axis)] = point[axis].toDouble(&valid);
      if (!valid || !std::isfinite(result[static_cast<size_t>(axis)]))
        return {};
    }
    return result;
  }
  const auto link = item.data(0, NativePlacementLinkRole).toString().toStdString();
  if (link.empty())
    return {};
  const auto collections = std::array{
    &scene.buildingPlacements,
    &scene.propPlacements,
    &scene.humanPlacements,
    &scene.scatterPlacements,
    &scene.vehiclePlacements,
    &scene.animalPlacements,
    &scene.robotPlacements,
    &scene.emplacementPlacements,
    &scene.pickupPlacements,
    &scene.populationPlacements,
    &scene.otherWorldPlacements,
    &scene.landingPlacements};
  for (const auto* collection : collections)
    for (const auto& placement : *collection)
      if (placement.id == link)
        return placement.position;
  return {};
}

QString placementDetail(const DahObjectPlacementInfo& placement)
{
  const auto status = placement.status.empty()
                        ? (placement.renderable ? QStringLiteral("native mesh")
                                                : QStringLiteral("placement preserved"))
                        : QString::fromStdString(placement.status);
  auto result = QStringLiteral("x %1, y %2, z %3 — %4")
                  .arg(placement.position[0], 0, 'f', 2)
                  .arg(placement.position[1], 0, 'f', 2)
                  .arg(placement.position[2], 0, 'f', 2)
                  .arg(status);
  if (!placement.nativeLayer.empty())
  {
    result +=
      QStringLiteral(" — layer: ") + QString::fromStdString(placement.nativeLayer);
  }
  if (!placement.nativeList.empty())
  {
    result += QStringLiteral(" — list: ") + QString::fromStdString(placement.nativeList);
  }
  if (placement.entityId)
  {
    result += QStringLiteral(" — entity: %1").arg(*placement.entityId);
  }
  if (!placement.tagIds.empty())
  {
    result += QStringLiteral(" — %1 native tag(s)").arg(placement.tagIds.size());
  }
  return result;
}

void appendNativePlacements(
  QTreeWidgetItem& root, const std::vector<DahObjectPlacementInfo>& placements)
{
  using PlacementList = std::vector<const DahObjectPlacementInfo*>;
  auto classes = std::map<std::string, std::map<std::string, PlacementList>>{};
  for (const auto& placement : placements)
  {
    const auto nativeClass =
      placement.nativeClass.empty() ? "Unresolved native class" : placement.nativeClass;
    const auto asset =
      placement.assetName.empty() ? "Unnamed native asset" : placement.assetName;
    classes[nativeClass][asset].push_back(&placement);
  }
  for (const auto& [nativeClass, assets] : classes)
  {
    auto* classRoot = new QTreeWidgetItem{
      &root, {QString::fromStdString(nativeClass), QStringLiteral("Native class")}};
    for (const auto& [asset, records] : assets)
    {
      auto* assetRoot = new QTreeWidgetItem{
        classRoot,
        {QString::fromStdString(asset),
         QStringLiteral("%1 placements").arg(records.size())}};
      for (const auto* placement : records)
      {
        auto* item = new QTreeWidgetItem{
          assetRoot,
          {QString::fromStdString(placement->id), placementDetail(*placement)}};
        item->setToolTip(0, QString::fromStdString(placement->id));
        item->setToolTip(1, placementDetail(*placement));
        setNativeFocusPosition(*item, placement->position);
      }
    }
    classRoot->setExpanded(true);
  }
  root.setExpanded(true);
}

void appendObjectives(
  QTreeWidgetItem& root, const std::vector<DahObjectiveInfo>& objectives)
{
  using ObjectiveList = std::vector<const DahObjectiveInfo*>;
  auto missions = std::map<std::string, std::map<std::string, ObjectiveList>>{};
  for (const auto& objective : objectives)
    missions[objective.mission][objective.type].push_back(&objective);
  for (const auto& [mission, types] : missions)
  {
    auto* missionRoot = new QTreeWidgetItem{
      &root,
      {QString::fromStdString(mission),
       QStringLiteral("Native script table; activation not simulated")}};
    for (const auto& [type, records] : types)
    {
      auto* typeRoot = new QTreeWidgetItem{
        missionRoot,
        {QString::fromStdString(type),
         QStringLiteral("%1 definitions").arg(records.size())}};
      for (const auto* objective : records)
      {
        auto* item = new QTreeWidgetItem{
          typeRoot,
          {QString::fromStdString(objective->name),
           QStringLiteral("Script definition — no intrinsic map position")}};
        item->setToolTip(0, QString::fromStdString(objective->id));
        item->setToolTip(
          1,
          QStringLiteral("No intrinsic position. Double-click a resolved target or "
                         "placed-object link below to focus it."));
        new QTreeWidgetItem{
          item, {QStringLiteral("Source"), QString::fromStdString(objective->source)}};
        new QTreeWidgetItem{
          item,
          {QStringLiteral("Runtime state"), QString::fromStdString(objective->status)}};
        for (const auto& target : objective->targets)
        {
          auto detail = QString::fromStdString(target.status);
          if (target.position)
          {
            detail += QStringLiteral(" — native region center x %1, y %2, z %3")
                        .arg((*target.position)[0], 0, 'f', 2)
                        .arg((*target.position)[1], 0, 'f', 2)
                        .arg((*target.position)[2], 0, 'f', 2);
          }
          else if (target.worldPlacementIds.empty())
          {
            detail += QStringLiteral(" — no resolved map position");
          }
          auto* targetItem = new QTreeWidgetItem{
            item, {QString::fromStdString(target.kind + ": " + target.name), detail}};
          if (target.position)
            setNativeFocusPosition(*targetItem, *target.position);
          else
            targetItem->setToolTip(
              1,
              QStringLiteral("No single resolved native position; use a placed-object "
                             "link below, if available."));
          for (const auto& layer : target.nativeLayers)
            new QTreeWidgetItem{
              targetItem,
              {QStringLiteral("Native layer"), QString::fromStdString(layer)}};
          for (const auto& link : target.worldPlacementIds)
          {
            auto* linkItem = new QTreeWidgetItem{
              targetItem,
              {QString::fromStdString(link),
               QStringLiteral(
                 "Reference to an existing authored object; not a new placement")}};
            linkItem->setData(0, NativePlacementLinkRole, QString::fromStdString(link));
            linkItem->setToolTip(
              0,
              QStringLiteral("Double-click to focus the referenced native placement; "
                             "mission state is not changed."));
          }
        }
        for (const auto& command : objective->layerCommands)
          new QTreeWidgetItem{
            item, {QStringLiteral("Layer command"), QString::fromStdString(command)}};
      }
    }
    missionRoot->setExpanded(true);
  }
  root.setExpanded(true);
}

QString nativeName(
  const std::string& name, const std::string& hash, const QString& unnamedKind)
{
  return name.empty()
           ? QStringLiteral("%1 %2").arg(unnamedKind, QString::fromStdString(hash))
           : QString::fromStdString(name);
}

void appendRegions(QTreeWidgetItem& root, const std::vector<DahRegionInfo>& regions)
{
  auto byLayer = std::map<std::string, std::vector<const DahRegionInfo*>>{};
  for (const auto& region : regions)
    byLayer[region.nativeLayerHash].push_back(&region);
  for (const auto& [layer, records] : byLayer)
  {
    auto* layerRoot = new QTreeWidgetItem{
      &root,
      {QString::fromStdString(layer),
       QStringLiteral("%1 native regions").arg(records.size())}};
    for (const auto* region : records)
    {
      auto* item = new QTreeWidgetItem{
        layerRoot,
        {nativeName(region->name, region->nameHash, QStringLiteral("Unnamed region")),
         QStringLiteral("center x %1, y %2, z %3 — %4")
           .arg(region->position[0], 0, 'f', 2)
           .arg(region->position[1], 0, 'f', 2)
           .arg(region->position[2], 0, 'f', 2)
           .arg(QString::fromStdString(region->id))}};
      setNativeFocusPosition(*item, region->position);
    }
  }
  root.setExpanded(true);
}

void appendLayers(QTreeWidgetItem& root, const std::vector<DahLayerInfo>& layers)
{
  for (const auto& layer : layers)
  {
    auto* item = new QTreeWidgetItem{
      &root,
      {nativeName(layer.name, layer.hash, QStringLiteral("Unnamed layer")),
       QStringLiteral("index %1 — load group %2 — %3 placements — %4 script calls")
         .arg(layer.index)
         .arg(layer.loadGroup)
         .arg(layer.placementCount)
         .arg(layer.scriptCallCount)}};
    item->setToolTip(
      0,
      QStringLiteral(
        "Native layer hash %1. Visibility is authored by mission scripts and is not "
        "simulated in this read-only scene.")
        .arg(QString::fromStdString(layer.hash)));
    for (const auto& owner : layer.missionOwners)
      new QTreeWidgetItem{
        item,
        {QStringLiteral("Mission owner"), QString::fromStdString(owner)}};
    for (const auto& command : layer.commands)
      new QTreeWidgetItem{
        item,
        {QStringLiteral("Native layer command"), QString::fromStdString(command)}};
  }
  root.setExpanded(true);
}

void appendMissionWorkspaces(QTreeWidgetItem& root, const DahSceneInfo& scene)
{
  for (const auto& mission : scene.missions)
  {
    auto objectiveCount = std::uint64_t{0};
    for (const auto& objective : scene.objectives)
      if (objective.mission == mission.name)
        ++objectiveCount;
    auto* missionRoot = new QTreeWidgetItem{
      &root,
      {QString::fromStdString(mission.name),
       QStringLiteral("%1 — %2 objectives — activation state not evaluated")
         .arg(mission.type.empty() ? QStringLiteral("mission")
                                  : QString::fromStdString(mission.type))
         .arg(objectiveCount)}};
    new QTreeWidgetItem{
      missionRoot,
      {QStringLiteral("Registration source"), QString::fromStdString(mission.source)}};
    if (!mission.zone.empty())
      new QTreeWidgetItem{
        missionRoot,
        {QStringLiteral("Registration zone"), QString::fromStdString(mission.zone)}};
    auto* layersRoot = new QTreeWidgetItem{
      missionRoot,
      {QStringLiteral("Referenced native layers"),
       QStringLiteral("References only; objects are never duplicated")}};
    auto layerCount = std::uint64_t{0};
    for (const auto& layer : scene.nativeLayers)
    {
      if (
        std::ranges::find(layer.missionOwners, mission.name)
        == layer.missionOwners.end())
        continue;
      ++layerCount;
      auto* layerItem = new QTreeWidgetItem{
        layersRoot,
        {nativeName(layer.name, layer.hash, QStringLiteral("Unnamed layer")),
         QStringLiteral("%1 placements — %2")
           .arg(layer.placementCount)
           .arg(QString::fromStdString(layer.hash))}};
      for (const auto& command : layer.commands)
        if (command.find(mission.name) != std::string::npos)
          new QTreeWidgetItem{
            layerItem,
            {QStringLiteral("Command evidence"), QString::fromStdString(command)}};
    }
    layersRoot->setText(
      1,
      QStringLiteral("%1 referenced layers; references only, never duplicated")
        .arg(layerCount));
  }
  auto* sharedRoot = new QTreeWidgetItem{
    &root,
    {QStringLiteral("Shared / unassigned native layers"),
     QStringLiteral("No mission-specific script owner decoded")}};
  for (const auto& layer : scene.nativeLayers)
    if (layer.missionOwners.empty())
      new QTreeWidgetItem{
        sharedRoot,
        {nativeName(layer.name, layer.hash, QStringLiteral("Unnamed layer")),
         QStringLiteral("%1 placements — %2")
           .arg(layer.placementCount)
           .arg(QString::fromStdString(layer.hash))}};
  root.setExpanded(true);
}

void appendEntityTags(QTreeWidgetItem& root, const std::vector<DahEntityTagInfo>& tags)
{
  for (const auto& tag : tags)
  {
    auto* item = new QTreeWidgetItem{
      &root,
      {nativeName(tag.name, tag.nameHash, QStringLiteral("Unnamed tag")),
       QStringLiteral("%1 entity IDs — %2 resolved scene placements")
         .arg(tag.entityIds.size())
         .arg(tag.placementIds.size())}};
    item->setToolTip(0, QString::fromStdString(tag.id));
    for (const auto& placementId : tag.placementIds)
    {
      auto* link = new QTreeWidgetItem{
        item,
        {QString::fromStdString(placementId),
         QStringLiteral("Existing native entity membership; double-click to focus")}};
      link->setData(0, NativePlacementLinkRole, QString::fromStdString(placementId));
    }
    if (tag.placementIds.empty())
    {
      new QTreeWidgetItem{
        item,
        {QStringLiteral("No fixed placement link"),
         QStringLiteral(
           "Tag may reference runtime-created or otherwise unresolved entities")}};
    }
  }
  root.setExpanded(true);
}

bool filterTreeItem(
  QTreeWidgetItem& item, const QString& filter, const bool parentMatches)
{
  const auto matches = parentMatches || filter.isEmpty() || matchesFilter(item, filter);
  auto visible = matches;
  for (auto index = 0; index < item.childCount(); ++index)
  {
    visible = filterTreeItem(*item.child(index), filter, matches) || visible;
  }
  item.setHidden(!visible);
  return visible;
}

} // namespace

const std::vector<DahMapElementGroup>& dahMapElementGroups()
{
  return Groups;
}

std::string_view dahMapElementTabTitle(const DahMapElementTab tab)
{
  switch (tab)
  {
  case DahMapElementTab::Buildings:
    return "Buildings";
  case DahMapElementTab::World:
    return "World";
  case DahMapElementTab::Props:
    return "Props";
  case DahMapElementTab::Actors:
    return "Actors";
  case DahMapElementTab::Gameplay:
    return "Gameplay";
  case DahMapElementTab::Systems:
    return "Systems";
  }
  return "Unknown";
}

DahMapElements::DahMapElements(MapDocument& document, QWidget* parent)
  : TabBookPage{parent}
  , m_document{document}
{
  createGui();
  connectObservers();
  updateStatus();
}

void DahMapElements::createGui()
{
  auto* introduction = new QLabel{
    tr("Loaded DAH records are organized by native class and asset. Layer and list "
       "provenance remain visible; mission records are not merged into a single "
       "gameplay state. This inspector does not place catalog examples into the map.")};
  introduction->setWordWrap(true);

  m_status = new QLabel{};
  m_status->setObjectName("DahMapElements_Status");
  m_status->setWordWrap(true);

  m_filter = new QLineEdit{};
  m_filter->setObjectName("DahMapElements_Filter");
  m_filter->setPlaceholderText(tr("Filter DAH map elements"));

  m_missionView = new QComboBox{};
  m_missionView->setObjectName("DahMapElements_MissionView");
  m_missionView->setToolTip(
    tr("Filters native viewport batches using proven mission-layer ownership. "
       "Conditional objective phases are not simulated."));

  m_tabs = new QTabWidget{};
  m_tabs->setObjectName("DahMapElements_Tabs");
  for (const auto tab : Tabs)
  {
    m_tabs->addTab(createTab(tab), text(dahMapElementTabTitle(tab)));
  }

  auto* layout = new QVBoxLayout{};
  layout->setContentsMargins(
    LayoutConstants::MediumHMargin,
    LayoutConstants::MediumVMargin,
    LayoutConstants::MediumHMargin,
    LayoutConstants::MediumVMargin);
  layout->setSpacing(LayoutConstants::MediumVMargin);
  layout->addWidget(introduction);
  layout->addWidget(m_status);
  layout->addWidget(new QLabel{tr("Viewport mission scope")});
  layout->addWidget(m_missionView);
  layout->addWidget(m_filter);
  layout->addWidget(m_tabs, 1);
  setLayout(layout);

  connect(m_filter, &QLineEdit::textChanged, this, &DahMapElements::updateFilter);
  connect(
    m_missionView,
    &QComboBox::currentIndexChanged,
    this,
    [this](int) { updateMissionView(); });
}

QWidget* DahMapElements::createTab(const DahMapElementTab tab)
{
  auto* tree = new QTreeWidget{};
  tree->setObjectName("DahMapElements_" + text(dahMapElementTabTitle(tab)) + "Tree");
  tree->setColumnCount(2);
  tree->setHeaderLabels(
    {tr("Native class / asset / placement"), tr("Status / provenance")});
  tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  tree->setRootIsDecorated(true);
  tree->setAlternatingRowColors(true);
  connect(
    tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
      const auto* scene = m_document.dahSceneInfo();
      if (!item || !scene)
        return;
      const auto nativePosition = nativeFocusPosition(*item, *scene);
      if (!nativePosition)
        return; // Definitions and unresolved targets never jump to an invented origin.
      auto* selectedView = static_cast<MapViewBase*>(nullptr);
      for (auto* view : window()->findChildren<MapViewBase*>())
      {
        if (!view->isVisible())
          continue;
        if (!selectedView || view->isCurrent())
          selectedView = view;
        if (view->isCurrent())
          break;
      }
      if (!selectedView)
        return;
      const auto target = vm::vec3f{
        static_cast<float>((*nativePosition)[0]),
        static_cast<float>((*nativePosition)[1]),
        static_cast<float>((*nativePosition)[2])};
      // Preserve viewing direction and stand back 48 native units. Orthographic
      // views still center the same target; no native object or layer is edited.
      selectedView->moveCameraToPosition(
        target - selectedView->camera().direction() * 48.0f, true);
      selectedView->refreshViews();
    });
  if (tab == DahMapElementTab::Buildings)
  {
    m_buildingsTree = tree;
  }
  else if (tab == DahMapElementTab::Props)
  {
    m_propsTree = tree;
  }

  auto* authoringRoot = new QTreeWidgetItem{
    tree,
    {tr("Authoring categories (not loaded objects)"),
     tr("Planning taxonomy; no native placements are implied")}};
  authoringRoot->setData(0, AuthoringTaxonomyRole, true);
  auto collections = std::map<std::string_view, QTreeWidgetItem*>{};
  for (const auto& group : Groups)
  {
    if (group.tab != tab)
    {
      continue;
    }
    auto [it, inserted] = collections.try_emplace(group.collection, nullptr);
    if (inserted)
    {
      it->second = new QTreeWidgetItem{authoringRoot, {text(group.collection)}};
      auto font = it->second->font(0);
      font.setBold(true);
      it->second->setFont(0, font);
      it->second->setFirstColumnSpanned(true);
    }
    auto* category =
      new QTreeWidgetItem{it->second, {text(group.title), text(group.purpose)}};
    category->setData(0, Qt::UserRole, text(group.id));
    for (const auto content : group.contents)
    {
      new QTreeWidgetItem{
        category,
        {text(content),
         group.humanStructure ? tr("Human building structure")
                              : tr("Separate asset or system record")}};
    }
  }
  tree->expandToDepth(1);
  m_trees.push_back(tree);

  auto* page = new QWidget{};
  auto* layout = new QVBoxLayout{};
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(tree);
  page->setLayout(layout);
  return page;
}

void DahMapElements::connectObservers()
{
  m_notifierConnection +=
    m_document.documentWasLoadedNotifier.connect([this]() { updateStatus(); });
}

void DahMapElements::updateStatus()
{
  const auto blocker = QSignalBlocker{m_missionView};
  const auto previousMission = m_missionView->currentData().toString();
  m_missionView->clear();
  m_missionView->addItem(tr("All authored content (diagnostic)"), QStringLiteral("*"));
  m_missionView->addItem(tr("Shared / unassigned only"), QString{});
  if (const auto* scene = m_document.dahSceneInfo())
  {
    for (const auto& mission : scene->missions)
      m_missionView->addItem(
        QString::fromStdString(mission.name), QString::fromStdString(mission.name));
    const auto previousIndex = m_missionView->findData(previousMission);
    m_missionView->setCurrentIndex(previousIndex < 0 ? 0 : previousIndex);
    m_status->setText(
      tr("%1: %2 world records, including %3 buildings, %4 props and %5 humans; "
         "%6 scatter instances, %7 native landing visuals, %8 editor markers and %9 "
         "objective definitions; %10 regions, %11 mission layers, %12 registered "
         "missions and %13 entity tags. "
         "Actors use authored reference poses. Native mission layers remain distinct; "
         "this is not a simulated mission state.")
        .arg(QString::fromStdString(scene->mapName))
        .arg(scene->worldPlacementCount)
        .arg(scene->buildingPlacementCount)
        .arg(scene->propPlacementCount)
        .arg(scene->humanPlacements.size())
        .arg(scene->scatterPlacements.size())
        .arg(scene->landingPlacements.size())
        .arg(scene->markers.size())
        .arg(scene->objectives.size())
        .arg(scene->regions.size())
        .arg(scene->nativeLayers.size())
        .arg(scene->missions.size())
        .arg(scene->entityTags.size()));
  }
  else
  {
    m_status->setText(
      tr("No native scene loaded. The authoring categories below are planning labels, "
         "not loaded assets. Open a .dahscene to inspect actual native placements."));
  }
  updateNativeBuildings();
  updateNativeProps();
  updateNativeExpansion();
  updateMissionView();
}

void DahMapElements::updateMissionView()
{
  const auto* scene = m_document.dahSceneInfo();
  if (scene == nullptr || m_missionView == nullptr)
    return;
  const auto mission = m_missionView->currentData().toString().toStdString();
  auto visibleLayers = std::optional<std::unordered_set<std::string>>{};
  if (mission != "*")
  {
    visibleLayers.emplace();
    for (const auto& layer : scene->nativeLayers)
    {
      const auto shared = layer.missionOwners.empty();
      const auto owned = std::ranges::find(layer.missionOwners, mission)
                         != layer.missionOwners.end();
      if (shared || (!mission.empty() && owned))
        visibleLayers->insert(layer.hash);
    }
  }
  for (auto lod = 0u; lod < 2u; ++lod)
    if (auto* renderer = m_document.dahSceneRenderer(lod))
      setDahSceneReferenceLayerFilter(*renderer, visibleLayers);
  for (auto* view : window()->findChildren<MapViewBase*>())
    view->refreshViews();
}

void DahMapElements::updateNativeBuildings()
{
  if (m_buildingsTree == nullptr)
  {
    return;
  }
  if (m_nativeBuildingsRoot != nullptr)
  {
    delete m_buildingsTree->takeTopLevelItem(
      m_buildingsTree->indexOfTopLevelItem(m_nativeBuildingsRoot));
    m_nativeBuildingsRoot = nullptr;
  }
  const auto* scene = m_document.dahSceneInfo();
  if (scene == nullptr)
  {
    return;
  }

  m_nativeBuildingsRoot = new QTreeWidgetItem{
    m_buildingsTree,
    {tr("%1 Native Placements").arg(QString::fromStdString(scene->mapName)),
     tr("Retail world.instance-database")}};
  auto font = m_nativeBuildingsRoot->font(0);
  font.setBold(true);
  m_nativeBuildingsRoot->setFont(0, font);

  appendNativePlacements(*m_nativeBuildingsRoot, scene->buildingPlacements);
}

void DahMapElements::updateNativeProps()
{
  if (m_propsTree == nullptr)
  {
    return;
  }
  if (m_nativePropsRoot != nullptr)
  {
    delete m_propsTree->takeTopLevelItem(
      m_propsTree->indexOfTopLevelItem(m_nativePropsRoot));
    m_nativePropsRoot = nullptr;
  }
  const auto* scene = m_document.dahSceneInfo();
  if (scene == nullptr)
  {
    return;
  }

  m_nativePropsRoot = new QTreeWidgetItem{
    m_propsTree,
    {tr("%1 Native Placements").arg(QString::fromStdString(scene->mapName)),
     tr("Retail world.instance-database")}};
  auto font = m_nativePropsRoot->font(0);
  font.setBold(true);
  m_nativePropsRoot->setFont(0, font);

  appendNativePlacements(*m_nativePropsRoot, scene->propPlacements);
}

void DahMapElements::updateNativeExpansion()
{
  for (auto* root : m_nativeExpansionRoots)
  {
    delete root;
  }
  m_nativeExpansionRoots.clear();
  const auto* scene = m_document.dahSceneInfo();
  if (scene == nullptr)
  {
    updateFilter(m_filter->text());
    return;
  }
  const auto populate = [&](
                          const size_t tab,
                          const QString& title,
                          const std::vector<DahObjectPlacementInfo>& placements,
                          const bool annotations = false) {
    if (placements.empty())
    {
      return;
    }
    auto* root = new QTreeWidgetItem{
      m_trees.at(tab),
      {title,
       (annotations ? tr("%1 editor annotations") : tr("%1 native records"))
         .arg(placements.size())}};
    m_nativeExpansionRoots.push_back(root);
    appendNativePlacements(*root, placements);
  };
  populate(1, tr("Native Foliage & Static Scatter"), scene->scatterPlacements);
  populate(3, tr("Native Human Placements"), scene->humanPlacements);
  populate(3, tr("Native Animal Placements"), scene->animalPlacements);
  populate(3, tr("Native Vehicle Placements"), scene->vehiclePlacements);
  populate(3, tr("Native Robots & Powersuits"), scene->robotPlacements);
  populate(4, tr("Native Emplacements & Defenses"), scene->emplacementPlacements);
  auto probes = std::vector<DahObjectPlacementInfo>{};
  auto debris = std::vector<DahObjectPlacementInfo>{};
  auto otherPickups = std::vector<DahObjectPlacementInfo>{};
  for (const auto& pickup : scene->pickupPlacements)
  {
    auto& destination = pickup.assetName == "pu_probe"        ? probes
                        : pickup.assetName == "pu_ufo_debris" ? debris
                                                              : otherPickups;
    destination.push_back(pickup);
  }
  populate(4, tr("Collectible Probes (authored placements)"), probes);
  populate(4, tr("Mission Debris Pickups (authored placements)"), debris);
  populate(4, tr("Native Health & Ammo Pickups"), otherPickups);
  populate(
    4,
    tr("Native Population Definitions (not placed humans)"),
    scene->populationPlacements);
  populate(4, tr("Other Native World Records"), scene->otherWorldPlacements);
  populate(4, tr("Native Landing Site Visuals"), scene->landingPlacements);
  if (!scene->regions.empty())
  {
    auto* root = new QTreeWidgetItem{
      m_trees.at(1),
      {tr("Native Spatial Regions"),
       tr("%1 verified retail region records; overlays remain hidden until boundary "
          "semantics are proven")
         .arg(scene->regions.size())}};
    m_nativeExpansionRoots.push_back(root);
    appendRegions(*root, scene->regions);
  }
  if (!scene->nativeLayers.empty())
  {
    auto* root = new QTreeWidgetItem{
      m_trees.at(4),
      {tr("Native Mission / Load Layers"),
       tr("%1 layers; authored membership shown without simulating activation")
         .arg(scene->nativeLayers.size())}};
    m_nativeExpansionRoots.push_back(root);
    appendLayers(*root, scene->nativeLayers);
  }
  if (!scene->missions.empty())
  {
    auto* root = new QTreeWidgetItem{
      m_trees.at(4),
      {tr("Mission Ownership Workspaces"),
       tr("%1 registrations; shared objects are referenced, never copied")
         .arg(scene->missions.size())}};
    m_nativeExpansionRoots.push_back(root);
    appendMissionWorkspaces(*root, *scene);
  }
  if (!scene->entityTags.empty())
  {
    auto* root = new QTreeWidgetItem{
      m_trees.at(4),
      {tr("Native Entity Tags"),
       tr("%1 tag groups with stable entity membership").arg(scene->entityTags.size())}};
    m_nativeExpansionRoots.push_back(root);
    appendEntityTags(*root, scene->entityTags);
  }
  auto* environmentRoot = new QTreeWidgetItem{
    m_trees.at(5),
    {tr("Native Environment Preview"),
     tr("Authored data inspection, not a live game lighting state")}};
  m_nativeExpansionRoots.push_back(environmentRoot);
  new QTreeWidgetItem{
    environmentRoot,
    {tr("Sky & authored sun"),
     tr("Native sky and authored sun are supported when the scene provides verified "
        "render contracts.")}};
  new QTreeWidgetItem{
    environmentRoot,
    {tr("Unresolved runtime presentation"),
     tr("Live fog, local lights, reflections, shadows and mission-driven effects remain "
        "incomplete; they are not implied by this preview.")}};
  environmentRoot->setExpanded(true);
  auto* trafficRoot = new QTreeWidgetItem{
    m_trees.at(5),
    {tr("Native Traffic & Route Representation"),
     tr("Vehicles, spawn recipes and route geometry remain separate systems")}};
  m_nativeExpansionRoots.push_back(trafficRoot);
  new QTreeWidgetItem{
    trafficRoot,
    {tr("Fixed / authored vehicle actors"),
     tr("%1 genuine placements rendered with their vehicle models")
       .arg(scene->vehiclePlacements.size())}};
  new QTreeWidgetItem{
    trafficRoot,
    {tr("Directed spawn recipes"),
     tr("%1 definitions, including %2 verified road-mode recipes; these are not fixed "
        "vehicle positions")
       .arg(scene->directedSpawnRecipeCount)
       .arg(scene->directedRoadRecipeCount)}};
  new QTreeWidgetItem{
    trafficRoot,
    {tr("Traffic route polylines"),
     tr("0 decoded. No spline is drawn from guessed vehicle positions; native route "
        "vertices, direction and connections remain required.")}};
  trafficRoot->setExpanded(true);
  auto* coverageRoot = new QTreeWidgetItem{
    m_trees.at(5),
    {tr("Area 42 Support Coverage"),
     tr("What this preview proves, preserves, and still lacks")}};
  m_nativeExpansionRoots.push_back(coverageRoot);
  new QTreeWidgetItem{
    coverageRoot,
    {tr("Native scene geometry"),
     tr("92 terrain placements with both LODs; every fixed renderable world placement "
        "is included")}};
  new QTreeWidgetItem{
    coverageRoot,
    {tr("Native organization"),
     tr("%1 regions, %2 mission/load layers and %3 entity tags are inspectable")
       .arg(scene->regions.size())
       .arg(scene->nativeLayers.size())
       .arg(scene->entityTags.size())}};
  new QTreeWidgetItem{
    coverageRoot,
    {tr("Editing / saving"),
     tr("Read-only reconstruction. Collision, destruction graphs, mission execution, "
        "streaming simulation and native package writes are not enabled.")}};
  if (!scene->unresolvedSystems.empty())
  {
    auto* unresolved = new QTreeWidgetItem{
      coverageRoot,
      {tr("Known unresolved systems"),
       tr("%1 explicit evidence gaps").arg(scene->unresolvedSystems.size())}};
    for (const auto& issue : scene->unresolvedSystems)
      new QTreeWidgetItem{unresolved, {tr("Open research item"), text(issue)}};
  }
  coverageRoot->setExpanded(true);
  populate(
    4,
    tr("Landing, Spawn & Objective Annotations (editor markers)"),
    scene->markers,
    true);
  if (!scene->objectives.empty())
  {
    auto* root = new QTreeWidgetItem{
      m_trees.at(4),
      {tr("Mission Objectives (native script definitions)"),
       tr("%1 definitions; no mission execution or invented placements")
         .arg(scene->objectives.size())}};
    m_nativeExpansionRoots.push_back(root);
    appendObjectives(*root, scene->objectives);
  }
  updateFilter(m_filter->text());
}

void DahMapElements::updateFilter(const QString& text)
{
  const auto filter = text.trimmed();
  for (auto* tree : m_trees)
  {
    for (auto collectionIndex = 0; collectionIndex < tree->topLevelItemCount();
         ++collectionIndex)
    {
      auto* collection = tree->topLevelItem(collectionIndex);
      if (
        collection->data(0, AuthoringTaxonomyRole).toBool()
        && m_document.dahSceneInfo() != nullptr)
      {
        collection->setHidden(true);
        continue;
      }
      filterTreeItem(*collection, filter, false);
    }
  }
}

} // namespace tb::ui
