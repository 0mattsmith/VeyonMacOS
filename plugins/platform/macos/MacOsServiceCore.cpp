/*
 * MacOsServiceCore.cpp - implementation of MacOsServiceCore class
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

#include <QEventLoop>

#include <pwd.h>

#include "MacOsPlatformConfiguration.h"
#include "MacOsServerProcess.h"
#include "MacOsServiceCore.h"
#include "VeyonConfiguration.h"


MacOsServiceCore::MacOsServiceCore( QObject* parent ) :
	QObject( parent )
{
	connect( &m_consoleUserPollTimer, &QTimer::timeout, this, &MacOsServiceCore::checkConsoleUser );
}



MacOsServiceCore::~MacOsServiceCore()
{
	stopServer();
}



void MacOsServiceCore::run()
{
	m_consoleUser = MacOsSessionFunctions::consoleUser();
	m_consoleUserChangedTimer.start();

	// don't wait for a fresh session on startup if a user is already logged in
	startServer();

	m_consoleUserPollTimer.start( ConsoleUserPollingInterval );

	QEventLoop eventLoop;
	eventLoop.exec();
}



void MacOsServiceCore::checkConsoleUser()
{
	const auto consoleUser = MacOsSessionFunctions::consoleUser();

	if( consoleUser != m_consoleUser )
	{
		vInfo() << "console user changed from" << m_consoleUser.name << "to" << consoleUser.name;

		stopServer();
		m_consoleUser = consoleUser;
		m_consoleUserChangedTimer.restart();
	}

	const auto serverRunning = m_serverProcess && m_serverProcess->state() != QProcess::NotRunning;
	if( serverRunning )
	{
		return;
	}

	// give the session some time to initialize (Dock, Finder, window server connection)
	const auto minimumSessionLifetime = MacOsPlatformConfiguration( &VeyonCore::config() ).minimumUserSessionLifetime() * 1000;
	if( m_consoleUser.isValid() && m_consoleUserChangedTimer.elapsed() < minimumSessionLifetime )
	{
		return;
	}

	// restart crashed servers with a delay
	if( m_lastServerStartTimer.isValid() && m_lastServerStartTimer.elapsed() < ServerRestartInterval )
	{
		return;
	}

	startServer();
}



void MacOsServiceCore::startServer()
{
	stopServer();

	const auto sessionId = m_sessionManager.openSession( QStringLiteral("console") );

	vInfo() << "Starting server for console user" << ( m_consoleUser.isValid() ? m_consoleUser.name : QStringLiteral("(login window)") )
			<< "with session ID" << sessionId;

	m_serverProcess = new MacOsServerProcess( m_consoleUser, serverEnvironment(), this );
	m_serverProcess->start();

	m_lastServerStartTimer.restart();
}



void MacOsServiceCore::stopServer()
{
	if( m_serverProcess == nullptr )
	{
		return;
	}

	vInfo() << "Stopping server for console user" << m_consoleUser.name;

	m_serverProcess->stop();
	delete m_serverProcess;
	m_serverProcess = nullptr;

	m_sessionManager.closeSession( QStringLiteral("console") );
}



QProcessEnvironment MacOsServiceCore::serverEnvironment() const
{
	QProcessEnvironment environment;
	environment.insert( QStringLiteral("PATH"), QStringLiteral("/usr/bin:/bin:/usr/sbin:/sbin") );
	environment.insert( QLatin1String(ServiceDataManager::serviceDataTokenEnvironmentVariable()),
						QString::fromUtf8( m_dataManager.token().toByteArray() ) );

	if( m_consoleUser.isValid() )
	{
		if( const auto passwordEntry = getpwuid( m_consoleUser.uid ) )
		{
			environment.insert( QStringLiteral("HOME"), QString::fromUtf8( passwordEntry->pw_dir ) );
			environment.insert( QStringLiteral("SHELL"), QString::fromUtf8( passwordEntry->pw_shell ) );
		}
		environment.insert( QStringLiteral("USER"), m_consoleUser.name );
		environment.insert( QStringLiteral("LOGNAME"), m_consoleUser.name );
	}
	else
	{
		environment.insert( QStringLiteral("HOME"), QStringLiteral("/var/root") );
	}

	return environment;
}
