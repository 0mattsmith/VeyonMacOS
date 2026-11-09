/*
 * PluginManager.cpp - implementation of the PluginManager class
 *
 * Copyright (c) 2017-2026 Tobias Junghans <tobydox@veyon.io>
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

#include <veyonconfig.h>

#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPluginLoader>

#include "Filesystem.h"
#include "Logger.h"
#include "PlatformPluginInterface.h"
#include "PluginManager.h"
#include "VeyonConfiguration.h"


PluginManager::PluginManager( QObject* parent ) :
	QObject( parent ),
	m_pluginInterfaces(),
	m_pluginObjects(),
	m_pluginLoaders(),
	m_noDebugMessages( qEnvironmentVariableIsSet( Logger::logLevelEnvironmentVariable() ) )
{
	initPluginSearchPath();
}



PluginManager::~PluginManager()
{
	vDebug();

	for( auto pluginLoader : std::as_const(m_pluginLoaders) )
	{
		pluginLoader->unload();
	}

	m_pluginInterfaces.clear();
	m_pluginObjects.clear();
	m_disabledPluginObjects.clear();
}



void PluginManager::loadPlatformPlugins()
{
	// configuration is not available yet so add-ons can't be considered here
	loadPlugins( QStringLiteral("*-platform") + VeyonCore::sharedLibrarySuffix(), false );
}



void PluginManager::loadPlugins()
{
	loadPlugins( QStringLiteral("*") + VeyonCore::sharedLibrarySuffix(), true );
}



void PluginManager::upgradePlugins()
{
	auto versions = VeyonCore::config().pluginVersions();

	for( auto pluginInterface : std::as_const( m_pluginInterfaces ) )
	{
		const auto pluginUid = pluginInterface->uid().toString();
		auto previousPluginVersion = QVersionNumber::fromString( versions.value( pluginUid ).toString() );
		if( previousPluginVersion.isNull() )
		{
			previousPluginVersion = QVersionNumber( 1, 1 );
		}
		const auto currentPluginVersion = pluginInterface->version();
		if( currentPluginVersion > previousPluginVersion )
		{
			vDebug() << "Upgrading plugin" << pluginInterface->name()
					 << "from" << previousPluginVersion.toString()
					 << "to" << currentPluginVersion.toString();
			pluginInterface->upgrade( previousPluginVersion );
		}

		versions[pluginUid] = currentPluginVersion.toString();
	}

	VeyonCore::config().setPluginVersions( versions );
}



void PluginManager::registerExtraPluginInterface( QObject* pluginObject )
{
	auto pluginInterface = qobject_cast<PluginInterface *>( pluginObject );
	if( pluginInterface )
	{
		m_pluginInterfaces += pluginInterface;
		m_pluginObjects += pluginObject;
	}
}



PluginUidList PluginManager::pluginUids() const
{
	PluginUidList pluginUidList;

	pluginUidList.reserve( m_pluginInterfaces.size() );

	for( auto pluginInterface : std::as_const( m_pluginInterfaces ) )
	{
		pluginUidList += pluginInterface->uid();
	}

	return pluginUidList;
}



PluginInterface* PluginManager::pluginInterface( Plugin::Uid pluginUid )
{
	for( auto pluginInterface : std::as_const( m_pluginInterfaces ) )
	{
		if( pluginInterface->uid() == pluginUid )
		{
			return pluginInterface;
		}
	}

	return nullptr;
}



QString PluginManager::pluginName( Plugin::Uid pluginUid ) const
{
	for( auto pluginInterface : m_pluginInterfaces )
	{
		if( pluginInterface->uid() == pluginUid )
		{
			return pluginInterface->name();
		}
	}

	return {};
}



QString PluginManager::pluginFilePath( Plugin::Uid pluginUid ) const
{
	return m_pluginFilePaths.value( pluginUid );
}



bool PluginManager::isPluginDisabled( Plugin::Uid pluginUid ) const
{
	const auto disabledAddOns = VeyonCore::config().disabledAddOns();
	for( const auto& disabledAddOn : disabledAddOns )
	{
		if( Plugin::Uid{disabledAddOn} == pluginUid )
		{
			return true;
		}
	}

	return false;
}



void PluginManager::initPluginSearchPath()
{
	QDir dir( QCoreApplication::applicationDirPath() );
	if( dir.cd( VeyonCore::pluginDir() ) )
	{
		const auto pluginSearchPath = dir.absolutePath();
		if( m_noDebugMessages == false )
		{
			vDebug() << "Adding plugin search path" << pluginSearchPath;
		}
		m_pluginSearchPaths.append(pluginSearchPath);
		QCoreApplication::addLibraryPath( pluginSearchPath );
	}
#if defined(VEYON_WITH_TESTS)
	for (const auto& baseDir : {QStringLiteral(CMAKE_BINARY_DIR "/plugins/platform"),
								 QStringLiteral(CMAKE_BINARY_DIR "/plugins/vncserver"),
								 QStringLiteral(CMAKE_BINARY_DIR "/plugins/")})
	{
		const auto pluginDirs = QDir(baseDir).entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot);
		for (const auto& pluginDir : pluginDirs)
		{
			m_pluginSearchPaths.append(pluginDir.absoluteFilePath());
		}
	}
#endif
}



void PluginManager::loadPlugins( const QString& nameFilter, bool includeAddOns )
{
	QFileInfoList plugins;
	for (const auto& pluginSearchPath : std::as_const(m_pluginSearchPaths))
	{
		plugins.append(QDir(pluginSearchPath).entryInfoList({nameFilter}));
	}

	PluginUidList disabledPluginUids;
	// only determined when loading add-ons: it's queried from the platform plugin on Windows and macOS, which
	// isn't available yet while the platform plugins themselves are being loaded
	QString addOnsDirectory;
	if( includeAddOns )
	{
		plugins.append( addOnPluginFiles( nameFilter ) );

		const auto disabledAddOns = VeyonCore::config().disabledAddOns();
		for( const auto& disabledAddOn : disabledAddOns )
		{
			disabledPluginUids.append( Plugin::Uid{disabledAddOn} );
		}

		addOnsDirectory = VeyonCore::filesystem().addOnsDirectoryPath();
	}

	for( const auto& fileInfo : plugins )
	{
		const auto fileName = fileInfo.fileName();

		// skip simple shared libraries
		if( fileName.startsWith( QLatin1String("lib") ) &&
			fileName.startsWith( QLatin1String("libveyon") ) == false )
		{
			continue;
		}

		auto pluginLoader = new QPluginLoader( fileInfo.filePath(), this );
		auto pluginObject = pluginLoader->instance();
		auto pluginInterface = qobject_cast<PluginInterface *>( pluginObject );

		if( pluginObject == nullptr || pluginInterface == nullptr ||
			m_pluginInterfaces.contains( pluginInterface ) ||
			m_disabledPluginObjects.contains( pluginObject ) )
		{
			if( pluginObject == nullptr && addOnsDirectory.isEmpty() == false &&
				QDir::toNativeSeparators( fileInfo.filePath() ).startsWith( addOnsDirectory ) )
			{
				vWarning() << "failed to load add-on" << fileInfo.filePath() << pluginLoader->errorString();
			}
			delete pluginLoader;
			continue;
		}

		// platform plugins can never be disabled
		if( disabledPluginUids.contains( pluginInterface->uid() ) &&
			qobject_cast<PlatformPluginInterface *>( pluginObject ) == nullptr )
		{
			if( m_noDebugMessages == false )
			{
				vDebug() << "plugin" << pluginInterface->name() << "has been disabled";
			}

			if( keepDisabledPluginsLoaded() )
			{
				// keep plugin loaded (but inactive) so its metadata can be displayed in the Add-On Store
				m_disabledPluginObjects += pluginObject;	// clazy:exclude=reserve-candidates
				m_pluginFilePaths[pluginInterface->uid()] = fileInfo.filePath();
				m_pluginLoaders += pluginLoader;			// clazy:exclude=reserve-candidates
			}
			else
			{
				pluginLoader->unload();
				delete pluginLoader;
			}
			continue;
		}

		if( m_noDebugMessages == false )
		{
			vDebug() << "discovered plugin" << pluginInterface->name() << "at" << fileInfo.filePath();
		}
		m_pluginInterfaces += pluginInterface;	// clazy:exclude=reserve-candidates
		m_pluginObjects += pluginObject;		// clazy:exclude=reserve-candidates
		m_pluginLoaders += pluginLoader;			// clazy:exclude=reserve-candidates
		m_pluginFilePaths[pluginInterface->uid()] = fileInfo.filePath();
	}
}



QFileInfoList PluginManager::addOnPluginFiles( const QString& nameFilter ) const
{
	QFileInfoList pluginFiles;

	const auto disabledAddOns = VeyonCore::config().disabledAddOns();

	// installed add-on packages are stored in subdirectories along with a manifest file
	const auto packageDirs = QDir( VeyonCore::filesystem().addOnsDirectoryPath() ).entryInfoList( QDir::Dirs | QDir::NoDotAndDotDot );
	for( const auto& packageDir : packageDirs )
	{
		// skip temporary directories used while installing or removing packages
		if( packageDir.fileName().startsWith( QLatin1Char('.') ) )
		{
			continue;
		}

		QFile manifestFile( packageDir.filePath() + QStringLiteral("/manifest.json") );
		if( manifestFile.open( QFile::ReadOnly ) == false )
		{
			continue;
		}

		const auto manifest = QJsonDocument::fromJson( manifestFile.readAll() ).object();
		if( manifest[QStringLiteral("type")].toString() != QLatin1String("plugin") )
		{
			continue;
		}

		// never load code of disabled third party add-ons - the Add-On Store uses the manifest instead
		const Plugin::Uid uid{ manifest[QStringLiteral("uid")].toString() };
		bool disabled = false;
		for( const auto& disabledAddOn : disabledAddOns )
		{
			disabled |= ( Plugin::Uid{disabledAddOn} == uid );
		}
		if( uid.isNull() || disabled )
		{
			continue;
		}

		pluginFiles.append( QDir( packageDir.filePath() ).entryInfoList( { nameFilter }, QDir::Files ) );
	}

	return pluginFiles;
}



bool PluginManager::keepDisabledPluginsLoaded() const
{
	switch( VeyonCore::component() )
	{
	case VeyonCore::Component::Master:
	case VeyonCore::Component::Configurator:
	case VeyonCore::Component::CLI:
		return true;
	default:
		break;
	}

	return false;
}
