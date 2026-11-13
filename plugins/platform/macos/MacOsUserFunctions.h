/*
 * MacOsUserFunctions.h - declaration of MacOsUserFunctions class
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

#include "PlatformUserFunctions.h"

// clazy:excludeall=copyable-polymorphic

class MacOsUserFunctions : public PlatformUserFunctions
{
public:
	using UserId = uid_t;

	static constexpr UserId InvalidUserId = UserId(-1);

	QString queryCurrentUserProperty( UserProperty property ) override;

	QStringList userGroups( bool queryDomainGroups ) override;
	QStringList groupsOfUser( const QString& username, bool queryDomainGroups ) override;
	QString userGroupSecurityIdentifier( const QString& groupName ) override;

	bool isAnyUserLoggedOn() override;

	// logging on users remotely is not implemented on macOS
	bool isLogonSupported() const override
	{
		return false;
	}

	bool prepareLogon( const QString& username, const Password& password ) override;
	bool performLogon( const QString& username, const Password& password ) override;
	void logoff() override;

	bool authenticate( const QString& username, const Password& password ) override;

	static UserId userIdFromName( const QString& username );

private:
	QString currentUserLoginName() const;
	static QString fullNameOfUser( const QString& username );

	static constexpr auto LogoffTimeout = 15000;

};
