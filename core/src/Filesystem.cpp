/*
 * Filesystem.cpp - filesystem related query and manipulation functions
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

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHostInfo>
#include <QStandardPaths>

#ifdef Q_OS_UNIX
#include <pwd.h>
#include <unistd.h>
#endif

#include "VeyonConfiguration.h"
#include "Filesystem.h"
#include "PlatformCoreFunctions.h"
#include "PlatformFilesystemFunctions.h"
#include "PlatformUserFunctions.h"


namespace {

// The user on whose behalf Veyon Configurator runs while it has administrative privileges
struct ElevatingUser
{
	QString home;
	QStringList processHomes;	// spellings of the home directory of the elevated process itself (root)
#ifdef Q_OS_UNIX
	uid_t uid = 0;
	gid_t gid = 0;
#endif

	bool isValid() const
	{
		return home.isEmpty() == false;
	}
};

const ElevatingUser& elevatingUser()
{
	// the user can't change while the program is running
	static const ElevatingUser user = []() {
		ElevatingUser result;
#ifdef Q_OS_UNIX
		// on Windows the elevated program keeps the profile of the user, so there's nothing to do
		if( VeyonCore::component() != VeyonCore::Component::Configurator || geteuid() != 0 )
		{
			return result;
		}

		const passwd* entry = nullptr;

		// pkexec (used on Linux) and sudo tell the UID of the calling user
		for( const auto* variable : { "PKEXEC_UID", "SUDO_UID" } )
		{
			bool ok = false;
			const auto uid = qEnvironmentVariableIntValue( variable, &ok );
			if( ok && uid > 0 )
			{
				entry = getpwuid( uid_t(uid) );
				break;
			}
		}

		// macOS ("do shell script … with administrator privileges") doesn't, but the administrator credentials
		// have been entered by the user logged in at the console, which the platform plugin reports for root
		if( entry == nullptr )
		{
			const auto loginName = VeyonCore::platform().userFunctions().
								   queryCurrentUserProperty( PlatformUserFunctions::UserProperty::LoginName );
			if( loginName.isEmpty() == false && loginName != QLatin1String("root") )
			{
				entry = getpwnam( loginName.toUtf8().constData() );
			}
		}

		if( entry && entry->pw_uid != 0 && entry->pw_dir && *entry->pw_dir )
		{
			result.home = QDir::cleanPath( QFile::decodeName( entry->pw_dir ) );
			result.uid = entry->pw_uid;
			result.gid = entry->pw_gid;

			// the home of the elevated process may be taken from $HOME (QDir::homePath()) or from the user
			// database (e.g. QStandardPaths on macOS), possibly with symlinks resolved (/var/root is
			// /private/var/root on macOS), so all spellings are mapped to the home of the user
			QStringList processHomes{ QDir::homePath() };
			const auto rootEntry = getpwuid( geteuid() );
			if( rootEntry && rootEntry->pw_dir )
			{
				processHomes.append( QFile::decodeName( rootEntry->pw_dir ) );
			}
			for( const auto& processHome : std::as_const( processHomes ) )
			{
				for( const auto& spelling : { QDir::cleanPath( processHome ), QFileInfo( processHome ).canonicalFilePath() } )
				{
					if( spelling.isEmpty() == false && spelling != QLatin1String("/") && spelling != result.home &&
						result.processHomes.contains( spelling ) == false )
					{
						result.processHomes.append( spelling );
					}
				}
			}

			vDebug() << "acting on behalf of user with home directory" << result.home;
		}
#endif
		return result;
	}();

	return user;
}



// Maps a path inside the home directory of the elevated process to the same path inside the user's home
QString toUserPath( const QString& path )
{
	const auto& user = elevatingUser();
	if( user.isValid() == false || path.isEmpty() )
	{
		return path;
	}

	const auto cleanPath = QDir::cleanPath( path );
	for( const auto& processHome : user.processHomes )
	{
		if( cleanPath == processHome || cleanPath.startsWith( processHome + QLatin1Char('/') ) )
		{
			return user.home + cleanPath.mid( processHome.length() );
		}
	}

	return path;
}

}



QString Filesystem::userHomePath() const
{
	const auto& user = elevatingUser();

	return user.isValid() ? user.home : QDir::homePath();
}



QString Filesystem::userStandardLocation( QStandardPaths::StandardLocation location ) const
{
	return toUserPath( QStandardPaths::writableLocation( location ) );
}



void Filesystem::transferOwnershipToUser( const QString& filePath ) const
{
#ifdef Q_OS_UNIX
	const auto& user = elevatingUser();
	if( user.isValid() == false || filePath.isEmpty() )
	{
		return;
	}

	// lchown() doesn't follow symbolic links, so only the file just written can be affected
	if( lchown( QFile::encodeName( filePath ).constData(), user.uid, user.gid ) != 0 ) // Flawfinder:ignore
	{
		vWarning() << "could not transfer ownership of" << filePath << "to the user";
	}
#else
	Q_UNUSED(filePath)
#endif
}



QString Filesystem::expandPath( QString path ) const
{
#ifndef Q_OS_WIN
	// configurations are often created on Windows (or imported from there) and contain paths such as
	// "%HOME%\Desktop" - QDir::toNativeSeparators() doesn't convert backslashes on Unix platforms
	path.replace( QLatin1Char('\\'), QLatin1Char('/') );
#endif

	const auto& filesystemFunctions = VeyonCore::platform().filesystemFunctions();
	const auto homePath = userHomePath();
	const auto personalAppDataPath = toUserPath( filesystemFunctions.personalAppDataPath() );
	const auto logPath = VeyonCore::platform().coreFunctions().isRunningAsAdmin() ?
							 filesystemFunctions.globalLogPath() : filesystemFunctions.personalLogPath();

	const auto p = QDir::toNativeSeparators( path.replace( QStringLiteral( "%LOGS%" ), logPath ).
											 replace( QStringLiteral( "$LOGS" ), logPath ).
											 replace( QStringLiteral( "%HOME%" ), homePath ).
											 replace( QStringLiteral( "$HOME" ), homePath ).
											 replace( QStringLiteral( "%HOSTNAME%" ), QHostInfo::localHostName() ).
											 replace( QStringLiteral( "$HOSTNAME" ), QHostInfo::localHostName() ).
											 replace( QStringLiteral( "%PROFILE%" ), homePath ).
											 replace( QStringLiteral( "$PROFILE" ), homePath ).
											 replace(QStringLiteral("%USER%"), VeyonCore::platform().userFunctions().queryCurrentUserProperty(PlatformUserFunctions::UserProperty::LoginName)).
											 replace(QStringLiteral("%DESKTOP%"), userStandardLocation(QStandardPaths::DesktopLocation)).
											 replace(QStringLiteral("%DOCUMENTS%"), userStandardLocation(QStandardPaths::DocumentsLocation)).
											 replace(QStringLiteral("%DOWNLOADS%"), userStandardLocation(QStandardPaths::DownloadLocation)).
											 replace(QStringLiteral("%PICTURES%"), userStandardLocation(QStandardPaths::PicturesLocation)).
											 replace(QStringLiteral("%VIDEOS%"), userStandardLocation(QStandardPaths::MoviesLocation)).
											 replace( QStringLiteral( "%APPDATA%" ), personalAppDataPath ).
											 replace( QStringLiteral( "$APPDATA" ), personalAppDataPath ).
											 replace( QStringLiteral( "%GLOBALAPPDATA%" ), VeyonCore::platform().filesystemFunctions().globalAppDataPath() ).
											 replace( QStringLiteral( "$GLOBALAPPDATA" ), VeyonCore::platform().filesystemFunctions().globalAppDataPath() ).
											 replace( QStringLiteral( "%TMP%" ), QDir::tempPath() ).
											 replace( QStringLiteral( "$TMP" ), QDir::tempPath() ).
											 replace( QStringLiteral( "%TEMP%" ), QDir::tempPath() ).
											 replace( QStringLiteral( "$TEMP" ), QDir::tempPath() ) );

	// remove duplicate directory separators - however skip the first two chars
	// as they might specify an UNC path on Windows
	if( p.length() > 3 )
	{
		return p.left( 2 ) + p.mid( 2 ).replace(
					QString( QStringLiteral( "%1%1" ) ).arg( QDir::separator() ),
					QDir::separator() );
	}

	return p;
}



QString Filesystem::pathPlaceholdersDescription()
{
	const QList<QPair<QString, QString>> placeholders{
		{ QStringLiteral("%HOME%"), tr("Home directory of the current user") },
		{ QStringLiteral("%USER%"), tr("Login name of the current user") },
		{ QStringLiteral("%HOSTNAME%"), tr("Name of the computer") },
		{ QStringLiteral("%APPDATA%"), tr("Veyon data directory of the current user") },
		{ QStringLiteral("%GLOBALAPPDATA%"), tr("Veyon data directory shared by all users") },
		{ QStringLiteral("%TEMP%"), tr("Directory for temporary files") },
		{ QStringLiteral("%LOGS%"), tr("Directory for Veyon log files") },
		{ QStringLiteral("%DESKTOP%"), tr("Desktop directory of the current user") },
		{ QStringLiteral("%DOCUMENTS%"), tr("Documents directory of the current user") },
		{ QStringLiteral("%DOWNLOADS%"), tr("Downloads directory of the current user") },
		{ QStringLiteral("%PICTURES%"), tr("Pictures directory of the current user") },
		{ QStringLiteral("%VIDEOS%"), tr("Videos directory of the current user (Movies on macOS)") },
	};

	QString rows;
	for( const auto& placeholder : placeholders )
	{
		// multi-argument arg() replaces in a single pass so the % characters of the placeholders are kept
		rows += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td></tr>").arg( placeholder.first,
																			  placeholder.second.toHtmlEscaped() );
	}

	return QStringLiteral("<p>%1</p><table cellspacing=\"2\">%2</table>").arg(
				tr( "The following placeholders can be used. They are replaced with the matching "
					"values on each computer." ).toHtmlEscaped(), rows );
}



QString Filesystem::shrinkPath( QString path ) const
{
#ifndef Q_OS_WIN
	path.replace( QLatin1Char('\\'), QLatin1Char('/') );
#endif
	path = QDir::toNativeSeparators( path );

	const auto tempPath = QDir::toNativeSeparators(QDir::tempPath());
	const auto globalTempPath = QDir::toNativeSeparators(VeyonCore::platform().filesystemFunctions().globalTempPath());
	// the log paths equal the temporary paths on most platforms - map them to %LOGS% only where they are distinct
	// (macOS) so that %TEMP% keeps its meaning everywhere else
	auto globalLogPath = QDir::toNativeSeparators(VeyonCore::platform().filesystemFunctions().globalLogPath());
	auto personalLogPath = QDir::toNativeSeparators(toUserPath(VeyonCore::platform().filesystemFunctions().personalLogPath()));
	if (globalLogPath == globalTempPath || globalLogPath == tempPath)
	{
		globalLogPath.clear();
	}
	if (personalLogPath == tempPath)
	{
		personalLogPath.clear();
	}
	const auto personalAppDataPath = toUserPath(VeyonCore::platform().filesystemFunctions().personalAppDataPath());
	const auto globalAppDataPath = VeyonCore::platform().filesystemFunctions().globalAppDataPath();
	const auto desktopPath = userStandardLocation(QStandardPaths::DesktopLocation);
	const auto documentsPath = userStandardLocation(QStandardPaths::DocumentsLocation);
	const auto downloadsPath = userStandardLocation(QStandardPaths::DownloadLocation);
	const auto picturesPath = userStandardLocation(QStandardPaths::PicturesLocation);
	const auto videosPath = userStandardLocation(QStandardPaths::MoviesLocation);
	const auto homePath = QDir::toNativeSeparators(userHomePath());

	for (const auto& mapping : {
			 qMakePair(tempPath, QStringLiteral("TEMP")),
			 qMakePair(globalLogPath, QStringLiteral("LOGS")),
			 qMakePair(personalLogPath, QStringLiteral("LOGS")),
			 qMakePair(personalAppDataPath, QStringLiteral("APPDATA")),
			 qMakePair(globalAppDataPath, QStringLiteral("GLOBALAPPDATA")),
			 qMakePair(desktopPath, QStringLiteral("DESKTOP")),
			 qMakePair(documentsPath, QStringLiteral("DOCUMENTS")),
			 qMakePair(downloadsPath, QStringLiteral("DOWNLOADS")),
			 qMakePair(picturesPath, QStringLiteral("PICTURES")),
			 qMakePair(videosPath, QStringLiteral("VIDEOS")),
			 qMakePair(homePath, QStringLiteral("HOME")),
			 })
	{
		if (mapping.first.isEmpty() == false && path.startsWith(mapping.first))
		{
			path.replace(mapping.first, QStringLiteral("%%1%").arg(mapping.second));
			break;
		}
	}

	// remove duplicate directory separators - however skip the first two chars
	// as they might specify an UNC path on Windows
	if( path.length() > 3 )
	{
		return QDir::toNativeSeparators( path.left( 2 ) + path.mid( 2 ).replace(
											 QString( QStringLiteral( "%1%1" ) ).arg( QDir::separator() ), QDir::separator() ) );
	}

	return QDir::toNativeSeparators( path );
}




bool Filesystem::ensurePathExists( const QString &path ) const
{
	const QString expandedPath = VeyonCore::filesystem().expandPath( path );

	if( path.isEmpty() || QDir( expandedPath ).exists() )
	{
		return true;
	}

	vDebug() << "creating " << path << "=>" << expandedPath;

	QString p = expandedPath;

	QStringList dirs;
	while( !QDir( p ).exists() && !p.isEmpty() )
	{
		dirs.push_front( QDir( p ).dirName() );
		p.chop( dirs.front().size() + 1 );
	}

	if( !p.isEmpty() )
	{
		return QDir( p ).mkpath( dirs.join( QDir::separator() ) );
	}

	return false;
}



QString Filesystem::privateKeyPath( const QString& name ) const
{
	const QString d = VeyonCore::filesystem().expandPath( VeyonCore::config().privateKeyBaseDir() ) +
			QDir::separator() + name + QDir::separator() + QStringLiteral( "key" );

	return QDir::toNativeSeparators( d );
}



QString Filesystem::publicKeyPath( const QString& name ) const
{
	const QString d = VeyonCore::filesystem().expandPath( VeyonCore::config().publicKeyBaseDir() ) +
			QDir::separator() + name + QDir::separator() + QStringLiteral( "key" );

	return QDir::toNativeSeparators( d );
}



QString Filesystem::screenshotDirectoryPath() const
{
	return expandPath( VeyonCore::config().screenshotDirectory() );
}



QString Filesystem::programFilePath( const QString& name ) const
{
	const auto fileName = name + VeyonCore::executableSuffix();

#ifdef Q_OS_MACOS
	// all programs except Veyon Master are located in Veyon.app/Contents/Helpers, as macOS would otherwise treat
	// them as instances of Veyon.app (see cmake/modules/MacOSBundle.cmake) - works from both MacOS and Helpers
	const auto helperFilePath = QDir::cleanPath( QCoreApplication::applicationDirPath() + QStringLiteral("/../Helpers/") + fileName );
	if( QFileInfo::exists( helperFilePath ) )
	{
		return helperFilePath;
	}
#endif

	return QDir::toNativeSeparators( QCoreApplication::applicationDirPath() + QDir::separator() + fileName );
}



QString Filesystem::serverFilePath() const
{
	return programFilePath( QStringLiteral("veyon-server") );
}



QString Filesystem::workerFilePath() const
{
	return programFilePath( QStringLiteral("veyon-worker") );
}



QString Filesystem::cliFilePath() const
{
	return programFilePath( QStringLiteral("veyon-cli") );
}



QString Filesystem::addOnsDirectoryPath() const
{
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
	// GLOBALAPPDATA is /etc/veyon on Linux which is no place for binaries
	return QStringLiteral("/var/lib/veyon/addons");
#else
	return QDir::toNativeSeparators( VeyonCore::platform().filesystemFunctions().globalAppDataPath() +
									 QDir::separator() + QStringLiteral("AddOns") );
#endif
}



QString Filesystem::userAddOnsDirectoryPath() const
{
	return QDir::toNativeSeparators( VeyonCore::platform().filesystemFunctions().personalAppDataPath() +
									 QDir::separator() + QStringLiteral("AddOns") );
}
