/*
 * WebsiteFilterBackendWindows.cpp - applies website rules on Windows
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
#include <QJsonArray>
#include <QSettings>

#include "WebsiteFilterBackend.h"


namespace {

// Chromium based browsers read their policies from these registry keys
const QStringList& chromiumPolicyKeys()
{
	static const QStringList keys{
		QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Google\\Chrome"),
		QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Edge"),
		QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\BraveSoftware\\Brave"),
		QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Chromium"),
	};
	return keys;
}

const auto FirefoxPolicyKey = QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Mozilla\\Firefox");

const auto BlockListName = QStringLiteral("URLBlocklist");
const auto AllowListName = QStringLiteral("URLAllowlist");
const auto DnsOverHttpsName = QStringLiteral("DnsOverHttpsMode");
const auto FirefoxBlockName = QStringLiteral("WebsiteFilter/Block");
const auto FirefoxExceptionsName = QStringLiteral("WebsiteFilter/Exceptions");

// list policies are stored as numbered values ("1", "2", ...) in a sub key
QStringList readList( QSettings& settings, const QString& group )
{
	settings.beginGroup( group );
	auto keys = settings.childKeys();
	std::sort( keys.begin(), keys.end(), []( const QString& a, const QString& b ) { return a.toInt() < b.toInt(); } );
	QStringList values;
	for( const auto& key : std::as_const(keys) )
	{
		values.append( settings.value( key ).toString() );
	}
	settings.endGroup();
	return values;
}

void writeList( QSettings& settings, const QString& group, const QStringList& values )
{
	settings.remove( group );
	if( values.isEmpty() )
	{
		return;
	}
	settings.beginGroup( group );
	for( int i = 0; i < values.size(); ++i )
	{
		settings.setValue( QString::number( i + 1 ), values.at( i ) );
	}
	settings.endGroup();
}

QStringList toStringList( const QJsonValue& value )
{
	QStringList list;
	const auto array = value.toArray();
	for( const auto& entry : array )
	{
		list.append( entry.toString() );
	}
	return list;
}

QString hostsFilePath()
{
	return QString::fromLocal8Bit( qgetenv( "SystemRoot" ) ) + QStringLiteral("\\System32\\drivers\\etc\\hosts");
}

void flushDnsCache()
{
	AddOnUtils::runProcess( QStringLiteral("ipconfig.exe"), { QStringLiteral("/flushdns") }, 10000 );
}

}



bool WebsiteFilterBackend::apply( const WebsiteRules& rules, QString* errorString )
{
	// back up policies configured before (e.g. via Group Policy) so they can be restored
	if( m_state.contains( QStringLiteral("backup") ) == false )
	{
		QJsonObject backup;
		for( const auto& key : chromiumPolicyKeys() )
		{
			QSettings settings( key, QSettings::Registry64Format );
			backup[key] = QJsonObject{
				{ BlockListName, QJsonArray::fromStringList( readList( settings, BlockListName ) ) },
				{ AllowListName, QJsonArray::fromStringList( readList( settings, AllowListName ) ) },
				{ DnsOverHttpsName, settings.value( DnsOverHttpsName ).toString() },
			};
		}
		QSettings firefoxSettings( FirefoxPolicyKey, QSettings::Registry64Format );
		backup[FirefoxPolicyKey] = QJsonObject{
			{ FirefoxBlockName, QJsonArray::fromStringList( readList( firefoxSettings, FirefoxBlockName ) ) },
			{ FirefoxExceptionsName, QJsonArray::fromStringList( readList( firefoxSettings, FirefoxExceptionsName ) ) },
		};
		m_state[QStringLiteral("backup")] = backup;
	}

	const auto backup = m_state[QStringLiteral("backup")].toObject();
	const auto useHostsFile = rules.useHostsFile && rules.mode == WebsiteRules::Mode::Blacklist;

	bool success = true;

	for( const auto& key : chromiumPolicyKeys() )
	{
		const auto previous = backup[key].toObject();
		QSettings settings( key, QSettings::Registry64Format );

		if( rules.mode == WebsiteRules::Mode::Whitelist )
		{
			writeList( settings, BlockListName, { QStringLiteral("*") } );
			writeList( settings, AllowListName, rules.domains + QStringList{ QStringLiteral("chrome://*"),
																			  QStringLiteral("edge://*"),
																			  QStringLiteral("brave://*") } );
		}
		else
		{
			writeList( settings, BlockListName, toStringList( previous[BlockListName] ) + rules.domains );
			writeList( settings, AllowListName, toStringList( previous[AllowListName] ) );
		}

		// browsers bypass the hosts file when using DNS over HTTPS
		const auto previousDnsOverHttps = previous[DnsOverHttpsName].toString();
		if( useHostsFile )
		{
			settings.setValue( DnsOverHttpsName, QStringLiteral("off") );
		}
		else if( previousDnsOverHttps.isEmpty() )
		{
			settings.remove( DnsOverHttpsName );
		}
		else
		{
			settings.setValue( DnsOverHttpsName, previousDnsOverHttps );
		}

		settings.sync();
		success &= settings.status() == QSettings::NoError;
	}

	QSettings firefoxSettings( FirefoxPolicyKey, QSettings::Registry64Format );
	const auto previousFirefox = backup[FirefoxPolicyKey].toObject();
	if( rules.mode == WebsiteRules::Mode::Whitelist )
	{
		writeList( firefoxSettings, FirefoxBlockName, { QStringLiteral("<all_urls>") } );
		writeList( firefoxSettings, FirefoxExceptionsName, firefoxPatterns( rules.domains ) );
	}
	else
	{
		writeList( firefoxSettings, FirefoxBlockName, toStringList( previousFirefox[FirefoxBlockName] ) + firefoxPatterns( rules.domains ) );
		writeList( firefoxSettings, FirefoxExceptionsName, toStringList( previousFirefox[FirefoxExceptionsName] ) );
	}
	firefoxSettings.sync();
	success &= firefoxSettings.status() == QSettings::NoError;

	if( updateHostsFile( hostsFilePath(), useHostsFile ? rules.domains : QStringList{} ) == false )
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
	if( m_state.contains( QStringLiteral("backup") ) )
	{
		const auto backup = m_state[QStringLiteral("backup")].toObject();

		for( const auto& key : chromiumPolicyKeys() )
		{
			const auto previous = backup[key].toObject();
			QSettings settings( key, QSettings::Registry64Format );
			writeList( settings, BlockListName, toStringList( previous[BlockListName] ) );
			writeList( settings, AllowListName, toStringList( previous[AllowListName] ) );
			const auto previousDnsOverHttps = previous[DnsOverHttpsName].toString();
			if( previousDnsOverHttps.isEmpty() )
			{
				settings.remove( DnsOverHttpsName );
			}
			else
			{
				settings.setValue( DnsOverHttpsName, previousDnsOverHttps );
			}
		}

		const auto previousFirefox = backup[FirefoxPolicyKey].toObject();
		QSettings firefoxSettings( FirefoxPolicyKey, QSettings::Registry64Format );
		writeList( firefoxSettings, FirefoxBlockName, toStringList( previousFirefox[FirefoxBlockName] ) );
		writeList( firefoxSettings, FirefoxExceptionsName, toStringList( previousFirefox[FirefoxExceptionsName] ) );

		m_state.remove( QStringLiteral("backup") );
	}

	updateHostsFile( hostsFilePath(), {} );
	flushDnsCache();
}
