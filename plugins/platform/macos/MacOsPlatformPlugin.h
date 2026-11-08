/*
 * MacOsPlatformPlugin.h - declaration of MacOsPlatformPlugin class
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

#include "PluginInterface.h"
#include "PlatformPluginInterface.h"
#include "MacOsCoreFunctions.h"
#include "MacOsFilesystemFunctions.h"
#include "MacOsInputDeviceFunctions.h"
#include "MacOsNetworkFunctions.h"
#include "MacOsServiceFunctions.h"
#include "MacOsSessionFunctions.h"
#include "MacOsUserFunctions.h"

class MacOsPlatformPlugin : public QObject, PlatformPluginInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.MacOsPlatform")
	Q_INTERFACES(PluginInterface PlatformPluginInterface)
public:
	explicit MacOsPlatformPlugin( QObject* parent = nullptr );
	~MacOsPlatformPlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("8b0f5c6e-3f1d-4a52-9f5e-7d2c4a6b1e93") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral( "MacOsPlatformPlugin" );
	}

	QString description() const override
	{
		return tr( "Plugin implementing abstract functions for the macOS platform" );
	}

	QString vendor() const override
	{
		return QStringLiteral( "Veyon Community" );
	}

	QString copyright() const override
	{
		return QStringLiteral( "Veyon Community" );
	}

	Plugin::Flags flags() const override
	{
		return Plugin::ProvidesDefaultImplementation;
	}

	PlatformCoreFunctions& coreFunctions() override
	{
		return m_macOsCoreFunctions;
	}

	PlatformFilesystemFunctions& filesystemFunctions() override
	{
		return m_macOsFilesystemFunctions;
	}

	PlatformInputDeviceFunctions& inputDeviceFunctions() override
	{
		return m_macOsInputDeviceFunctions;
	}

	PlatformNetworkFunctions& networkFunctions() override
	{
		return m_macOsNetworkFunctions;
	}

	PlatformServiceFunctions& serviceFunctions() override
	{
		return m_macOsServiceFunctions;
	}

	PlatformSessionFunctions& sessionFunctions() override
	{
		return m_macOsSessionFunctions;
	}

	PlatformUserFunctions& userFunctions() override
	{
		return m_macOsUserFunctions;
	}

private:
	MacOsCoreFunctions m_macOsCoreFunctions{};
	MacOsFilesystemFunctions m_macOsFilesystemFunctions{};
	MacOsInputDeviceFunctions m_macOsInputDeviceFunctions{};
	MacOsNetworkFunctions m_macOsNetworkFunctions{};
	MacOsServiceFunctions m_macOsServiceFunctions{};
	MacOsSessionFunctions m_macOsSessionFunctions{};
	MacOsUserFunctions m_macOsUserFunctions{};

};
