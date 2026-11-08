/*
 * MacOsCoreFunctions.cpp - implementation of MacOsCoreFunctions class
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

#include <QFileInfo>
#include <QProcess>
#include <QScreen>
#include <QSocketNotifier>
#include <QTextStream>
#include <QWidget>

#include <IOKit/pwr_mgt/IOPMLib.h>
#include <libproc.h>
#include <sys/param.h>
#include <syslog.h>
#include <unistd.h>

#include "MacOsCocoaHelpers.h"
#include "MacOsCoreFunctions.h"
#include "MacOsUserFunctions.h"
#include "StandardInputReader.h"


MacOsCoreFunctions::~MacOsCoreFunctions()
{
	restoreScreenSaverSettings();
}



bool MacOsCoreFunctions::applyConfiguration()
{
	return true;
}



bool MacOsCoreFunctions::prepareSessionBusAccess()
{
	// there's no session bus on macOS
	return true;
}



void MacOsCoreFunctions::initNativeLoggingSystem( const QString& appName )
{
	m_syslogIdentity = QStringLiteral("veyon-%1").arg( appName.toLower() ).toUtf8();
	openlog( m_syslogIdentity.constData(), LOG_PID, LOG_USER );
}



void MacOsCoreFunctions::writeToNativeLoggingSystem( const QString& message, Logger::LogLevel loglevel )
{
	int priority = LOG_INFO;

	switch( loglevel )
	{
	case Logger::LogLevel::Critical: priority = LOG_CRIT; break;
	case Logger::LogLevel::Error: priority = LOG_ERR; break;
	case Logger::LogLevel::Warning: priority = LOG_WARNING; break;
	case Logger::LogLevel::Debug: priority = LOG_DEBUG; break;
	default: break;
	}

	syslog( priority, "%s", message.toUtf8().constData() );
}



StandardInputReader* MacOsCoreFunctions::createStandardInputReader( QObject* parent )
{
	auto reader = new StandardInputReader( parent );

	auto notifier = new QSocketNotifier( STDIN_FILENO, QSocketNotifier::Read, reader );
	QObject::connect( notifier, &QSocketNotifier::activated,
					  reader, [reader]() {
		reader->lineRead( QTextStream( stdin ).readLine() );
	} );

	return reader;
}



void MacOsCoreFunctions::reboot()
{
	if( isRunningAsAdmin() )
	{
		QProcess::startDetached( QLatin1String(ShutdownCommand), { QStringLiteral("-r"), QStringLiteral("now") } );
	}
	else
	{
		QProcess::startDetached( QStringLiteral("/usr/bin/osascript"),
								 { QStringLiteral("-e"), QStringLiteral("tell application \"System Events\" to restart") } );
	}
}



void MacOsCoreFunctions::powerDown( bool installUpdates )
{
	Q_UNUSED(installUpdates)

	if( isRunningAsAdmin() )
	{
		QProcess::startDetached( QLatin1String(ShutdownCommand), { QStringLiteral("-h"), QStringLiteral("now") } );
	}
	else
	{
		QProcess::startDetached( QStringLiteral("/usr/bin/osascript"),
								 { QStringLiteral("-e"), QStringLiteral("tell application \"System Events\" to shut down") } );
	}
}



void MacOsCoreFunctions::raiseWindow( QWidget* widget, bool stayOnTop )
{
	if( widget == nullptr )
	{
		return;
	}

	if( stayOnTop )
	{
		const auto isVisible = widget->isVisible();
		const auto isFullscreen = widget->isFullScreen();

		widget->setWindowFlag( Qt::WindowStaysOnTopHint, true );

		if( isFullscreen )
		{
			widget->showFullScreen();
		}
		else if( isVisible )
		{
			widget->show();
		}

		MacOsCocoaHelpers::setWindowAboveEverything( widget, true );
	}

	MacOsCocoaHelpers::activateApplication();

	widget->activateWindow();
	widget->raise();
}



void MacOsCoreFunctions::disableScreenSaver()
{
	if( m_powerAssertionActive )
	{
		return;
	}

	IOPMAssertionID assertionId = 0;
	const auto reason = QStringLiteral("Veyon").toCFString();
	if( IOPMAssertionCreateWithName( kIOPMAssertionTypePreventUserIdleDisplaySleep,
									 kIOPMAssertionLevelOn, reason, &assertionId ) == kIOReturnSuccess )
	{
		m_powerAssertionId = assertionId;
		m_powerAssertionActive = true;
	}
	else
	{
		vWarning() << "could not create power management assertion";
	}
	CFRelease( reason );
}



void MacOsCoreFunctions::restoreScreenSaverSettings()
{
	if( m_powerAssertionActive )
	{
		IOPMAssertionRelease( m_powerAssertionId );
		m_powerAssertionActive = false;
	}
}



void MacOsCoreFunctions::setSystemUiState( bool enabled )
{
	MacOsCocoaHelpers::setKioskMode( enabled == false );
}



QString MacOsCoreFunctions::activeDesktopName()
{
	return {};
}



bool MacOsCoreFunctions::isRunningAsAdmin() const
{
	return getuid() == 0 || geteuid() == 0;
}



bool MacOsCoreFunctions::runProgramAsAdmin( const QString& program, const QStringList& parameters )
{
	QStringList commandLine{ shellQuote( program ) };
	for( const auto& parameter : parameters )
	{
		commandLine.append( shellQuote( parameter ) );
	}

	// run program in background so osascript returns immediately after successful authentication
	const auto script = QStringLiteral("do shell script \"%1 > /dev/null 2>&1 &\" with administrator privileges").
						arg( appleScriptEscape( commandLine.join( QLatin1Char(' ') ) ) );

	return QProcess::execute( QStringLiteral("/usr/bin/osascript"), { QStringLiteral("-e"), script } ) == 0;
}



bool MacOsCoreFunctions::runProgramAsUser( const QString& program, const QStringList& parameters,
										   const QString& username, const QString& desktop,
										   const QByteArray& stdInData )
{
	Q_UNUSED(desktop)

	const auto uid = MacOsUserFunctions::userIdFromName( username );
	if( uid == MacOsUserFunctions::InvalidUserId )
	{
		vCritical() << "failed to resolve uid from username" << username;
		return false;
	}

	QString executable = program;
	QStringList arguments = parameters;

	if( geteuid() == 0 )
	{
		// launch the program inside the user's GUI (Aqua) bootstrap namespace with the user's privileges
		executable = QStringLiteral("/bin/launchctl");
		arguments = QStringList{ QStringLiteral("asuser"), QString::number( uid ),
								 QStringLiteral("/usr/bin/sudo"), QStringLiteral("-u"), username,
								 QStringLiteral("-H"), QStringLiteral("--"), program } + parameters;
	}
	else if( uid != getuid() )
	{
		vCritical() << "can't run program as different user without administrative privileges";
		return false;
	}

	auto process = new QProcess;

	if( stdInData.isEmpty() == false )
	{
		QObject::connect( process, &QProcess::started, process, [=]() {
			process->write( stdInData );
			process->closeWriteChannel();
		} );
	}

	QObject::connect( process, QOverload<int, QProcess::ExitStatus>::of( &QProcess::finished ), &QProcess::deleteLater );
	process->start( executable, arguments );

	return true;
}



QString MacOsCoreFunctions::genericUrlHandler() const
{
	return QStringLiteral( "open" );
}



QString MacOsCoreFunctions::queryDisplayDeviceName( const QScreen& screen ) const
{
	QStringList nameParts;
	nameParts.append( screen.manufacturer() );
	nameParts.append( screen.model() );
	nameParts.removeAll( {} );
	if( nameParts.isEmpty() )
	{
		return screen.name();
	}

	return QStringLiteral("%1 [%2]").arg( nameParts.join( QLatin1Char(' ') ), screen.name() );
}



QString MacOsCoreFunctions::getApplicationName( ProcessId processId ) const
{
	char path[PROC_PIDPATHINFO_MAXSIZE]{};
	if( proc_pidpath( int(processId), path, sizeof(path) ) > 0 )
	{
		const auto filePath = QString::fromUtf8( path );

		// use name of application bundle if possible
		const auto bundleIndex = filePath.indexOf( QLatin1String(".app/") );
		if( bundleIndex > 0 )
		{
			return QFileInfo( filePath.left( bundleIndex ) ).fileName();
		}

		return QFileInfo( filePath ).fileName();
	}

	char name[2*MAXCOMLEN+1]{};
	if( proc_name( int(processId), name, sizeof(name) ) > 0 )
	{
		return QString::fromUtf8( name );
	}

	return {};
}



QString MacOsCoreFunctions::shellQuote( const QString& argument )
{
	auto quotedArgument = argument;
	quotedArgument.replace( QLatin1Char('\''), QStringLiteral("'\\''") );
	return QLatin1Char('\'') + quotedArgument + QLatin1Char('\'');
}



QString MacOsCoreFunctions::appleScriptEscape( const QString& text )
{
	auto escapedText = text;
	escapedText.replace( QLatin1Char('\\'), QStringLiteral("\\\\") );
	escapedText.replace( QLatin1Char('"'), QStringLiteral("\\\"") );
	return escapedText;
}
