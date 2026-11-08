/*
 * MacOsServerProcess.cpp - implementation of MacOsServerProcess class
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

#include <csignal>

#include "Filesystem.h"
#include "MacOsServerProcess.h"
#include "VeyonConfiguration.h"


MacOsServerProcess::MacOsServerProcess( const MacOsSessionFunctions::ConsoleUser& user,
										const QProcessEnvironment& processEnvironment,
										QObject* parent ) :
	QProcess( parent ),
	m_user( user )
{
	setProcessEnvironment( processEnvironment );
}



MacOsServerProcess::~MacOsServerProcess()
{
	stop();
}



void MacOsServerProcess::start()
{
	if( VeyonCore::config().logToSystem() )
	{
		setProcessChannelMode( QProcess::ForwardedChannels );
	}

	const auto serverFilePath = VeyonCore::filesystem().serverFilePath();

	if( m_user.isValid() )
	{
		// run with root privileges but inside the user's Aqua session so the server can show
		// windows (screen lock, messages) and access the window server
		QProcess::start( QStringLiteral("/bin/launchctl"),
						 { QStringLiteral("asuser"), QString::number( m_user.uid ), serverFilePath } );
	}
	else
	{
		// login window - server is still useful for e.g. power control and monitoring
		QProcess::start( serverFilePath, QStringList{} );
	}
}



void MacOsServerProcess::stop()
{
	if( state() == QProcess::NotRunning )
	{
		return;
	}

	signalProcessTree( SIGTERM );

	if( waitForFinished( ServerTerminateTimeout ) == false )
	{
		vWarning() << "server for user" << m_user.name << "still running - killing now";
		signalProcessTree( SIGKILL );
		waitForFinished( ServerKillTimeout );
	}
}



void MacOsServerProcess::signalProcessTree( int signal )
{
	const auto pid = pid_t( processId() );
	if( pid <= 0 )
	{
		return;
	}

	// launchctl may spawn the server as child process depending on the macOS version
	QProcess::execute( QStringLiteral("/usr/bin/pkill"), { QStringLiteral("-%1").arg( signal ),
														   QStringLiteral("-P"), QString::number( pid ) } );
	::kill( pid, signal );
}
