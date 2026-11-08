/*
 * AddOnCommands.cpp - implementation of AddOnCommands class
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
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

#include "AddOnCommands.h"
#include "AddOnInterface.h"
#include "PlatformCoreFunctions.h"
#include "PlatformPluginInterface.h"
#include "PluginManager.h"
#include "VeyonConfiguration.h"


AddOnCommands::AddOnCommands( QObject* parent ) :
	QObject( parent ),
	m_commands( {
		{ QStringLiteral("list"), tr( "List all add-ons, built-in features and system components" ) },
		{ QStringLiteral("enable"), tr( "Enable an add-on (specified by name, ID or UID)" ) },
		{ QStringLiteral("disable"), tr( "Disable an add-on (specified by name, ID or UID)" ) },
		{ QStringLiteral("install"), tr( "Install an add-on package (*.veyonaddon)" ) },
		{ QStringLiteral("remove"), tr( "Remove an installed add-on" ) },
		{ QStringLiteral("info"), tr( "Show information about an add-on package" ) },
		{ QStringLiteral("pack"), tr( "Create an add-on package from a manifest and plugin binaries" ) },
		{ QStringLiteral("repositories"), tr( "List or set the add-on repositories" ) },
		{ QStringLiteral("revert"), tr( "Remove all restrictions applied by add-ons on this computer and restore the previous settings" ) },
		} )
{
}



QStringList AddOnCommands::commands() const
{
	return m_commands.keys();
}



QString AddOnCommands::commandHelp( const QString& command ) const
{
	return m_commands.value( command );
}



CommandLinePluginInterface::RunResult AddOnCommands::handle_list( const QStringList& arguments )
{
	Q_UNUSED(arguments)

	TableHeader tableHeader( { tr("Name"), tr("Type"), tr("Version"), tr("State"), tr("ID"), tr("UID") } );
	TableRows tableRows;

	const auto addOns = m_manager.addOns();
	tableRows.reserve( addOns.count() );

	for( const auto& addOn : addOns )
	{
		QString kind;
		switch( addOn.kind )
		{
		case AddOnManager::Kind::AddOn: kind = addOn.packageType == QLatin1String("script") ? tr("Script add-on") : tr("Add-on"); break;
		case AddOnManager::Kind::BuiltinFeature: kind = tr("Built-in feature"); break;
		case AddOnManager::Kind::SystemComponent: kind = tr("System component"); break;
		}

		QString state = addOn.enabled ? tr("enabled") : tr("disabled");
		if( addOn.restartRequired() )
		{
			state += QStringLiteral(" (%1)").arg( tr("restart required") );
		}

		tableRows.append( { addOn.name, kind, addOn.version, state, addOn.id,
							addOn.uid.toString( QUuid::WithoutBraces ) } );
	}

	printTable( Table( tableHeader, tableRows ) );

	return NoResult;
}



CommandLinePluginInterface::RunResult AddOnCommands::handle_enable( const QStringList& arguments )
{
	return setEnabled( arguments, true );
}



CommandLinePluginInterface::RunResult AddOnCommands::handle_disable( const QStringList& arguments )
{
	return setEnabled( arguments, false );
}



CommandLinePluginInterface::RunResult AddOnCommands::handle_install( const QStringList& arguments )
{
	if( arguments.isEmpty() )
	{
		printUsage( commandLineModuleName(), QStringLiteral("install"),
					{ { tr("FILE"), {} } }, { { QStringLiteral("--user"), {} } } );
		return NotEnoughArguments;
	}

	const auto scope = arguments.contains( QStringLiteral("--user") ) ? AddOnManager::InstallScope::CurrentUser
																		 : AddOnManager::InstallScope::System;

	QString errorString;
	if( m_manager.installPackage( arguments.first(), scope, &errorString ) == false )
	{
		return operationError( errorString );
	}

	return Successful;
}



CommandLinePluginInterface::RunResult AddOnCommands::handle_remove( const QStringList& arguments )
{
	if( arguments.isEmpty() )
	{
		printUsage( commandLineModuleName(), QStringLiteral("remove"), { { tr("ADD-ON"), {} } } );
		return NotEnoughArguments;
	}

	const auto addOn = findAddOn( arguments.first() );
	if( addOn.has_value() == false )
	{
		return operationError( tr( "No add-on found matching \"%1\"." ).arg( arguments.first() ) );
	}

	QString errorString;
	if( m_manager.uninstall( addOn.value(), &errorString ) == false )
	{
		return operationError( errorString );
	}

	return Successful;
}



CommandLinePluginInterface::RunResult AddOnCommands::handle_info( const QStringList& arguments )
{
	if( arguments.isEmpty() )
	{
		printUsage( commandLineModuleName(), QStringLiteral("info"), { { tr("FILE"), {} } } );
		return NotEnoughArguments;
	}

	const auto package = AddOnManager::readPackage( arguments.first() );
	if( package.isValid() == false )
	{
		return operationError( package.errorString );
	}

	const auto& manifest = package.manifest;

	print( tr( "Name: %1" ).arg( manifest[QStringLiteral("name")].toString() ) );
	print( tr( "ID: %1" ).arg( manifest[QStringLiteral("id")].toString() ) );
	print( tr( "UID: %1" ).arg( manifest[QStringLiteral("uid")].toString() ) );
	print( tr( "Type: %1" ).arg( manifest[QStringLiteral("type")].toString() ) );
	print( tr( "Version: %1" ).arg( manifest[QStringLiteral("version")].toString() ) );
	print( tr( "Vendor: %1" ).arg( manifest[QStringLiteral("vendor")].toString() ) );
	print( tr( "Description: %1" ).arg( manifest[QStringLiteral("description")].toString() ) );

	if( manifest[QStringLiteral("type")].toString() == QLatin1String("plugin") )
	{
		print( tr( "Platforms: %1" ).arg( manifest[QStringLiteral("binaries")].toObject().keys().join( QStringLiteral(", ") ) ) );
	}
	else
	{
		QStringList actions;
		for( const auto& action : manifest[QStringLiteral("actions")].toArray() )
		{
			actions.append( action.toObject()[QStringLiteral("name")].toString() );
		}
		print( tr( "Actions: %1" ).arg( actions.join( QStringLiteral(", ") ) ) );
	}

	return NoResult;
}



CommandLinePluginInterface::RunResult AddOnCommands::handle_pack( const QStringList& arguments )
{
	if( arguments.size() < 3 )
	{
		printUsage( commandLineModuleName(), QStringLiteral("pack"),
					{ { tr("MANIFEST"), {} }, { tr("OUTPUT-FILE"), {} }, { tr("PLATFORM=BINARY"), {} } } );
		printDescription( tr( "Creates an add-on package from a manifest file (JSON) and one or more compiled plugins. "
							  "Every binary has to be prefixed with the platform it has been built for, e.g. "
							  "windows-x86_64=myaddon.dll, macos-universal=myaddon.dylib or linux-x86_64=myaddon.so." ) );
		return NotEnoughArguments;
	}

	QFile manifestFile( arguments[0] );
	if( manifestFile.open( QFile::ReadOnly ) == false )
	{
		return operationError( tr( "Could not open manifest file %1." ).arg( arguments[0] ) );
	}

	auto manifest = QJsonDocument::fromJson( manifestFile.readAll() ).object();
	manifest[QStringLiteral("type")] = QStringLiteral("plugin");
	manifest[QStringLiteral("veyonAddOn")] = AddOnManager::PackageFormatVersion;

	// embed icon if specified as file name
	const auto iconFileName = manifest[QStringLiteral("iconFile")].toString();
	if( iconFileName.isEmpty() == false )
	{
		QFile iconFile( QFileInfo( manifestFile.fileName() ).dir().filePath( iconFileName ) );
		if( iconFile.open( QFile::ReadOnly ) )
		{
			manifest[QStringLiteral("icon")] = QString::fromLatin1( iconFile.readAll().toBase64() );
		}
		manifest.remove( QStringLiteral("iconFile") );
	}

	for( const auto& binaryArgument : arguments.mid( 2 ) )
	{
		const auto separator = binaryArgument.indexOf( QLatin1Char('=') );
		if( separator <= 0 )
		{
			return operationError( tr( "Invalid binary specification \"%1\" - expected PLATFORM=FILE." ).arg( binaryArgument ) );
		}

		QString errorString;
		if( AddOnManager::addBinaryToPackage( manifest, binaryArgument.left( separator ),
											  binaryArgument.mid( separator + 1 ), &errorString ) == false )
		{
			return operationError( errorString );
		}
	}

	QString errorString;
	if( AddOnManager::writePackage( arguments[1], manifest, &errorString ) == false )
	{
		return operationError( errorString );
	}

	return Successful;
}



CommandLinePluginInterface::RunResult AddOnCommands::handle_repositories( const QStringList& arguments )
{
	const auto subCommand = arguments.value( 0, QStringLiteral("list") );

	if( subCommand == QLatin1String("list") )
	{
		const auto repositories = VeyonCore::config().addOnRepositories();
		for( const auto& repository : repositories )
		{
			print( repository );
		}
		return NoResult;
	}

	if( subCommand == QLatin1String("set") || subCommand == QLatin1String("clear") )
	{
		if( VeyonCore::config().isStoreWritable() == false )
		{
			return operationError( tr( "The configuration is not writable - please run this command with administrative privileges." ) );
		}

		VeyonCore::config().setAddOnRepositories( subCommand == QLatin1String("set") ? arguments.mid( 1 ) : QStringList{} );
		VeyonCore::config().flushStore();
		return Successful;
	}

	printUsage( commandLineModuleName(), QStringLiteral("repositories"),
				{ { QStringLiteral("list|set|clear"), {} } }, { { tr("URL"), {} } } );

	return InvalidArguments;
}



CommandLinePluginInterface::RunResult AddOnCommands::setEnabled( const QStringList& arguments, bool enabled )
{
	if( arguments.isEmpty() )
	{
		printUsage( commandLineModuleName(), enabled ? QStringLiteral("enable") : QStringLiteral("disable"),
					{ { tr("ADD-ON"), {} } } );
		return NotEnoughArguments;
	}

	const auto addOn = findAddOn( arguments.first() );

	QString errorString;
	bool success = false;
	if( addOn.has_value() )
	{
		success = m_manager.setEnabled( addOn.value(), enabled, true, &errorString );
	}
	else if( Plugin::Uid{ arguments.first() }.isNull() == false )
	{
		// allow (re-)enabling add-ons which are not available anymore
		success = m_manager.setEnabled( Plugin::Uid{ arguments.first() }, enabled, true, &errorString );
	}
	else
	{
		errorString = tr( "No add-on found matching \"%1\"." ).arg( arguments.first() );
	}

	if( success == false )
	{
		return operationError( errorString );
	}

	return Successful;
}



std::optional<AddOnManager::AddOn> AddOnCommands::findAddOn( const QString& identifier ) const
{
	const Plugin::Uid uid{ identifier };
	const auto addOns = m_manager.addOns();

	for( const auto& addOn : addOns )
	{
		if( ( uid.isNull() == false && addOn.uid == uid ) ||
			addOn.id.compare( identifier, Qt::CaseInsensitive ) == 0 ||
			addOn.name.compare( identifier, Qt::CaseInsensitive ) == 0 )
		{
			return addOn;
		}
	}

	return std::nullopt;
}



CommandLinePluginInterface::RunResult AddOnCommands::handle_revert( const QStringList& arguments )
{
	Q_UNUSED(arguments)

	if( VeyonCore::platform().coreFunctions().isRunningAsAdmin() == false )
	{
		return operationError( tr( "Administrative privileges are required for reverting changes." ) );
	}

	auto result = Successful;

	// disabled add-ons may have applied restrictions before they were disabled
	const auto pluginObjects = VeyonCore::pluginManager().pluginObjects() + VeyonCore::pluginManager().disabledPluginObjects();
	for( auto pluginObject : pluginObjects )
	{
		const auto addOn = qobject_cast<AddOnInterface *>( pluginObject );
		const auto plugin = qobject_cast<PluginInterface *>( pluginObject );
		if( addOn == nullptr || plugin == nullptr )
		{
			continue;
		}

		const auto name = addOn->addOnName().isEmpty() ? plugin->name() : addOn->addOnName();
		QString errorString;
		if( addOn->revertSystemChanges( &errorString ) )
		{
			info( tr( "%1: done" ).arg( name ) );
		}
		else
		{
			error( tr( "%1: %2" ).arg( name, errorString ) );
			result = Failed;
		}
	}

	return result;
}



CommandLinePluginInterface::RunResult AddOnCommands::operationError( const QString& message )
{
	CommandLineIO::error( message );

	return Failed;
}
