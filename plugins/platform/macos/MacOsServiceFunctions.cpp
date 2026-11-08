/*
 * MacOsServiceFunctions.cpp - implementation of MacOsServiceFunctions class
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
#include <QProcess>
#include <QSaveFile>

#include <sys/stat.h>
#include <unistd.h>

#include "MacOsServiceCore.h"
#include "MacOsServiceFunctions.h"


QString MacOsServiceFunctions::veyonServiceName() const
{
	return QStringLiteral("io.veyon.veyon-service");
}



bool MacOsServiceFunctions::isRegistered( const QString& name )
{
	return QFile::exists( plistFilePath( name ) );
}



bool MacOsServiceFunctions::isRunning( const QString& name )
{
	QByteArray output;
	return launchctl( { QStringLiteral("print"), serviceTarget( name ) }, &output ) == 0 &&
			output.contains( "state = running" );
}



bool MacOsServiceFunctions::start( const QString& name )
{
	if( isLoaded( name ) == false &&
		launchctl( { QStringLiteral("bootstrap"), QStringLiteral("system"), plistFilePath( name ) } ) != 0 )
	{
		vCritical() << "could not load service" << name;
		return false;
	}

	return launchctl( { QStringLiteral("kickstart"), serviceTarget( name ) } ) == 0;
}



bool MacOsServiceFunctions::stop( const QString& name )
{
	if( isLoaded( name ) == false )
	{
		return true;
	}

	// unload the job as launchd would restart it otherwise (KeepAlive)
	return launchctl( { QStringLiteral("bootout"), serviceTarget( name ) } ) == 0;
}



bool MacOsServiceFunctions::install( const QString& name, const QString& filePath,
									 StartMode startMode, const QString& displayName )
{
	Q_UNUSED(displayName)

	const auto plist = QStringLiteral(
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
		"<plist version=\"1.0\">\n"
		"<dict>\n"
		"\t<key>Label</key>\n"
		"\t<string>%1</string>\n"
		"\t<key>ProgramArguments</key>\n"
		"\t<array>\n"
		"\t\t<string>%2</string>\n"
		"\t</array>\n"
		"\t<key>RunAtLoad</key>\n"
		"\t<true/>\n"
		"\t<key>KeepAlive</key>\n"
		"\t<true/>\n"
		"\t<key>ThrottleInterval</key>\n"
		"\t<integer>5</integer>\n"
		"\t<key>ProcessType</key>\n"
		"\t<string>Interactive</string>\n"
		"\t<key>StandardErrorPath</key>\n"
		"\t<string>/Library/Logs/Veyon/veyon-service-stderr.log</string>\n"
		"</dict>\n"
		"</plist>\n" ).arg( name.toHtmlEscaped(), filePath.toHtmlEscaped() );

	QFile::remove( plistFilePath( name ) );

	QSaveFile plistFile( plistFilePath( name ) );
	if( plistFile.open( QFile::WriteOnly ) == false ||
		plistFile.write( plist.toUtf8() ) < 0 ||
		plistFile.commit() == false )
	{
		vCritical() << "could not write" << plistFilePath( name );
		return false;
	}

	// launchd refuses plists which are writable by others or not owned by root
	const auto plistPath = plistFilePath( name ).toUtf8();
	if( chown( plistPath.constData(), 0, 0 ) != 0 || chmod( plistPath.constData(), 0644 ) != 0 ) // Flawfinder:ignore
	{
		vWarning() << "could not adjust ownership/permissions of" << plistFilePath( name );
	}

	// launchd does not create the directory for the log file
	QDir().mkpath( QStringLiteral("/Library/Logs/Veyon") );

	if( setStartMode( name, startMode ) == false )
	{
		return false;
	}

	return startMode == StartMode::Disabled ||
			launchctl( { QStringLiteral("bootstrap"), QStringLiteral("system"), plistFilePath( name ) } ) == 0 ||
			isLoaded( name );
}



bool MacOsServiceFunctions::uninstall( const QString& name )
{
	stop( name );

	return QFile::exists( plistFilePath( name ) ) == false || QFile::remove( plistFilePath( name ) );
}



bool MacOsServiceFunctions::setStartMode( const QString& name, StartMode startMode )
{
	// launchd does not know manually started daemons - treat "manual" as not starting at boot
	return launchctl( { startMode == StartMode::Auto ? QStringLiteral("enable") : QStringLiteral("disable"),
						serviceTarget( name ) } ) == 0;
}



bool MacOsServiceFunctions::runAsService( const QString& name, const ServiceEntryPoint& serviceEntryPoint )
{
	Q_UNUSED(name)

	serviceEntryPoint();

	return true;
}



void MacOsServiceFunctions::manageServerInstances()
{
	MacOsServiceCore serviceCore;
	serviceCore.run();
}



int MacOsServiceFunctions::launchctl( const QStringList& arguments, QByteArray* output )
{
	QProcess process;
	process.setProcessChannelMode( QProcess::MergedChannels );
	process.start( QStringLiteral("/bin/launchctl"), arguments );
	if( process.waitForFinished( 30000 ) == false || process.exitStatus() != QProcess::NormalExit )
	{
		return -1;
	}

	if( output )
	{
		*output = process.readAll();
	}

	return process.exitCode();
}



QString MacOsServiceFunctions::plistFilePath( const QString& name )
{
	return QStringLiteral("/Library/LaunchDaemons/%1.plist").arg( name );
}



QString MacOsServiceFunctions::serviceTarget( const QString& name )
{
	return QStringLiteral("system/%1").arg( name );
}



bool MacOsServiceFunctions::isLoaded( const QString& name )
{
	return launchctl( { QStringLiteral("print"), serviceTarget( name ) } ) == 0;
}
