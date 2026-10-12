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

#include "base/NotifierConnection.h"
#include "ui/TabBook.h"

#include <string_view>
#include <vector>

class QLabel;
class QComboBox;
class QLineEdit;
class QString;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;

namespace tb::ui
{
class MapDocument;

enum class DahMapElementTab
{
  Buildings,
  World,
  Props,
  Actors,
  Gameplay,
  Systems,
};

struct DahMapElementGroup
{
  DahMapElementTab tab;
  std::string_view collection;
  std::string_view id;
  std::string_view title;
  std::string_view purpose;
  std::vector<std::string_view> contents;
  bool humanStructure = false;
};

const std::vector<DahMapElementGroup>& dahMapElementGroups();
std::string_view dahMapElementTabTitle(DahMapElementTab tab);

class DahMapElements : public TabBookPage
{
private:
  MapDocument& m_document;
  QLabel* m_status = nullptr;
  QComboBox* m_missionView = nullptr;
  QLineEdit* m_filter = nullptr;
  QTabWidget* m_tabs = nullptr;
  std::vector<QTreeWidget*> m_trees;
  QTreeWidget* m_buildingsTree = nullptr;
  QTreeWidget* m_propsTree = nullptr;
  QTreeWidgetItem* m_nativeBuildingsRoot = nullptr;
  QTreeWidgetItem* m_nativePropsRoot = nullptr;
  std::vector<QTreeWidgetItem*> m_nativeExpansionRoots;
  NotifierConnection m_notifierConnection;

public:
  explicit DahMapElements(MapDocument& document, QWidget* parent = nullptr);

private:
  void createGui();
  QWidget* createTab(DahMapElementTab tab);
  void connectObservers();
  void updateStatus();
  void updateNativeBuildings();
  void updateNativeProps();
  void updateNativeExpansion();
  void updateMissionView();
  void updateFilter(const QString& text);
};

} // namespace tb::ui
