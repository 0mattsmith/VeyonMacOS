/*
 * MacOsSessionFunctions.cpp - implementation of MacOsSessionFunctions class
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
#include <QDateTime>
#include <QHostInfo>
#include <QProcessEnvironment>
#include <QSettings>

#include <SystemConfiguration/SystemConfiguration.h>
#include <utmpx.h>

#include "MacOsSessionFunctions.h"


MacOsSessionFunctions::SessionId MacOsSessionFunctions::currentSessionId()
{
	// only the user at the console (the active Aqua session) is supported
	return DefaultSessionId;
}



MacOsSessionFunctions::SessionUptime MacOsSessionFunctions::currentSessionUptime() const
{
	const auto user = consoleUser();
	if( user.isValid() == false )
	{
		return InvalidSessionUptime;
	}

	const auto loginTime = consoleLoginTime( user.name );
	if( loginTime < 0 )
	{
		return InvalidSessionUptime;
	}

	return SessionUptime( QDateTime::currentSecsSinceEpoch() - loginTime );
}



QString MacOsSessionFunctions::currentSessionClientAddress() const
{
	return {};
}



QString MacOsSessionFunctions::currentSessionClientName() const
{
	return {};
}



QString MacOsSessionFunctions::currentSessionHostName() const
{
	return QHostInfo::localHostName();
}



QString MacOsSessionFunctions::currentSessionType() const
{
	return consoleUser().isValid() ? QStringLiteral("aqua") : QStringLiteral("loginwindow");
}



bool MacOsSessionFunctions::currentSessionHasUser() const
{
	return consoleUser().isValid();
}



MacOsSessionFunctions::EnvironmentVariables MacOsSessionFunctions::currentSessionEnvironmentVariables() const
{
	const auto environment = QProcessEnvironment::systemEnvironment();

	EnvironmentVariables variables;
	const auto keys = environment.keys();
	for( const auto& key : keys )
	{
		variables[key] = environment.value( key );
	}

	return variables;
}



QVariant MacOsSessionFunctions::querySettingsValueInCurrentSession( const QString& key ) const
{
	return QSettings( QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName() ).value( key );
}



MacOsSessionFunctions::ConsoleUser MacOsSessionFunctions::consoleUser()
{
	ConsoleUser user;

	const auto name = SCDynamicStoreCopyConsoleUser( nullptr, &user.uid, &user.gid );
	if( name )
	{
		user.name = QString::fromCFString( name );
		CFRelease( name );
	}

	return user;
}



qint64 MacOsSessionFunctions::consoleLoginTime( const QString& userName )
{
	qint64 loginTime = -1;
	const auto userNameUtf8 = userName.toUtf8();

	setutxent();
	while( const auto entry = getutxent() )
	{
		if( entry->ut_type == USER_PROCESS &&
			qstrncmp( entry->ut_line, "console", sizeof(entry->ut_line) ) == 0 &&
			qstrncmp( entry->ut_user, userNameUtf8.constData(), sizeof(entry->ut_user) ) == 0 )
		{
			loginTime = qMax<qint64>( loginTime, entry->ut_tv.tv_sec );
		}
	}
	endutxent();

	return loginTime;
}
