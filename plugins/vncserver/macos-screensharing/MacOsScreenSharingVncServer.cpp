/*
 * MacOsScreenSharingVncServer.cpp - VNC server plugin using the built-in Screen Sharing service of macOS
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

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QTimer>

#include <unistd.h>

#include "MacOsScreenSharingConfigurationWidget.h"
#include "MacOsScreenSharingVncServer.h"
#include "VeyonConfiguration.h"


namespace {

const auto ScreenSharingService = QStringLiteral("system/com.apple.screensharing");
const auto ScreenSharingPlist = QStringLiteral("/System/Library/LaunchDaemons/com.apple.screensharing.plist");
const auto VncSettingsFile = QStringLiteral("/Library/Preferences/com.apple.VNCSettings.txt");
const auto RemoteManagementPreferences = QStringLiteral("/Library/Preferences/com.apple.RemoteManagement");
const auto OriginalStateFile = QStringLiteral("/Library/Application Support/Veyon/ScreenSharingOriginalState.json");

int launchctl( const QStringList& arguments )
{
	QProcess process;
	process.setProcessChannelMode( QProcess::MergedChannels );
	process.start( QStringLiteral("/bin/launchctl"), arguments );
	if( process.waitForFinished( 15000 ) == false )
	{
		return -1;
	}
	return process.exitCode();
}

}



MacOsScreenSharingVncServer::MacOsScreenSharingVncServer( QObject* parent ) :
	QObject( parent ),
	m_configuration( &VeyonCore::config() )
{
}



QWidget* MacOsScreenSharingVncServer::configurationWidget()
{
	return new MacOsScreenSharingConfigurationWidget( m_configuration );
}



void MacOsScreenSharingVncServer::prepareServer()
{
	if( m_configuration.manageScreenSharing() == false )
	{
		if( isScreenSharingRunning() == false )
		{
			vWarning() << "Screen Sharing is not enabled - please enable it in System Settings > General > Sharing "
						  "and allow VNC viewers to control the screen with a password";
		}
		return;
	}

	if( geteuid() != 0 )
	{
		vWarning() << "insufficient privileges for configuring Screen Sharing";
		return;
	}

	saveOriginalState();

	if( setLegacyVncPassword( configuredPassword().toByteArray() ) == false ||
		enableScreenSharing() == false )
	{
		vCritical() << "failed to configure Screen Sharing - please enable it manually in System Settings > General > Sharing";
	}
}



bool MacOsScreenSharingVncServer::runServer( int serverPort, const Password& password )
{
	Q_UNUSED(serverPort)
	Q_UNUSED(password)

	// Screen Sharing runs on its own (managed by launchd) - just make sure it stays enabled
	QTimer watchdog;
	connect( &watchdog, &QTimer::timeout, &watchdog, [this]() {
		if( m_configuration.manageScreenSharing() && geteuid() == 0 && isScreenSharingRunning() == false )
		{
			vWarning() << "Screen Sharing has been disabled - enabling it again";
			enableScreenSharing();
		}
	} );
	watchdog.start( ScreenSharingCheckInterval );

	QEventLoop().exec();

	return true;
}



int MacOsScreenSharingVncServer::configuredServerPort()
{
	return m_configuration.serverPort();
}



MacOsScreenSharingVncServer::Password MacOsScreenSharingVncServer::configuredPassword()
{
	const auto configuredPassword = m_configuration.password().plainText();

	if( m_configuration.manageScreenSharing() && configuredPassword.isEmpty() )
	{
		// generate a new password each time the server starts
		if( m_generatedPassword.isEmpty() )
		{
			m_generatedPassword = generatePassword();
		}
		return Password( m_generatedPassword );
	}

	return configuredPassword;
}



bool MacOsScreenSharingVncServer::isScreenSharingRunning()
{
	// the service is socket activated, so being loaded means it accepts connections
	return launchctl( { QStringLiteral("print"), ScreenSharingService } ) == 0;
}



bool MacOsScreenSharingVncServer::setLegacyVncPassword( const QByteArray& password )
{
	// macOS stores the legacy VNC password XOR-obfuscated with a fixed key in a hex encoded text file
	static constexpr unsigned char key[16] = { 0x17, 0x34, 0x51, 0x6e, 0x8b, 0xa8, 0xc5, 0xe2,
											   0xff, 0x1c, 0x39, 0x56, 0x73, 0x90, 0xad, 0xca };

	QByteArray obfuscatedPassword;
	for( int i = 0; i < int(sizeof(key)); ++i )
	{
		const auto c = i < qMin<int>( int(password.size()), MaximumPasswordLength ) ? static_cast<unsigned char>( password.at( i ) ) : 0;
		obfuscatedPassword.append( char( c ^ key[i] ) );
	}

	QSaveFile file( VncSettingsFile );
	if( file.open( QFile::WriteOnly ) == false ||
		file.write( obfuscatedPassword.toHex().toUpper() + '\n' ) < 0 ||
		file.commit() == false )
	{
		vCritical() << "could not write" << VncSettingsFile;
		return false;
	}

	QFile::setPermissions( VncSettingsFile, QFile::ReadOwner | QFile::WriteOwner );

	// enable "VNC viewers may control screen with password"
	return QProcess::execute( QStringLiteral("/usr/bin/defaults"),
							  { QStringLiteral("write"), RemoteManagementPreferences,
								QStringLiteral("VNCLegacyConnectionsEnabled"), QStringLiteral("-bool"), QStringLiteral("YES") } ) == 0;
}



bool MacOsScreenSharingVncServer::enableScreenSharing()
{
	if( isScreenSharingRunning() )
	{
		return true;
	}

	launchctl( { QStringLiteral("enable"), ScreenSharingService } );
	launchctl( { QStringLiteral("bootstrap"), QStringLiteral("system"), ScreenSharingPlist } );

	return isScreenSharingRunning();
}



void MacOsScreenSharingVncServer::saveOriginalState()
{
	// keep the state from before Veyon managed Screen Sharing for the first time
	if( QFile::exists( OriginalStateFile ) )
	{
		return;
	}

	QFile vncSettings( VncSettingsFile );
	const auto vncSettingsFileExisted = vncSettings.open( QFile::ReadOnly );

	QProcess defaults;
	defaults.start( QStringLiteral("/usr/bin/defaults"),
					{ QStringLiteral("read"), RemoteManagementPreferences, QStringLiteral("VNCLegacyConnectionsEnabled") } );
	const auto legacyConnectionsEnabled = defaults.waitForFinished( 10000 ) && defaults.exitCode() == 0 &&
										  defaults.readAllStandardOutput().trimmed() == "1";

	const QJsonObject state{
		{ QStringLiteral("screenSharingRunning"), isScreenSharingRunning() },
		{ QStringLiteral("legacyConnectionsEnabled"), legacyConnectionsEnabled },
		{ QStringLiteral("vncSettingsFileExisted"), vncSettingsFileExisted },
		{ QStringLiteral("vncSettings"), QString::fromLatin1( vncSettingsFileExisted ? vncSettings.readAll().toBase64() : QByteArray() ) },
	};

	QDir().mkpath( QFileInfo( OriginalStateFile ).absolutePath() );
	QSaveFile file( OriginalStateFile );
	if( file.open( QFile::WriteOnly ) == false ||
		file.write( QJsonDocument( state ).toJson() ) < 0 ||
		file.commit() == false )
	{
		vWarning() << "could not save original Screen Sharing state to" << OriginalStateFile;
		return;
	}

	// the file contains the (obfuscated) previous VNC password
	QFile::setPermissions( OriginalStateFile, QFile::ReadOwner | QFile::WriteOwner );
}



QByteArray MacOsScreenSharingVncServer::generatePassword()
{
	static const char characters[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";

	QByteArray password;
	for( int i = 0; i < MaximumPasswordLength; ++i )
	{
		password.append( characters[QRandomGenerator::system()->bounded( int(sizeof(characters)) - 1 )] );
	}

	return password;
}


IMPLEMENT_CONFIG_PROXY(MacOsScreenSharingConfiguration)
