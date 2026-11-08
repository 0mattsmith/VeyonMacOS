/*
 * TaskManagerDialog.h - declaration of TaskManagerDialog class
 *
 * Copyright (c) 2026 Veyon Community
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

#include <QDialog>
#include <QJsonArray>
#include <QTimer>

#include "ApplicationRules.h"
#include "ComputerControlInterface.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QRadioButton;
class QTabWidget;
class QTreeWidget;

class TaskManagerPlugin;
class VeyonMasterInterface;

class TaskManagerDialog : public QDialog
{
	Q_OBJECT
public:
	TaskManagerDialog( TaskManagerPlugin& plugin, VeyonMasterInterface& master,
					   const ComputerControlInterfaceList& computerControlInterfaces, QWidget* parent = nullptr );
	~TaskManagerDialog() override = default;

	void setComputerControlInterfaces( const ComputerControlInterfaceList& computerControlInterfaces );

private:
	static constexpr auto AutoRefreshInterval = 3000;

	enum ProcessColumn
	{
		ColumnName,
		ColumnPid,
		ColumnUser,
		ColumnCpu,
		ColumnMemory,
		ColumnTitle,
		ColumnCount
	};

	QWidget* createProcessesPage();
	QWidget* createRulesPage();

	void updateComputerList();
	ComputerControlInterface::Pointer currentComputer() const;
	ComputerControlInterfaceList selectedComputers() const;

	void refresh();
	void updateProcessTree();
	void endSelectedProcesses( bool force );
	void endProcessesOnAllComputers();
	void addSelectedProcessesToRules( ApplicationRules::Mode mode );
	QList<ProcessInfo> selectedProcesses() const;

	ApplicationRules rulesFromUi() const;
	void setRulesInUi( const ApplicationRules& rules );
	void applyRulesToComputers();
	void removeRulesFromComputers();

	void loadPresets();
	void savePreset();
	void deletePreset();
	void loadPreset( int index );
	QJsonArray userPresets() const;
	void setUserPresets( const QJsonArray& presets );

	void handleProcessList( ComputerControlInterface::Pointer computerControlInterface,
							const QString& platform, const ProcessInfoList& processes );
	void handleRulesState( ComputerControlInterface::Pointer computerControlInterface,
						   const ApplicationRules& rules, const QVariantList& blockedApplications );
	void handleTerminationResult( ComputerControlInterface::Pointer computerControlInterface,
								  int terminatedCount, const QStringList& failedNames );

	TaskManagerPlugin& m_plugin;
	VeyonMasterInterface& m_master;
	ComputerControlInterfaceList m_computerControlInterfaces;

	QHash<QString, ApplicationRules> m_rulesStates;
	QHash<QString, QVariantList> m_blockedApplications;
	ProcessInfoList m_processes;
	QString m_platform;

	QListWidget* m_computerList{nullptr};
	QTabWidget* m_tabWidget{nullptr};

	QLineEdit* m_filterEdit{nullptr};
	QCheckBox* m_showAllProcessesCheckBox{nullptr};
	QCheckBox* m_autoRefreshCheckBox{nullptr};
	QTreeWidget* m_processTree{nullptr};
	QLabel* m_processStatusLabel{nullptr};
	QTimer m_refreshTimer{this};

	QComboBox* m_presetComboBox{nullptr};
	QRadioButton* m_modeDisabledButton{nullptr};
	QRadioButton* m_modeBlacklistButton{nullptr};
	QRadioButton* m_modeWhitelistButton{nullptr};
	QPlainTextEdit* m_patternsEdit{nullptr};
	QLineEdit* m_messageEdit{nullptr};
	QCheckBox* m_persistentCheckBox{nullptr};
	QLabel* m_rulesStatusLabel{nullptr};
	QTreeWidget* m_blockedTree{nullptr};

};
