/*
 * WebsiteFilterBackendLinux.cpp - applies website rules on Linux
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
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include "WebsiteFilterBackend.h"


namespace {

// Chromium based browsers read all JSON files in these directories
const QStringList& chromiumPolicyDirectories()
{
	static const QStringList directories{
		QStringLiteral("/etc/opt/chrome/policies/managed"),
		QStringLiteral("/etc/chromium/policies/managed"),
		QStringLiteral("/etc/opt/edge/policies/managed"),
		QStringLiteral("/etc/brave/policies/managed"),
	};
	return directories;
}

const auto PolicyFileName = QStringLiteral("veyon-website-filter.json");
const auto FirefoxPolicyFile = QStringLiteral("/etc/firefox/policies/policies.json");
const auto HostsFilePath = QStringLiteral("/etc/hosts");

bool writeJson( const QString& filePath, const QJsonObject& object )
{
	QDir().mkpath( QFileInfo( filePath ).absolutePath() );
	QSaveFile file( filePath );
	return file.open( QFile::WriteOnly ) &&
			file.write( QJsonDocument( object ).toJson() ) > 0 &&
			file.commit();
}

QJsonObject readJson( const QString& filePath )
{
	QFile file( filePath );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return {};
	}
	return QJsonDocument::fromJson( file.readAll() ).object();
}

}



bool WebsiteFilterBackend::apply( const WebsiteRules& rules, QString* errorString )
{
	// back up the Firefox website filter configured before (other policies in the file are kept as they are)
	if( m_state.contains( QStringLiteral("backup") ) == false )
	{
		const auto firefoxPolicies = readJson( FirefoxPolicyFile );
		m_state[QStringLiteral("backup")] = QJsonObject{
			{ QStringLiteral("firefoxFileExisted"), QFile::exists( FirefoxPolicyFile ) },
			{ QStringLiteral("firefoxWebsiteFilter"), firefoxPolicies[QStringLiteral("policies")].toObject()[QStringLiteral("WebsiteFilter")] },
		};
	}

	QJsonObject chromiumPolicies;
	QJsonObject firefoxWebsiteFilter;
	if( rules.mode == WebsiteRules::Mode::Whitelist )
	{
		chromiumPolicies[QStringLiteral("URLBlocklist")] = QJsonArray{ QStringLiteral("*") };
		chromiumPolicies[QStringLiteral("URLAllowlist")] = QJsonArray::fromStringList( rules.domains + QStringList{ QStringLiteral("chrome://*"),
																													QStringLiteral("edge://*"),
																													QStringLiteral("brave://*") } );
		firefoxWebsiteFilter[QStringLiteral("Block")] = QJsonArray{ QStringLiteral("<all_urls>") };
		firefoxWebsiteFilter[QStringLiteral("Exceptions")] = QJsonArray::fromStringList( firefoxPatterns( rules.domains ) );
	}
	else
	{
		chromiumPolicies[QStringLiteral("URLBlocklist")] = QJsonArray::fromStringList( rules.domains );
		firefoxWebsiteFilter[QStringLiteral("Block")] = QJsonArray::fromStringList( firefoxPatterns( rules.domains ) );
	}

	bool success = true;
	for( const auto& directory : chromiumPolicyDirectories() )
	{
		success &= writeJson( directory + QLatin1Char('/') + PolicyFileName, chromiumPolicies );
	}

	auto firefoxPolicies = readJson( FirefoxPolicyFile );
	auto policies = firefoxPolicies[QStringLiteral("policies")].toObject();
	policies[QStringLiteral("WebsiteFilter")] = firefoxWebsiteFilter;
	firefoxPolicies[QStringLiteral("policies")] = policies;
	success &= writeJson( FirefoxPolicyFile, firefoxPolicies );

	const auto useHostsFile = rules.useHostsFile && rules.mode == WebsiteRules::Mode::Blacklist;
	if( updateHostsFile( HostsFilePath, useHostsFile ? rules.domains : QStringList{} ) == false )
	{
		*errorString = QCoreApplication::translate( "WebsiteFilterBackend", "Could not update the hosts file." );
		success = false;
	}

	if( success == false && errorString->isEmpty() )
	{
		*errorString = QCoreApplication::translate( "WebsiteFilterBackend", "Could not write browser policies." );
	}

	return success;
}



void WebsiteFilterBackend::clear()
{
	for( const auto& directory : chromiumPolicyDirectories() )
	{
		QFile::remove( directory + QLatin1Char('/') + PolicyFileName );
	}

	if( m_state.contains( QStringLiteral("backup") ) )
	{
		const auto backup = m_state[QStringLiteral("backup")].toObject();
		if( backup[QStringLiteral("firefoxFileExisted")].toBool() )
		{
			auto firefoxPolicies = readJson( FirefoxPolicyFile );
			auto policies = firefoxPolicies[QStringLiteral("policies")].toObject();
			const auto previousFilter = backup[QStringLiteral("firefoxWebsiteFilter")];
			if( previousFilter.isUndefined() || previousFilter.isNull() )
			{
				policies.remove( QStringLiteral("WebsiteFilter") );
			}
			else
			{
				policies[QStringLiteral("WebsiteFilter")] = previousFilter;
			}
			firefoxPolicies[QStringLiteral("policies")] = policies;
			writeJson( FirefoxPolicyFile, firefoxPolicies );
		}
		else
		{
			QFile::remove( FirefoxPolicyFile );
		}
		m_state.remove( QStringLiteral("backup") );
	}

	updateHostsFile( HostsFilePath, {} );
}
