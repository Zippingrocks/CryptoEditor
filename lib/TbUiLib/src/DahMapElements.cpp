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

#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "ui/MapDocument.h"
#include "ui/ViewConstants.h"

#include <array>
#include <map>

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
      }
    }
    classRoot->setExpanded(true);
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
  layout->addWidget(m_filter);
  layout->addWidget(m_tabs, 1);
  setLayout(layout);

  connect(m_filter, &QLineEdit::textChanged, this, &DahMapElements::updateFilter);
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
  if (const auto* scene = m_document.dahSceneInfo())
  {
    m_status->setText(
      tr("%1: %2 world records, including %3 buildings, %4 props and %5 humans; "
         "%6 scatter instances, %7 native landing visuals and %8 editor markers. "
         "Actors use authored reference poses. Native mission layers remain distinct; "
         "this is not a simulated mission state.")
        .arg(QString::fromStdString(scene->mapName))
        .arg(scene->worldPlacementCount)
        .arg(scene->buildingPlacementCount)
        .arg(scene->propPlacementCount)
        .arg(scene->humanPlacements.size())
        .arg(scene->scatterPlacements.size())
        .arg(scene->landingPlacements.size())
        .arg(scene->markers.size()));
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
  populate(4, tr("Native Pickups & Collectibles"), scene->pickupPlacements);
  populate(
    4,
    tr("Native Population Definitions (not placed humans)"),
    scene->populationPlacements);
  populate(4, tr("Other Native World Records"), scene->otherWorldPlacements);
  populate(4, tr("Native Landing Site Visuals"), scene->landingPlacements);
  populate(4, tr("Landing & Spawn Annotations (editor markers)"), scene->markers, true);
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
