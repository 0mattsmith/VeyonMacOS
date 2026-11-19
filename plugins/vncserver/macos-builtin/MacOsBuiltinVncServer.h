/*
 * MacOsBuiltinVncServer.h - declaration of MacOsBuiltinVncServer class
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
#include "VncServerPluginInterface.h"

/*!
 * \brief VNC server capturing the screen of the current macOS session itself (ScreenCaptureKit) and injecting
 * input events (CGEvent) - unlike Apple's Screen Sharing it always shows the session of the logged on user, which
 * Screen Sharing refuses to do for connections authenticated with the VNC password only
 */
class MacOsBuiltinVncServer : public QObject, VncServerPluginInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.MacOsBuiltinVncServer")
	Q_INTERFACES(PluginInterface VncServerPluginInterface)
public:
	explicit MacOsBuiltinVncServer( QObject* parent = nullptr );

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("8a9dcc08-4d29-4db4-966a-708696fee299") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral( "MacOsBuiltinVncServer" );
	}

	QString description() const override
	{
		return tr( "Builtin VNC server (macOS)" );
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

	QStringList supportedSessionTypes() const override
	{
		return {};
	}

	QWidget* configurationWidget() override
	{
		return nullptr;
	}

	void prepareServer() override;

	bool runServer( int serverPort, const Password& password ) override;

	int configuredServerPort() override
	{
		return -1;
	}

	Password configuredPassword() override
	{
		return {};
	}

private:
	static constexpr auto FramesPerSecond = 30;

	static void rfbLogDebug( const char* format, ... );
	static void rfbLogNone( const char* format, ... );

};
