/*
 * AddOnManager.cpp - implementation of AddOnManager class
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
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaEnum>
#include <QMutex>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSysInfo>
#include <QTimer>
#include <QVersionNumber>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

#include "AddOnManager.h"
#include "FeatureProviderInterface.h"
#include "Filesystem.h"
#include "PlatformCoreFunctions.h"
#include "PlatformPluginInterface.h"
#include "PluginManager.h"
#include "VeyonConfiguration.h"


static QMutex loadedScriptPackagesMutex; // clazy:exclude=non-pod-global-static
static PluginUidList loadedScriptPackages; // clazy:exclude=non-pod-global-static


AddOnManager::AddOnManager( QObject* parent ) :
	QObject( parent )
{
}



AddOnManager::AddOnList AddOnManager::addOns() const
{
	AddOnList addOns;

	const auto addPlugins = [&]( const QObjectList& pluginObjects, bool loaded ) {
		for( auto pluginObject : pluginObjects )
		{
			const auto addOn = addOnFromPluginObject( pluginObject, loaded );
			if( addOn.uid.isNull() ||
				std::find_if( addOns.cbegin(), addOns.cend(),
							  [&addOn]( const AddOn& a ) { return a.uid == addOn.uid; } ) != addOns.cend() )
			{
				continue;
			}
			addOns.append( addOn );
		}
	};

	addPlugins( VeyonCore::pluginManager().pluginObjects(), true );
	addPlugins( VeyonCore::pluginManager().disabledPluginObjects(), false );

	// merge information of installed packages
	for( const auto& [baseDirectory, origin] : {
			 std::make_pair( systemAddOnsDirectory(), Origin::Installed ),
			 std::make_pair( userAddOnsDirectory(), Origin::UserInstalled ) } )
	{
		const auto packageDirs = QDir( baseDirectory ).entryInfoList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name );
		for( const auto& packageDir : packageDirs )
		{
			if( packageDir.fileName().startsWith( QLatin1Char('.') ) )
			{
				continue;
			}

			const auto manifest = readManifest( packageDir.filePath() );
			if( manifest.isEmpty() )
			{
				continue;
			}

			auto packageAddOn = addOnFromManifest( manifest, packageDir.filePath(), origin );

			auto it = std::find_if( addOns.begin(), addOns.end(),
									[&packageAddOn]( const AddOn& a ) { return a.uid == packageAddOn.uid; } );
			if( it != addOns.end() )
			{
				// plugin of package is loaded - keep runtime information but use package metadata
				packageAddOn.loaded = it->loaded;
				if( packageAddOn.iconData.isEmpty() )
				{
					packageAddOn.iconUrl = it->iconUrl;
				}
				*it = packageAddOn;
			}
			else
			{
				if( packageAddOn.packageType == QLatin1String("script") )
				{
					QMutexLocker locker( &loadedScriptPackagesMutex );
					packageAddOn.loaded = loadedScriptPackages.contains( packageAddOn.uid );
				}
				addOns.append( packageAddOn );
			}
		}
	}

	std::sort( addOns.begin(), addOns.end(), []( const AddOn& a, const AddOn& b ) {
		if( a.kind != b.kind )
		{
			return a.kind < b.kind;
		}
		return a.name.compare( b.name, Qt::CaseInsensitive ) < 0;
	} );

	return addOns;
}



bool AddOnManager::setEnabled( const AddOn& addOn, bool enabled, bool saveConfiguration, QString* errorString )
{
	if( enabled == false && addOn.canBeDisabled == false )
	{
		if( errorString )
		{
			*errorString = tr( "The component \"%1\" is required by Veyon and can't be disabled." ).arg( addOn.name );
		}
		return false;
	}

	if( addOn.origin == Origin::UserInstalled )
	{
		const auto markerFile = addOn.location + QDir::separator() + QLatin1String(UserDisabledMarkerFileName);
		bool success = true;
		if( enabled )
		{
			success = QFile::exists( markerFile ) == false || QFile::remove( markerFile );
		}
		else
		{
			QFile file( markerFile );
			success = file.open( QFile::WriteOnly | QFile::Truncate );
		}

		if( success == false && errorString )
		{
			*errorString = tr( "Could not change the state of add-on \"%1\"." ).arg( addOn.name );
		}

		Q_EMIT addOnsChanged();

		return success;
	}

	return setEnabled( addOn.uid, enabled, saveConfiguration, errorString );
}



bool AddOnManager::setEnabled( Plugin::Uid uid, bool enabled, bool saveConfiguration, QString* errorString )
{
	auto disabledAddOns = VeyonCore::config().disabledAddOns();
	disabledAddOns.erase( std::remove_if( disabledAddOns.begin(), disabledAddOns.end(),
										  [&uid]( const QString& entry ) { return Plugin::Uid{entry} == uid; } ),
						  disabledAddOns.end() );
	if( enabled == false )
	{
		disabledAddOns.append( uid.toString( QUuid::WithoutBraces ) );
	}

	if( saveConfiguration && VeyonCore::config().isStoreWritable() == false )
	{
		if( VeyonCore::component() == VeyonCore::Component::CLI )
		{
			if( errorString )
			{
				*errorString = tr( "The configuration is not writable - please run this command with administrative privileges." );
			}
			return false;
		}

		// let veyon-cli perform the change with administrative privileges
		const auto success = runCliElevated( { QStringLiteral("addons"),
											   enabled ? QStringLiteral("enable") : QStringLiteral("disable"),
											   uid.toString( QUuid::WithoutBraces ) }, errorString );
		VeyonCore::config().reloadFromStore();
		Q_EMIT addOnsChanged();
		return success;
	}

	VeyonCore::config().setDisabledAddOns( disabledAddOns );

	if( saveConfiguration )
	{
		VeyonCore::config().flushStore();
	}

	Q_EMIT addOnsChanged();

	return true;
}



bool AddOnManager::installPackage( const QString& packageFilePath, InstallScope scope, QString* errorString )
{
	const auto setError = [errorString]( const QString& message ) {
		vWarning() << message;
		if( errorString )
		{
			*errorString = message;
		}
		return false;
	};

	const auto package = readPackage( packageFilePath );
	if( package.isValid() == false )
	{
		return setError( package.errorString );
	}

	const auto& manifest = package.manifest;
	const auto type = manifest[QStringLiteral("type")].toString();
	const auto id = manifest[QStringLiteral("id")].toString();
	const Plugin::Uid uid{ manifest[QStringLiteral("uid")].toString() };

	if( scope == InstallScope::CurrentUser )
	{
		if( type != QLatin1String("script") )
		{
			return setError( tr( "Only script add-ons can be installed for the current user only." ) );
		}

		if( VeyonCore::config().userAddOnsEnabled() == false )
		{
			return setError( tr( "Installing add-ons for individual users has been disabled by the administrator." ) );
		}
	}

	// refuse packages which would replace bundled components
	for( const auto& pluginObjects : { VeyonCore::pluginManager().pluginObjects(),
									   VeyonCore::pluginManager().disabledPluginObjects() } )
	{
		for( auto pluginObject : pluginObjects )
		{
			const auto pluginInterface = qobject_cast<PluginInterface *>( pluginObject );
			if( pluginInterface && pluginInterface->uid() == uid &&
				QDir::toNativeSeparators( VeyonCore::pluginManager().pluginFilePath( uid ) ).startsWith( systemAddOnsDirectory() ) == false )
			{
				return setError( tr( "The add-on \"%1\" conflicts with the built-in component \"%2\"." ).
								 arg( manifest[QStringLiteral("name")].toString(), pluginInterface->name() ) );
			}
		}
	}

	const auto baseDirectory = scope == InstallScope::System ? systemAddOnsDirectory() : userAddOnsDirectory();

	if( scope == InstallScope::System &&
		VeyonCore::platform().coreFunctions().isRunningAsAdmin() == false &&
		VeyonCore::component() != VeyonCore::Component::CLI )
	{
		const auto success = runCliElevated( { QStringLiteral("addons"), QStringLiteral("install"),
											   QDir::toNativeSeparators( QFileInfo( packageFilePath ).absoluteFilePath() ) },
											 errorString );
		VeyonCore::config().reloadFromStore();
		Q_EMIT addOnsChanged();
		return success;
	}

	if( QDir().mkpath( baseDirectory ) == false )
	{
		return setError( tr( "Could not create add-ons directory %1." ).arg( baseDirectory ) );
	}

	// clean up remains of previous upgrades or removals (files might have been in use back then)
	const auto trashDirectories = QDir( baseDirectory ).entryInfoList( { QStringLiteral(".trash-*") },
																	   QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot );
	for( const auto& trashDirectory : trashDirectories )
	{
		QDir( trashDirectory.filePath() ).removeRecursively();
	}

	const auto targetDirectory = baseDirectory + QDir::separator() + id;
	const auto stagingDirectory = baseDirectory + QDir::separator() + QStringLiteral(".%1.new").arg( id );

	removeDirectory( stagingDirectory, baseDirectory );
	if( QDir().mkpath( stagingDirectory ) == false )
	{
		return setError( tr( "Could not create directory %1." ).arg( stagingDirectory ) );
	}

	auto installedManifest = manifest;

	if( type == QLatin1String("plugin") )
	{
		const auto binaries = manifest[QStringLiteral("binaries")].toObject();
		QJsonObject binary;
		QString binaryKey;
		for( const auto& key : currentPlatformKeys() )
		{
			if( binaries.contains( key ) )
			{
				binary = binaries[key].toObject();
				binaryKey = key;
				break;
			}
		}

		if( binary.isEmpty() )
		{
			removeDirectory( stagingDirectory, baseDirectory );
			return setError( tr( "The add-on \"%1\" is not available for this platform (%2)." ).
							 arg( manifest[QStringLiteral("name")].toString(), currentPlatformKeys().constFirst() ) );
		}

		const auto data = QByteArray::fromBase64( binary[QStringLiteral("data")].toString().toLatin1() );
		const auto expectedChecksum = binary[QStringLiteral("sha256")].toString().toLower();
		const auto checksum = QString::fromLatin1( QCryptographicHash::hash( data, QCryptographicHash::Sha256 ).toHex() );
		if( data.isEmpty() || ( expectedChecksum.isEmpty() == false && checksum != expectedChecksum ) )
		{
			removeDirectory( stagingDirectory, baseDirectory );
			return setError( tr( "The add-on package is damaged (checksum mismatch)." ) );
		}

		// plugin file names must not start with "lib" as such files are ignored by the PluginManager
		auto fileName = id;
		fileName.replace( QRegularExpression( QStringLiteral("[^A-Za-z0-9_-]") ), QStringLiteral("_") );
		fileName = QStringLiteral("addon-") + fileName + VeyonCore::sharedLibrarySuffix();

		QFile binaryFile( stagingDirectory + QDir::separator() + fileName );
		if( binaryFile.open( QFile::WriteOnly | QFile::Truncate ) == false ||
			binaryFile.write( data ) != data.size() )
		{
			removeDirectory( stagingDirectory, baseDirectory );
			return setError( tr( "Could not write add-on file %1." ).arg( binaryFile.fileName() ) );
		}
		binaryFile.close();
		binaryFile.setPermissions( QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
								   QFile::ReadGroup | QFile::ExeGroup |
								   QFile::ReadOther | QFile::ExeOther );

		binary.remove( QStringLiteral("data") );
		installedManifest[QStringLiteral("binaries")] = QJsonObject{ { binaryKey, binary } };
		installedManifest[QStringLiteral("installedFile")] = fileName;
	}

	const auto iconData = QByteArray::fromBase64( manifest[QStringLiteral("icon")].toString().toLatin1() );
	if( iconData.isEmpty() == false )
	{
		QFile iconFile( stagingDirectory + QDir::separator() + QLatin1String(IconFileName) );
		if( iconFile.open( QFile::WriteOnly | QFile::Truncate ) )
		{
			iconFile.write( iconData );
		}
		installedManifest.remove( QStringLiteral("icon") );
	}

	QSaveFile manifestFile( stagingDirectory + QDir::separator() + QLatin1String(ManifestFileName) );
	if( manifestFile.open( QFile::WriteOnly ) == false ||
		manifestFile.write( QJsonDocument( installedManifest ).toJson( QJsonDocument::Indented ) ) < 0 ||
		manifestFile.commit() == false )
	{
		removeDirectory( stagingDirectory, baseDirectory );
		return setError( tr( "Could not write add-on manifest." ) );
	}

	if( QFileInfo::exists( targetDirectory ) && removeDirectory( targetDirectory, baseDirectory ) == false )
	{
		removeDirectory( stagingDirectory, baseDirectory );
		return setError( tr( "The previous version of add-on \"%1\" is still in use. "
							 "Please stop all Veyon programs and services and try again." ).
						 arg( manifest[QStringLiteral("name")].toString() ) );
	}

	if( QDir().rename( stagingDirectory, targetDirectory ) == false )
	{
		removeDirectory( stagingDirectory, baseDirectory );
		return setError( tr( "Could not install add-on to %1." ).arg( targetDirectory ) );
	}

	vInfo() << "installed add-on" << id << manifest[QStringLiteral("version")].toString() << "to" << targetDirectory;

	Q_EMIT addOnsChanged();

	return true;
}



bool AddOnManager::uninstall( const AddOn& addOn, QString* errorString )
{
	if( addOn.removable == false || addOn.location.isEmpty() )
	{
		if( errorString )
		{
			*errorString = tr( "The add-on \"%1\" is part of Veyon and can't be removed. You can disable it instead." ).arg( addOn.name );
		}
		return false;
	}

	if( addOn.origin == Origin::Installed &&
		VeyonCore::platform().coreFunctions().isRunningAsAdmin() == false &&
		VeyonCore::component() != VeyonCore::Component::CLI )
	{
		const auto success = runCliElevated( { QStringLiteral("addons"), QStringLiteral("remove"), addOn.id }, errorString );
		VeyonCore::config().reloadFromStore();
		Q_EMIT addOnsChanged();
		return success;
	}

	const auto baseDirectory = addOn.origin == Origin::UserInstalled ? userAddOnsDirectory() : systemAddOnsDirectory();
	if( removeDirectory( addOn.location, baseDirectory ) == false )
	{
		if( errorString )
		{
			*errorString = tr( "Could not remove add-on \"%1\". Please stop all Veyon programs and services and try again." ).arg( addOn.name );
		}
		return false;
	}

	if( addOn.origin == Origin::Installed && isDisabledInConfiguration( addOn.uid ) )
	{
		setEnabled( addOn.uid, true, VeyonCore::config().isStoreWritable() );
	}

	vInfo() << "removed add-on" << addOn.id;

	Q_EMIT addOnsChanged();

	return true;
}



bool AddOnManager::canModifySystemAddOns() const
{
	return VeyonCore::config().isStoreWritable() &&
			( QFileInfo( systemAddOnsDirectory() ).isWritable() ||
			  VeyonCore::platform().coreFunctions().isRunningAsAdmin() );
}



bool AddOnManager::runCliElevated( const QStringList& arguments, QString* errorString )
{
	const auto setError = [errorString]( const QString& message ) {
		vWarning() << message;
		if( errorString )
		{
			*errorString = message;
		}
		return false;
	};

#ifdef Q_OS_WIN
	// use the non-console version of the CLI so no console window pops up
	const auto cliFilePath = QDir::toNativeSeparators( QCoreApplication::applicationDirPath() +
													   QStringLiteral("/veyon-wcli.exe") );
#else
	const auto cliFilePath = VeyonCore::filesystem().cliFilePath();
#endif

	if( QFileInfo::exists( cliFilePath ) == false )
	{
		return setError( tr( "Veyon CLI not found at %1." ).arg( cliFilePath ) );
	}

	const auto runProcess = [&]( const QString& program, const QStringList& args ) -> int {
		QProcess process;
		QEventLoop eventLoop;
		QObject::connect( &process, QOverload<int, QProcess::ExitStatus>::of( &QProcess::finished ), &eventLoop, &QEventLoop::quit );
		QObject::connect( &process, &QProcess::errorOccurred, &eventLoop, &QEventLoop::quit );
		process.start( program, args );
		if( process.waitForStarted() == false )
		{
			return -1;
		}
		// keep the UI responsive while e.g. an authentication dialog is shown
		eventLoop.exec();
		process.waitForFinished();
		if( process.exitStatus() != QProcess::NormalExit )
		{
			return -1;
		}
		const auto output = process.readAllStandardError().trimmed();
		if( output.isEmpty() == false )
		{
			vDebug() << output;
		}
		return process.exitCode();
	};

	if( VeyonCore::platform().coreFunctions().isRunningAsAdmin() )
	{
		return runProcess( cliFilePath, arguments ) == 0 || setError( tr( "The operation failed." ) );
	}

#if defined(Q_OS_WIN)
	QStringList quotedArguments;
	quotedArguments.reserve( arguments.size() );
	for( auto argument : arguments )
	{
		if( argument.contains( QLatin1Char(' ') ) || argument.contains( QLatin1Char('"') ) || argument.isEmpty() )
		{
			argument.replace( QLatin1Char('"'), QStringLiteral("\\\"") );
			if( argument.endsWith( QLatin1Char('\\') ) )
			{
				argument.append( QLatin1Char('\\') );
			}
			argument = QLatin1Char('"') + argument + QLatin1Char('"');
		}
		quotedArguments.append( argument );
	}

	const auto parameters = quotedArguments.join( QLatin1Char(' ') );

	SHELLEXECUTEINFOW sei{};
	sei.cbSize = sizeof(sei);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
	sei.lpVerb = L"runas";
	sei.lpFile = reinterpret_cast<LPCWSTR>( cliFilePath.utf16() );
	sei.lpParameters = reinterpret_cast<LPCWSTR>( parameters.utf16() );
	sei.hwnd = GetForegroundWindow();
	sei.nShow = SW_HIDE;

	if( ShellExecuteExW( &sei ) == false || sei.hProcess == nullptr )
	{
		return setError( tr( "The operation has been cancelled or administrative privileges could not be obtained." ) );
	}

	while( WaitForSingleObject( sei.hProcess, 50 ) == WAIT_TIMEOUT )
	{
		QCoreApplication::processEvents( QEventLoop::ExcludeUserInputEvents );
	}

	DWORD exitCode = 1;
	GetExitCodeProcess( sei.hProcess, &exitCode );
	CloseHandle( sei.hProcess );

	return exitCode == 0 || setError( tr( "The operation failed." ) );

#elif defined(Q_OS_MACOS)
	// build a shell command line where every argument is single-quoted
	QStringList commandLine;
	commandLine.reserve( arguments.size() + 1 );
	for( const auto& argument : QStringList{ cliFilePath } + arguments )
	{
		auto quotedArgument = argument;
		quotedArgument.replace( QLatin1Char('\''), QStringLiteral("'\\''") );
		commandLine.append( QLatin1Char('\'') + quotedArgument + QLatin1Char('\'') );
	}

	// escape for AppleScript string literal
	auto script = commandLine.join( QLatin1Char(' ') );
	script.replace( QLatin1Char('\\'), QStringLiteral("\\\\") );
	script.replace( QLatin1Char('"'), QStringLiteral("\\\"") );

	const auto appleScript = QStringLiteral("do shell script \"%1\" with prompt \"%2\" with administrator privileges").
							 arg( script, tr( "Veyon wants to change its add-on configuration." ) );

	return runProcess( QStringLiteral("/usr/bin/osascript"), { QStringLiteral("-e"), appleScript } ) == 0 ||
			setError( tr( "The operation has been cancelled or administrative privileges could not be obtained." ) );
#else
	return runProcess( QStringLiteral("pkexec"), QStringList{ cliFilePath } + arguments ) == 0 ||
			setError( tr( "The operation has been cancelled or administrative privileges could not be obtained." ) );
#endif
}



AddOnManager::Package AddOnManager::readPackage( const QString& packageFilePath )
{
	QFile packageFile( packageFilePath );
	if( packageFile.open( QFile::ReadOnly ) == false )
	{
		return { {}, tr( "Could not open add-on package %1." ).arg( packageFilePath ) };
	}

	if( packageFile.size() > MaximumPackageSize )
	{
		return { {}, tr( "The add-on package %1 is too large." ).arg( packageFilePath ) };
	}

	return parsePackage( packageFile.readAll() );
}



AddOnManager::Package AddOnManager::parsePackage( const QByteArray& packageData )
{
	QJsonParseError parseError;
	const auto document = QJsonDocument::fromJson( packageData, &parseError );
	if( parseError.error != QJsonParseError::NoError || document.isObject() == false )
	{
		return { {}, tr( "The file is not a valid Veyon add-on package (%1)." ).arg( parseError.errorString() ) };
	}

	const auto manifest = document.object();

	const auto formatVersion = manifest[QStringLiteral("veyonAddOn")].toInt();
	if( formatVersion < 1 || formatVersion > PackageFormatVersion )
	{
		return { {}, tr( "The add-on package format is not supported by this version of Veyon." ) };
	}

	if( isValidPackageId( manifest[QStringLiteral("id")].toString() ) == false )
	{
		return { {}, tr( "The add-on package has an invalid ID." ) };
	}

	if( Plugin::Uid{ manifest[QStringLiteral("uid")].toString() }.isNull() )
	{
		return { {}, tr( "The add-on package has an invalid UID." ) };
	}

	if( manifest[QStringLiteral("name")].toString().trimmed().isEmpty() )
	{
		return { {}, tr( "The add-on package has no name." ) };
	}

	const auto type = manifest[QStringLiteral("type")].toString();
	if( type == QLatin1String("plugin") )
	{
		if( manifest[QStringLiteral("binaries")].toObject().isEmpty() )
		{
			return { {}, tr( "The add-on package does not contain any binaries." ) };
		}
	}
	else if( type == QLatin1String("script") )
	{
		if( manifest[QStringLiteral("actions")].toArray().isEmpty() )
		{
			return { {}, tr( "The script add-on does not contain any actions." ) };
		}
	}
	else
	{
		return { {}, tr( "The add-on package has an unknown type." ) };
	}

	const auto minimumVersion = QVersionNumber::fromString( manifest[QStringLiteral("minimumVeyonVersion")].toString() );
	if( minimumVersion.isNull() == false &&
		minimumVersion > QVersionNumber::fromString( VeyonCore::versionString() ) )
	{
		return { {}, tr( "The add-on requires Veyon %1 or newer." ).arg( minimumVersion.toString() ) };
	}

	return { manifest, {} };
}



bool AddOnManager::writePackage( const QString& packageFilePath, const QJsonObject& manifest, QString* errorString )
{
	auto packageManifest = manifest;
	packageManifest[QStringLiteral("veyonAddOn")] = PackageFormatVersion;

	const auto data = QJsonDocument( packageManifest ).toJson( QJsonDocument::Indented );
	const auto package = parsePackage( data );
	if( package.isValid() == false )
	{
		if( errorString )
		{
			*errorString = package.errorString;
		}
		return false;
	}

	QSaveFile file( packageFilePath );
	if( file.open( QFile::WriteOnly ) == false ||
		file.write( data ) != data.size() ||
		file.commit() == false )
	{
		if( errorString )
		{
			*errorString = tr( "Could not write file %1." ).arg( packageFilePath );
		}
		return false;
	}

	return true;
}



bool AddOnManager::addBinaryToPackage( QJsonObject& manifest, const QString& platformKey, const QString& binaryFilePath,
									   QString* errorString )
{
	QFile binaryFile( binaryFilePath );
	if( binaryFile.open( QFile::ReadOnly ) == false )
	{
		if( errorString )
		{
			*errorString = tr( "Could not open file %1." ).arg( binaryFilePath );
		}
		return false;
	}

	const auto data = binaryFile.readAll();

	auto binaries = manifest[QStringLiteral("binaries")].toObject();
	binaries[platformKey] = QJsonObject{
		{ QStringLiteral("fileName"), QFileInfo( binaryFilePath ).fileName() },
		{ QStringLiteral("size"), double(data.size()) },
		{ QStringLiteral("sha256"), QString::fromLatin1( QCryptographicHash::hash( data, QCryptographicHash::Sha256 ).toHex() ) },
		{ QStringLiteral("data"), QString::fromLatin1( data.toBase64() ) }
	};
	manifest[QStringLiteral("binaries")] = binaries;

	return true;
}



QString AddOnManager::currentPlatform()
{
#if defined(Q_OS_WIN)
	return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
	return QStringLiteral("macos");
#elif defined(Q_OS_ANDROID)
	return QStringLiteral("android");
#else
	return QStringLiteral("linux");
#endif
}



QStringList AddOnManager::currentPlatformKeys()
{
	const auto platform = currentPlatform();
	auto architecture = QSysInfo::buildCpuArchitecture();
	if( architecture == QLatin1String("x86_64") || architecture == QLatin1String("amd64") )
	{
		architecture = QStringLiteral("x86_64");
	}

	QStringList keys{ QStringLiteral("%1-%2").arg( platform, architecture ) };
	if( platform == QLatin1String("macos") )
	{
		keys.append( QStringLiteral("macos-universal") );
	}
	keys.append( platform );

	return keys;
}



QString AddOnManager::systemAddOnsDirectory()
{
	return VeyonCore::filesystem().addOnsDirectoryPath();
}



QString AddOnManager::userAddOnsDirectory()
{
	return VeyonCore::filesystem().userAddOnsDirectoryPath();
}



QList<QJsonObject> AddOnManager::installedManifests( const QString& packageType, bool includeUserPackages, bool includeDisabled )
{
	QList<QJsonObject> manifests;

	QList<std::pair<QString, Origin>> baseDirectories{ { systemAddOnsDirectory(), Origin::Installed } };
	if( includeUserPackages && VeyonCore::config().userAddOnsEnabled() )
	{
		baseDirectories.append( { userAddOnsDirectory(), Origin::UserInstalled } );
	}

	for( const auto& [baseDirectory, origin] : std::as_const(baseDirectories) )
	{
		const auto packageDirs = QDir( baseDirectory ).entryInfoList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name );
		for( const auto& packageDir : packageDirs )
		{
			if( packageDir.fileName().startsWith( QLatin1Char('.') ) )
			{
				continue;
			}

			auto manifest = readManifest( packageDir.filePath() );
			if( manifest.isEmpty() || manifest[QStringLiteral("type")].toString() != packageType )
			{
				continue;
			}

			const auto addOn = addOnFromManifest( manifest, packageDir.filePath(), origin );
			if( includeDisabled == false && addOn.enabled == false )
			{
				continue;
			}

			manifest[QStringLiteral("_location")] = packageDir.filePath();
			manifest[QStringLiteral("_userPackage")] = origin == Origin::UserInstalled;
			manifests.append( manifest );
		}
	}

	return manifests;
}



QJsonObject AddOnManager::readManifest( const QString& packageDirectory )
{
	QFile manifestFile( packageDirectory + QDir::separator() + QLatin1String(ManifestFileName) );
	if( manifestFile.open( QFile::ReadOnly ) == false )
	{
		return {};
	}

	const auto manifest = QJsonDocument::fromJson( manifestFile.readAll() ).object();
	if( isValidPackageId( manifest[QStringLiteral("id")].toString() ) == false ||
		Plugin::Uid{ manifest[QStringLiteral("uid")].toString() }.isNull() )
	{
		return {};
	}

	return manifest;
}



AddOnInterface::Category AddOnManager::categoryFromString( const QString& category )
{
	bool ok = false;
	const auto value = QMetaEnum::fromType<AddOnInterface::Category>().keyToValue( category.toLatin1().constData(), &ok );
	if( ok )
	{
		return AddOnInterface::Category( value );
	}

	return AddOnInterface::Category::Other;
}



QString AddOnManager::categoryToString( AddOnInterface::Category category )
{
	return QLatin1String( QMetaEnum::fromType<AddOnInterface::Category>().valueToKey( int(category) ) );
}



QString AddOnManager::categoryDisplayName( AddOnInterface::Category category )
{
	switch( category )
	{
	case AddOnInterface::Category::Monitoring: return tr( "Monitoring" );
	case AddOnInterface::Category::Control: return tr( "Control" );
	case AddOnInterface::Category::Files: return tr( "Files" );
	case AddOnInterface::Category::Network: return tr( "Network & Internet" );
	case AddOnInterface::Category::Communication: return tr( "Communication" );
	case AddOnInterface::Category::Scripting: return tr( "Custom scripts" );
	case AddOnInterface::Category::Other: break;
	}

	return tr( "Other" );
}



bool AddOnManager::isValidPackageId( const QString& id )
{
	static const QRegularExpression idRX( QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{1,99}$") );
	return idRX.match( id ).hasMatch() && id.contains( QStringLiteral("..") ) == false;
}



QString AddOnManager::humanReadableName( const QString& camelCaseName )
{
	QString result;
	result.reserve( camelCaseName.size() + 8 );

	for( int i = 0; i < camelCaseName.size(); ++i )
	{
		const auto c = camelCaseName.at( i );
		if( i > 0 && c.isUpper() &&
			( camelCaseName.at( i - 1 ).isLower() ||
			  ( i + 1 < camelCaseName.size() && camelCaseName.at( i + 1 ).isLower() && camelCaseName.at( i - 1 ).isUpper() ) ) )
		{
			result.append( QLatin1Char(' ') );
		}
		result.append( c );
	}

	return result;
}



void AddOnManager::markScriptPackageLoaded( Plugin::Uid uid )
{
	QMutexLocker locker( &loadedScriptPackagesMutex );
	if( loadedScriptPackages.contains( uid ) == false )
	{
		loadedScriptPackages.append( uid );
	}
}



AddOnManager::AddOn AddOnManager::addOnFromPluginObject( QObject* pluginObject, bool loaded ) const
{
	const auto pluginInterface = qobject_cast<PluginInterface *>( pluginObject );
	if( pluginInterface == nullptr )
	{
		return {};
	}

	AddOn addOn;
	addOn.uid = pluginInterface->uid();
	addOn.id = pluginInterface->name();
	addOn.name = humanReadableName( pluginInterface->name() );
	addOn.description = pluginInterface->description();
	addOn.vendor = pluginInterface->vendor();
	addOn.version = pluginInterface->version().toString();
	addOn.loaded = loaded;
	addOn.origin = Origin::Bundled;
	addOn.packageType = QStringLiteral("plugin");
	addOn.location = VeyonCore::pluginManager().pluginFilePath( addOn.uid );

	const auto addOnInterface = qobject_cast<AddOnInterface *>( pluginObject );
	const auto featureProviderInterface = qobject_cast<FeatureProviderInterface *>( pluginObject );
	const auto isPlatformPlugin = qobject_cast<PlatformPluginInterface *>( pluginObject ) != nullptr;

	if( addOnInterface && isPlatformPlugin == false )
	{
		addOn.kind = Kind::AddOn;
		if( addOnInterface->addOnName().isEmpty() == false )
		{
			addOn.name = addOnInterface->addOnName();
		}
		addOn.category = addOnInterface->addOnCategory();
		addOn.iconUrl = addOnInterface->addOnIconUrl();
		addOn.summary = addOnInterface->addOnSummary();
		addOn.clientPlatforms = addOnInterface->addOnClientPlatforms();
	}
	else if( featureProviderInterface && isPlatformPlugin == false && addOn.location.isEmpty() == false )
	{
		addOn.kind = Kind::BuiltinFeature;

		QStringList featureNames;
		for( const auto& feature : featureProviderInterface->featureList() )
		{
			if( feature.testFlag( Feature::Flag::Master ) && feature.testFlag( Feature::Flag::Meta ) == false &&
				feature.displayName().isEmpty() == false )
			{
				featureNames.append( feature.displayName() );
				if( addOn.iconUrl.isEmpty() )
				{
					addOn.iconUrl = feature.iconUrl();
				}
			}
		}
		if( featureNames.isEmpty() == false )
		{
			addOn.summary = tr( "Provides: %1" ).arg( featureNames.join( QStringLiteral(", ") ) );
		}
	}
	else
	{
		addOn.kind = Kind::SystemComponent;
		addOn.canBeDisabled = false;
	}

	addOn.enabled = addOn.canBeDisabled == false || isDisabledInConfiguration( addOn.uid ) == false;

	return addOn;
}



AddOnManager::AddOn AddOnManager::addOnFromManifest( const QJsonObject& manifest, const QString& location, Origin origin )
{
	AddOn addOn;
	addOn.uid = Plugin::Uid{ manifest[QStringLiteral("uid")].toString() };
	addOn.id = manifest[QStringLiteral("id")].toString();
	addOn.name = manifest[QStringLiteral("name")].toString();
	addOn.description = manifest[QStringLiteral("description")].toString();
	addOn.summary = manifest[QStringLiteral("summary")].toString();
	addOn.vendor = manifest[QStringLiteral("vendor")].toString();
	addOn.version = manifest[QStringLiteral("version")].toString();
	addOn.category = categoryFromString( manifest[QStringLiteral("category")].toString() );
	addOn.kind = Kind::AddOn;
	addOn.origin = origin;
	addOn.packageType = manifest[QStringLiteral("type")].toString();
	addOn.location = location;
	addOn.removable = true;
	addOn.canBeDisabled = true;

	for( const auto& platform : manifest[QStringLiteral("clientPlatforms")].toArray() )
	{
		addOn.clientPlatforms.append( platform.toString() );
	}

	QFile iconFile( location + QDir::separator() + QLatin1String(IconFileName) );
	if( iconFile.open( QFile::ReadOnly ) )
	{
		addOn.iconData = iconFile.readAll();
	}

	if( origin == Origin::UserInstalled )
	{
		addOn.enabled = QFile::exists( location + QDir::separator() + QLatin1String(UserDisabledMarkerFileName) ) == false;
	}
	else
	{
		addOn.enabled = isDisabledInConfiguration( addOn.uid ) == false;
	}

	return addOn;
}



bool AddOnManager::isDisabledInConfiguration( Plugin::Uid uid )
{
	const auto disabledAddOns = VeyonCore::config().disabledAddOns();
	return std::find_if( disabledAddOns.cbegin(), disabledAddOns.cend(),
						 [&uid]( const QString& entry ) { return Plugin::Uid{entry} == uid; } ) != disabledAddOns.cend();
}



bool AddOnManager::removeDirectory( const QString& path, const QString& baseDirectory )
{
	QDir dir( path );
	if( dir.exists() == false )
	{
		return true;
	}

	// safety check: only ever remove directories inside the add-ons base directory
	const auto canonicalBase = QDir( baseDirectory ).canonicalPath();
	const auto canonicalPath = dir.canonicalPath();
	if( canonicalBase.isEmpty() ||
		canonicalPath.startsWith( canonicalBase + QLatin1Char('/') ) == false )
	{
		vCritical() << "refusing to remove directory" << path << "outside of" << baseDirectory;
		return false;
	}

	if( dir.removeRecursively() )
	{
		return true;
	}

	// files might still be in use (e.g. loaded plugins on Windows) - move directory out of the way
	const auto trashPath = baseDirectory + QDir::separator() +
						   QStringLiteral(".trash-%1").arg( QUuid::createUuid().toString( QUuid::WithoutBraces ) );
	return QDir().rename( path, trashPath );
}
