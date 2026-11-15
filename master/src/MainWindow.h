/*
 * MainWindow.h - main window of Veyon Master Application
 *
 * Copyright (c) 2004-2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#pragma once

#include <functional>

#include <QMainWindow>

#include "ComputerControlInterface.h"

class QButtonGroup;
class QMenu;
class QToolButton;
class ToolButton;

class VeyonMaster;

namespace Ui {
class MainWindow;
}


class MainWindow : public QMainWindow
{
	Q_OBJECT
public:
	explicit MainWindow( VeyonMaster& masterCore, QWidget* parent = nullptr );
	~MainWindow() override;

	static bool initAuthentication();
	static bool initAccessControl();

	VeyonMaster& masterCore()
	{
		return m_master;
	}

	void reloadSubFeatures();

	// show related features as one toolbar slot (stacked buttons or a menu) instead of one button per feature
	void setFeatureGroupingEnabled( bool enabled );

	ComputerControlInterfaceList selectedComputerControlInterfaces() const;

protected:
	void closeEvent( QCloseEvent* event ) override;
	bool eventFilter( QObject* object, QEvent* event ) override;
	void keyPressEvent( QKeyEvent *e ) override;


private Q_SLOTS:
	void showAboutDialog();
	void showAddOnStore();
	void openConfigurator();

private:
	void addAddOnStoreButton();
#ifdef Q_OS_MACOS
	void setupMacOSMenuBar();
#endif

	static int buttonId(const Feature& feature) // clazy:exclude=qt6-qhash-signature
	{
		return int(qHash(feature.uid()));
	}

	static constexpr const char* originalSizePropertyName()
	{
		return "originalSize";
	}

	static constexpr const char* toolBarShortcutPropertyName()
	{
		return "toolBarShortcut";
	}

	struct ToolBarGroupMember
	{
		QString featureName;
		QString label{};				// label in the toolbar or menu (default: display name of the feature)
		bool inlineSubFeatures{false};	// list sub-features directly in the group menu instead of a submenu
		bool separatorBefore{false};
		Feature feature{};				// filled in if the feature is available
	};

	struct ToolBarGroup
	{
		QString name;					// object name of the group's widget (and its menu)
		QString label{};				// label of a menu-only group button
		QString description{};			// tooltip of a menu-only group button or the drop-down arrow
		QList<ToolBarGroupMember> stackedMembers{};	// up to two features shown as stacked half-height buttons
		QList<ToolBarGroupMember> menuMembers{};	// features listed in the group's drop-down menu
		QStringList menuMemberPrefixes{};			// add all features whose names start with one of these
		QString iconFeature{};			// feature whose icon a menu-only group button shows
		QString iconUrl{};				// icon of a menu-only group button (takes precedence if it exists)
	};

	QList<ToolBarGroup> toolBarGroups() const;

	void populateToolBar();
	void clearToolBar();
	void addFeaturesToToolBar();
	void addGroupedFeaturesToToolBar();
	void addFeatureGroup( const ToolBarGroup& group,
						  const QList<ToolBarGroupMember>& stackedMembers,
						  const QList<ToolBarGroupMember>& menuMembers );
	ToolButton* createFeatureButton( const Feature& feature, const QString& label );
	void addFeatureButton( const Feature& feature );
	QMenu* createGroupMenu( const ToolBarGroup& group, const QList<ToolBarGroupMember>& members, QWidget* parent );
	void addFeatureAction( QMenu* menu, const Feature& feature, const QString& label );
	void addSubFeatureAction( QMenu* menu, const Feature& subFeature, const Feature& parentFeature, QToolButton* parentButton );
	void addSubFeaturesToToolButton( QToolButton* button, const Feature& parentFeature );
	void discardMenu( QMenu* menu );

	void runFeatureFromToolBar( const Feature& feature );
	void updateModeButtons();
	void updateModeButtonGroup();

	void loadComputerPositions();
	void saveComputerPositions();

	Ui::MainWindow* ui;

	VeyonMaster& m_master;

	QButtonGroup* m_modeGroup;

	bool m_featureGroupingEnabled{true};
	QList<QAction *> m_toolBarActions;
	QList<std::function<void()>> m_subFeatureMenuUpdaters;

} ;
