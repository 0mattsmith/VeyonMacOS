/*
 * MacOsSessionFunctions.h - declaration of MacOsSessionFunctions class
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

#pragma once

#include <sys/types.h>

#include "PlatformSessionFunctions.h"

// clazy:excludeall=copyable-polymorphic

class MacOsSessionFunctions : public PlatformSessionFunctions
{
public:
	struct ConsoleUser
	{
		QString name;
		uid_t uid{0};
		gid_t gid{0};

		bool isValid() const
		{
			// at the login window the console is owned by root ("loginwindow")
			return name.isEmpty() == false && name != QLatin1String("loginwindow") && uid != 0;
		}

		bool operator==( const ConsoleUser& other ) const
		{
			return name == other.name && uid == other.uid;
		}

		bool operator!=( const ConsoleUser& other ) const
		{
			return !( *this == other );
		}
	};

	SessionId currentSessionId() override;

	SessionUptime currentSessionUptime() const override;
	QString currentSessionClientAddress() const override;
	QString currentSessionClientName() const override;
	QString currentSessionHostName() const override;

	QString currentSessionType() const override;
	bool currentSessionHasUser() const override;

	EnvironmentVariables currentSessionEnvironmentVariables() const override;
	QVariant querySettingsValueInCurrentSession( const QString& key ) const override;

	//! \brief Returns the user owning the console (i.e. the user logged in at the screen)
	static ConsoleUser consoleUser();

	//! \brief Returns the login time of the given user at the console in seconds since epoch (-1 if unknown)
	static qint64 consoleLoginTime( const QString& userName );

};
