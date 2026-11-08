/*
 * WebsiteFilterBackend.h - applies website rules via browser policies and the hosts file
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

#include <QJsonObject>

#include "WebsiteRules.h"

/*!
 * \brief Applies website rules using the enterprise policies of web browsers (the mechanism also used by
 * Group Policy and MDM solutions) and optionally the hosts file. Policy values configured before are backed
 * up and restored when the rules are removed. Implemented per platform in WebsiteFilterBackendWindows.cpp,
 * WebsiteFilterBackendMacOS.cpp and WebsiteFilterBackendLinux.cpp.
 */
class WebsiteFilterBackend
{
public:
	bool apply( const WebsiteRules& rules, QString* errorString );
	void clear();

	//! \brief Backup of previous policy values - persisted so it can be restored after restarts
	QJsonObject state() const
	{
		return m_state;
	}

	void setState( const QJsonObject& state )
	{
		m_state = state;
	}

	//! \brief Browser policy patterns for Firefox (match patterns) for the given domains
	static QStringList firefoxPatterns( const QStringList& domains );

	//! \brief Updates the section managed by Veyon in a hosts file (removes it if domains is empty)
	static bool updateHostsFile( const QString& hostsFilePath, const QStringList& blockedDomains );

	//! \brief Executable names of common web browsers (used for closing them on request)
	static QStringList browserProcessNames();

	//! \brief Asks running web browsers to quit - has to be called in the session of the user
	static void closeBrowsers();

private:
	QJsonObject m_state;

};
