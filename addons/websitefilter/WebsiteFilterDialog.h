/*
 * WebsiteFilterDialog.h - declaration of WebsiteFilterDialog class
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
#include <QHash>
#include <QJsonArray>

#include "ComputerControlInterface.h"
#include "WebsiteRules.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QRadioButton;

class VeyonMasterInterface;
class WebsiteFilterPlugin;

class WebsiteFilterDialog : public QDialog
{
	Q_OBJECT
public:
	WebsiteFilterDialog( WebsiteFilterPlugin& plugin, VeyonMasterInterface& master,
						 const ComputerControlInterfaceList& computerControlInterfaces, QWidget* parent = nullptr );
	~WebsiteFilterDialog() override = default;

	void setComputerControlInterfaces( const ComputerControlInterfaceList& computerControlInterfaces );

private:
	struct ComputerState
	{
		WebsiteRules rules;
		bool active{false};
		QString error;
	};

	QWidget* createRulesPanel();

	void updateComputerList();
	ComputerControlInterface::Pointer currentComputer() const;
	ComputerControlInterfaceList selectedComputers() const;
	void updateStatus();

	WebsiteRules rulesFromUi() const;
	void setRulesInUi( const WebsiteRules& rules );
	void updateOptions();
	void applyRulesToComputers();
	void removeRulesFromComputers();

	void loadPresets();
	void savePreset();
	void deletePreset();
	void loadPreset( int index );
	QJsonArray userPresets() const;
	void setUserPresets( const QJsonArray& presets );

	void handleState( ComputerControlInterface::Pointer computerControlInterface,
					  const WebsiteRules& rules, bool active, const QString& error );

	WebsiteFilterPlugin& m_plugin;
	VeyonMasterInterface& m_master;
	ComputerControlInterfaceList m_computerControlInterfaces;
	QHash<QString, ComputerState> m_states;

	QListWidget* m_computerList{nullptr};
	QComboBox* m_presetComboBox{nullptr};
	QRadioButton* m_modeDisabledButton{nullptr};
	QRadioButton* m_modeBlacklistButton{nullptr};
	QRadioButton* m_modeWhitelistButton{nullptr};
	QLabel* m_domainsHintLabel{nullptr};
	QPlainTextEdit* m_domainsEdit{nullptr};
	QCheckBox* m_hostsFileCheckBox{nullptr};
	QCheckBox* m_closeBrowsersCheckBox{nullptr};
	QCheckBox* m_keepAfterRestartCheckBox{nullptr};
	QLabel* m_statusLabel{nullptr};

};
