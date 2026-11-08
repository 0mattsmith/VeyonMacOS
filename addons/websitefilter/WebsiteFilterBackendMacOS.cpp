/*
 * WebsiteFilterBackendMacOS.cpp - applies website rules on macOS
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

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>

#include "WebsiteFilterBackend.h"


namespace {

// Chromium based browsers read policies from managed preferences (usually deployed via MDM)
const QStringList& chromiumPreferenceDomains()
{
	static const QStringList domains{
		QStringLiteral("com.google.Chrome"),
		QStringLiteral("com.microsoft.Edge"),
		QStringLiteral("com.brave.Browser"),
	};
	return domains;
}

const auto ManagedPreferencesDirectory = QStringLiteral("/Library/Managed Preferences");
const auto HostsFilePath = QStringLiteral("/etc/hosts");

QString plistArray( const QString& key, const QStringList& values )
{
	QString xml = QStringLiteral("\t<key>%1</key>\n\t<array>\n").arg( key );
	for( const auto& value : values )
	{
		xml += QStringLiteral("\t\t<string>%1</string>\n").arg( value.toHtmlEscaped() );
	}
	return xml + QStringLiteral("\t</array>\n");
}

void flushDnsCache()
{
	AddOnUtils::runProcess( QStringLiteral("/usr/bin/dscacheutil"), { QStringLiteral("-flushcache") }, 10000 );
	AddOnUtils::runProcess( QStringLiteral("/usr/bin/killall"), { QStringLiteral("-HUP"), QStringLiteral("mDNSResponder") }, 10000 );
}

}



bool WebsiteFilterBackend::apply( const WebsiteRules& rules, QString* errorString )
{
	QDir().mkpath( ManagedPreferencesDirectory );

	// back up existing managed preferences (deployed by MDM) so they can be restored
	if( m_state.contains( QStringLiteral("backup") ) == false )
	{
		QJsonObject backup;
		for( const auto& domain : chromiumPreferenceDomains() )
		{
			QFile file( ManagedPreferencesDirectory + QLatin1Char('/') + domain + QStringLiteral(".plist") );
			if( file.open( QFile::ReadOnly ) )
			{
				backup[domain] = QString::fromLatin1( file.readAll().toBase64() );
			}
		}
		m_state[QStringLiteral("backup")] = backup;
	}

	QString policies;
	if( rules.mode == WebsiteRules::Mode::Whitelist )
	{
		policies = plistArray( QStringLiteral("URLBlocklist"), { QStringLiteral("*") } ) +
				   plistArray( QStringLiteral("URLAllowlist"), rules.domains + QStringList{ QStringLiteral("chrome://*"),
																							 QStringLiteral("edge://*"),
																							 QStringLiteral("brave://*") } );
	}
	else
	{
		policies = plistArray( QStringLiteral("URLBlocklist"), rules.domains );
	}

	const auto plist = QStringLiteral( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
									   "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
									   "<plist version=\"1.0\">\n<dict>\n%1</dict>\n</plist>\n" ).arg( policies );

	bool success = true;
	for( const auto& domain : chromiumPreferenceDomains() )
	{
		QSaveFile file( ManagedPreferencesDirectory + QLatin1Char('/') + domain + QStringLiteral(".plist") );
		success &= file.open( QFile::WriteOnly ) && file.write( plist.toUtf8() ) > 0 && file.commit();
	}

	// the hosts file also covers Safari and other applications
	const auto useHostsFile = rules.useHostsFile && rules.mode == WebsiteRules::Mode::Blacklist;
	if( updateHostsFile( HostsFilePath, useHostsFile ? rules.domains : QStringList{} ) == false )
	{
		*errorString = QCoreApplication::translate( "WebsiteFilterBackend", "Could not update the hosts file." );
		success = false;
	}
	flushDnsCache();

	if( success == false && errorString->isEmpty() )
	{
		*errorString = QCoreApplication::translate( "WebsiteFilterBackend", "Could not write browser policies." );
	}

	return success;
}



void WebsiteFilterBackend::clear()
{
	// without a backup the managed preferences have not been written by Veyon and must not be touched
	if( m_state.contains( QStringLiteral("backup") ) )
	{
		const auto backup = m_state[QStringLiteral("backup")].toObject();
		for( const auto& domain : chromiumPreferenceDomains() )
		{
			const auto filePath = ManagedPreferencesDirectory + QLatin1Char('/') + domain + QStringLiteral(".plist");
			if( backup.contains( domain ) )
			{
				QSaveFile file( filePath );
				if( file.open( QFile::WriteOnly ) )
				{
					file.write( QByteArray::fromBase64( backup[domain].toString().toLatin1() ) );
					file.commit();
				}
			}
			else
			{
				QFile::remove( filePath );
			}
		}
		m_state.remove( QStringLiteral("backup") );
	}

	updateHostsFile( HostsFilePath, {} );
	flushDnsCache();
}
