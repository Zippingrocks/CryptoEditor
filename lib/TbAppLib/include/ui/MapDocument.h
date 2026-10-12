/*
 Copyright (C) 2010 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "base/Notifier.h"
#include "base/NotifierConnection.h"
#include "mdl/PointTrace.h"

#include "vm/bbox.h"
#include "vm/polygon.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace kdl
{
class task_manager;
} // namespace kdl

namespace tb
{
class Logger;
class LoggingHub;

namespace gl
{
class ResourceId;
class ResourceManager;
} // namespace gl

namespace mdl
{
class Autosaver;
class Command;
class LayerNode;
class Map;
class Node;
class PickResult;
class UndoableCommand;

enum class MapFormat;
enum class VisualEffect;

struct EnvironmentConfig;
struct GameInfo;
struct SelectionChange;
} // namespace mdl

namespace render
{
class DirectRenderable;
class MapRenderer;
} // namespace render

namespace ui
{
class AsyncTaskRunner;
class CachingLogger;

struct PointFile
{
  mdl::PointTrace trace;
  std::filesystem::path path;
};

struct PortalFile
{
  std::vector<vm::polygon3f> portals;
  std::filesystem::path path;
};

struct DahObjectPlacementInfo
{
  std::string id;
  std::string nativeClass;
  std::string assetName;
  std::array<double, 3> position{};
  bool renderable = false;
  std::string status;
  std::string nativeLayer;
  std::string nativeList;
  std::optional<std::int64_t> entityId;
  std::vector<std::string> tagIds;
};

struct DahRegionInfo
{
  std::string id;
  std::string name;
  std::string nameHash;
  std::string nativeLayerHash;
  std::array<double, 3> position{};
};

struct DahLayerInfo
{
  std::string hash;
  std::string name;
  std::uint64_t index = 0;
  std::uint64_t loadGroup = 0;
  std::uint64_t placementCount = 0;
  std::uint64_t scriptCallCount = 0;
};

struct DahEntityTagInfo
{
  std::string id;
  std::string name;
  std::string nameHash;
  std::vector<std::int64_t> entityIds;
  std::vector<std::string> placementIds;
};

struct DahObjectiveTargetInfo
{
  std::string kind;
  std::string name;
  std::string status;
  std::optional<std::array<double, 3>> position;
  std::vector<std::string> worldPlacementIds;
  std::vector<std::string> nativeLayers;
};

struct DahObjectiveInfo
{
  std::string id;
  std::string mission;
  std::string name;
  std::string type;
  std::string status;
  std::string source;
  std::vector<DahObjectiveTargetInfo> targets;
  std::vector<std::string> layerCommands;
};

struct DahSceneInfo
{
  std::filesystem::path path;
  std::string profileId;
  std::string mapName;
  vm::bbox3d bounds;
  std::uint64_t placementCount = 0;
  std::uint64_t lodCount = 0;
  std::uint64_t geometryCount = 0;
  std::uint64_t worldPlacementCount = 0;
  std::uint64_t buildingPlacementCount = 0;
  std::uint64_t propPlacementCount = 0;
  std::array<std::uint64_t, 2> nativeTriangleCounts{};
  std::vector<DahObjectPlacementInfo> buildingPlacements;
  std::vector<DahObjectPlacementInfo> propPlacements;
  std::vector<DahObjectPlacementInfo> humanPlacements;
  std::vector<DahObjectPlacementInfo> scatterPlacements;
  std::vector<DahObjectPlacementInfo> markers;
  std::vector<DahObjectPlacementInfo> vehiclePlacements;
  std::vector<DahObjectPlacementInfo> animalPlacements;
  std::vector<DahObjectPlacementInfo> robotPlacements;
  std::vector<DahObjectPlacementInfo> emplacementPlacements;
  std::vector<DahObjectPlacementInfo> pickupPlacements;
  std::vector<DahObjectPlacementInfo> populationPlacements;
  std::vector<DahObjectPlacementInfo> otherWorldPlacements;
  std::vector<DahObjectPlacementInfo> landingPlacements;
  std::vector<DahObjectiveInfo> objectives;
  std::vector<DahRegionInfo> regions;
  std::vector<DahLayerInfo> nativeLayers;
  std::vector<DahEntityTagInfo> entityTags;
  std::vector<std::string> unresolvedSystems;
};

class MapDocument
{
public:
  static const vm::bbox3d DefaultWorldBounds;

  Notifier<> documentWasLoadedNotifier;
  Notifier<> documentWasSavedNotifier;
  Notifier<> documentDidChangeNotifier;

  Notifier<> modificationStateDidChangeNotifier;

  Notifier<> editorContextDidChangeNotifier;
  Notifier<> currentLayerDidChangeNotifier;
  Notifier<> currentMaterialNameDidChangeNotifier;

  Notifier<> selectionWillChangeNotifier;
  Notifier<const mdl::SelectionChange&> selectionDidChangeNotifier;

  Notifier<const std::vector<mdl::Node*>&> nodesWereAddedNotifier;
  Notifier<const std::vector<mdl::Node*>&> nodesWillBeRemovedNotifier;
  Notifier<const std::vector<mdl::Node*>&> nodesWereRemovedNotifier;
  Notifier<const std::vector<mdl::Node*>&> nodesWillChangeNotifier;
  Notifier<const std::vector<mdl::Node*>&> nodesDidChangeNotifier;

  Notifier<const std::vector<mdl::Node*>&> nodeVisibilityDidChangeNotifier;
  Notifier<const std::vector<mdl::Node*>&> nodeLockingDidChangeNotifier;

  Notifier<> groupWasOpenedNotifier;
  Notifier<> groupWasClosedNotifier;

  Notifier<const std::vector<gl::ResourceId>&> resourcesWereProcessedNotifier;

  Notifier<> materialCollectionsWillChangeNotifier;
  Notifier<> materialCollectionsDidChangeNotifier;

  Notifier<> materialUsageCountsDidChangeNotifier;

  Notifier<> entityDefinitionsWillChangeNotifier;
  Notifier<> entityDefinitionsDidChangeNotifier;

  Notifier<> modsWillChangeNotifier;
  Notifier<> modsDidChangeNotifier;

  Notifier<mdl::VisualEffect> triggerVisualEffectNotifier;

  Notifier<> gridDidChangeNotifier;

  Notifier<mdl::Command&> commandDoNotifier;
  Notifier<mdl::Command&> commandDoneNotifier;
  Notifier<mdl::Command&> commandDoFailedNotifier;
  Notifier<mdl::UndoableCommand&> commandUndoNotifier;
  Notifier<mdl::UndoableCommand&> commandUndoneNotifier;
  Notifier<mdl::UndoableCommand&> commandUndoFailedNotifier;
  Notifier<const std::string&, bool, bool> transactionDoneNotifier;
  Notifier<const std::string&, bool, bool> transactionUndoneNotifier;

  Notifier<> pointFileWasLoadedNotifier;
  Notifier<> pointFileWasUnloadedNotifier;

  Notifier<> portalFileWasLoadedNotifier;
  Notifier<> portalFileWasUnloadedNotifier;

private:
  // pointer so that MapDocument can be moveable
  kdl::task_manager* m_taskManager;
  gl::ResourceManager* m_resourceManager;
  std::unique_ptr<LoggingHub> m_loggingHub;

  std::unique_ptr<mdl::Map> m_map;
  std::unique_ptr<mdl::Autosaver> m_autosaver;

  std::optional<PointFile> m_pointFile;
  std::optional<PortalFile> m_portalFile;

  std::unique_ptr<render::MapRenderer> m_mapRenderer;
  std::optional<DahSceneInfo> m_dahSceneInfo;
  mdl::LayerNode* m_dahSceneLod0Layer = nullptr;
  mdl::LayerNode* m_dahSceneLod1Layer = nullptr;
  std::unique_ptr<render::DirectRenderable> m_dahSceneLod0Renderer;
  std::unique_ptr<render::DirectRenderable> m_dahSceneLod1Renderer;
  mdl::LayerNode* m_dahSceneMarkerLayer = nullptr;
  std::unique_ptr<render::DirectRenderable> m_dahSceneMarkerRenderer;
  mdl::LayerNode* m_dahSceneRegionLayer = nullptr;
  std::unique_ptr<render::DirectRenderable> m_dahSceneRegionRenderer;

  NotifierConnection m_notifierConnection;

public:
  explicit MapDocument(
    kdl::task_manager& taskManager, gl::ResourceManager& resourceManager);

  MapDocument(MapDocument&&) noexcept;
  MapDocument& operator=(MapDocument&&) noexcept;

  static Result<std::unique_ptr<MapDocument>> createDocument(
    const mdl::EnvironmentConfig& environmentConfig,
    const mdl::GameInfo& gameInfo,
    mdl::MapFormat mapFormat,
    const vm::bbox3d& worldBounds,
    kdl::task_manager& taskManager,
    gl::ResourceManager& resourceManager);

  static Result<std::unique_ptr<MapDocument>> loadDocument(
    const mdl::EnvironmentConfig& environmentConfig,
    const mdl::GameInfo& gameInfo,
    mdl::MapFormat mapFormat,
    const vm::bbox3d& worldBounds,
    std::filesystem::path path,
    kdl::task_manager& taskManager,
    gl::ResourceManager& resourceManager);

  ~MapDocument();

  Result<void> create(
    const mdl::EnvironmentConfig& environmentConfig,
    const mdl::GameInfo& gameInfo,
    mdl::MapFormat mapFormat,
    const vm::bbox3d& worldBounds);

  Result<void> load(
    const mdl::EnvironmentConfig& environmentConfig,
    const mdl::GameInfo& gameInfo,
    mdl::MapFormat mapFormat,
    const vm::bbox3d& worldBounds,
    std::filesystem::path path);

  Result<void> reload();

  void triggerAutosave();

private:
  void setMap(std::unique_ptr<mdl::Map>);

  void updateMapFromPreferences();

public: // accessors and such
  mdl::Map& map();
  const mdl::Map& map() const;

  render::MapRenderer& mapRenderer();
  const render::MapRenderer& mapRenderer() const;

  void setDahSceneReference(
    DahSceneInfo info,
    mdl::LayerNode* lod0Layer,
    std::unique_ptr<render::DirectRenderable> lod0Renderer,
    mdl::LayerNode* lod1Layer,
    std::unique_ptr<render::DirectRenderable> lod1Renderer,
    mdl::LayerNode* markerLayer = nullptr,
    std::unique_ptr<render::DirectRenderable> markerRenderer = nullptr,
    mdl::LayerNode* regionLayer = nullptr,
    std::unique_ptr<render::DirectRenderable> regionRenderer = nullptr);
  const DahSceneInfo* dahSceneInfo() const;
  mdl::LayerNode* dahSceneLayer(std::uint64_t lodIndex) const;
  render::DirectRenderable* dahSceneRenderer(std::uint64_t lodIndex) const;
  std::optional<std::uint64_t> dahSceneLodForLayer(const mdl::LayerNode* layer) const;
  bool isDahSceneLayer(const mdl::LayerNode* layer) const;
  bool isDahScene() const;

  Logger& logger();
  void setTargetLogger(Logger* parentLogger);

public: // point file management
  mdl::PointTrace* pointTrace();
  void loadPointFile(std::filesystem::path path);
  bool isPointFileLoaded() const;
  bool canReloadPointFile() const;
  void reloadPointFile();
  void unloadPointFile();

public: // portal file management
  const std::vector<vm::polygon3f>* portals() const;
  void loadPortalFile(std::filesystem::path path);
  bool isPortalFileLoaded() const;
  bool canReloadPortalFile() const;
  void reloadPortalFile();
  void unloadPortalFile();

private: // observers
  void connectObservers();
  void connectMapObservers();

  void transactionDone(const std::string& name, bool observable, bool isModification);
  void transactionUndone(const std::string& name, bool observable, bool isModification);
  void documentWasLoaded();
  void documentWasCleared();
  void preferenceDidChange(const std::filesystem::path& preferencePath);
};

} // namespace ui
} // namespace tb
