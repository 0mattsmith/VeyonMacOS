/*
 * MacOsScreenSharingConfigurationWidget.h - configuration widget for macOS Screen Sharing VNC server plugin
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

#include <QWidget>

class QCheckBox;
class QLineEdit;
class QSpinBox;
class MacOsScreenSharingConfiguration;

class MacOsScreenSharingConfigurationWidget : public QWidget
{
	Q_OBJECT
public:
	explicit MacOsScreenSharingConfigurationWidget( MacOsScreenSharingConfiguration& configuration, QWidget* parent = nullptr );
	~MacOsScreenSharingConfigurationWidget() override = default;

private:
	MacOsScreenSharingConfiguration& m_configuration;

	QCheckBox* m_manageScreenSharingCheckBox{nullptr};
	QSpinBox* m_serverPortSpinBox{nullptr};
	QLineEdit* m_passwordEdit{nullptr};

};
