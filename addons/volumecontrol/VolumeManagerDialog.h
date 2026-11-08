/*
 * VolumeManagerDialog.h - declaration of VolumeManagerDialog class
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
#include <QElapsedTimer>
#include <QTimer>

#include "VolumeControlPlugin.h"

class QCheckBox;
class QGridLayout;
class QLabel;
class QSlider;
class QToolButton;

class VolumeManagerDialog : public QDialog
{
	Q_OBJECT
public:
	VolumeManagerDialog( VolumeControlPlugin& plugin, const ComputerControlInterfaceList& computerControlInterfaces,
						 QWidget* parent = nullptr );
	~VolumeManagerDialog() override;

	void setComputerControlInterfaces( const ComputerControlInterfaceList& computerControlInterfaces );

private:
	static constexpr auto RefreshInterval = 3000;
	static constexpr auto UserInteractionHoldTime = 2500;

	struct Row
	{
		ComputerControlInterface::Pointer computer;
		QLabel* nameLabel{nullptr};
		QSlider* slider{nullptr};
		QLabel* levelLabel{nullptr};
		QToolButton* muteButton{nullptr};
		QToolButton* lockButton{nullptr};
		QElapsedTimer lastInteraction;
	};

	void rebuildRows();
	void refresh();
	void handleState( ComputerControlInterface::Pointer computerControlInterface, const VolumeControlPlugin::VolumeState& state );
	Row* findRow( const ComputerControlInterface::Pointer& computerControlInterface );
	ComputerControlInterfaceList connectedComputers() const;

	VolumeControlPlugin& m_plugin;
	ComputerControlInterfaceList m_computerControlInterfaces;
	QList<Row*> m_rows;

	QGridLayout* m_rowsLayout{nullptr};
	QSlider* m_allSlider{nullptr};
	QLabel* m_allLevelLabel{nullptr};
	QCheckBox* m_persistentCheckBox{nullptr};
	QTimer m_refreshTimer{this};

};
