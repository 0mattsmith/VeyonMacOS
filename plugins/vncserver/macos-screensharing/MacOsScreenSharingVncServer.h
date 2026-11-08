/*
 * MacOsScreenSharingVncServer.h - VNC server plugin using the built-in Screen Sharing service of macOS
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
#include "MacOsScreenSharingConfiguration.h"

/*!
 * \brief Uses Apple's Screen Sharing service (screensharingd, VNC on port 5900) as VNC server
 *
 * Screen Sharing captures the screen with system privileges, so no Screen Recording permission
 * has to be granted to Veyon. The Veyon Server connects to it locally using legacy VNC password
 * authentication ("VNC viewers may control screen with password"). If managed automatically, a
 * random password is configured each time the Veyon Server starts.
 */
class MacOsScreenSharingVncServer : public QObject, VncServerPluginInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.MacOsScreenSharingVncServer")
	Q_INTERFACES(PluginInterface VncServerPluginInterface)
public:
	explicit MacOsScreenSharingVncServer( QObject* parent = nullptr );

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("5c1e0b0a-7c55-4b1d-9a65-2f8d1c3e4a71") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral( "MacOsScreenSharingVncServer" );
	}

	QString description() const override
	{
		return tr( "macOS Screen Sharing (built-in VNC server)" );
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

	QWidget* configurationWidget() override;

	void prepareServer() override;

	bool runServer( int serverPort, const Password& password ) override;

	int configuredServerPort() override;

	Password configuredPassword() override;

	static bool isScreenSharingRunning();
	static bool setLegacyVncPassword( const QByteArray& password );
	static bool enableScreenSharing();

	/*!
	 * \brief Saves the Screen Sharing settings found before Veyon changed them for the first time, so they can be
	 * restored when Veyon is uninstalled (see macos/uninstall.sh)
	 */
	static void saveOriginalState();

private:
	// legacy VNC authentication uses DES which only supports 8 characters
	static constexpr auto MaximumPasswordLength = 8;
	static constexpr auto ScreenSharingCheckInterval = 60000;

	static QByteArray generatePassword();

	MacOsScreenSharingConfiguration m_configuration;
	QByteArray m_generatedPassword;

};
