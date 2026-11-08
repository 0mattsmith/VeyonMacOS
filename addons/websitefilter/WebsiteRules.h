/*
 * WebsiteRules.h - rules for allowing/blocking websites
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
#include <QVariantMap>

#include "AddOnUtils.h"

struct WebsiteRules
{
	enum class Mode
	{
		Disabled,
		Blacklist,	// block listed websites
		Whitelist,	// allow listed websites only
	};

	Mode mode{Mode::Disabled};
	QStringList domains;
	bool useHostsFile{false};
	bool closeBrowsers{false};
	bool keepAfterRestart{false};

	bool isActive() const
	{
		return mode == Mode::Whitelist || ( mode == Mode::Blacklist && domains.isEmpty() == false );
	}

	//! \brief Converts user input (URLs, "www." prefixes, wildcards) into plain domain names
	static QStringList normalizedDomains( const QStringList& input )
	{
		QStringList domains;
		for( const auto& entry : input )
		{
			auto domain = AddOnUtils::hostFromUrlOrDomain( entry );
			if( domain.startsWith( QLatin1String("www.") ) )
			{
				domain = domain.mid( 4 );
			}
			if( domain.isEmpty() == false && domains.contains( domain ) == false )
			{
				domains.append( domain );
			}
		}
		return domains;
	}

	QVariantMap toVariantMap() const
	{
		return {
			{ QStringLiteral("mode"), int(mode) },
			{ QStringLiteral("domains"), domains },
			{ QStringLiteral("useHostsFile"), useHostsFile },
			{ QStringLiteral("closeBrowsers"), closeBrowsers },
			{ QStringLiteral("keepAfterRestart"), keepAfterRestart },
		};
	}

	static WebsiteRules fromVariantMap( const QVariantMap& map )
	{
		WebsiteRules rules;
		rules.mode = Mode( qBound( 0, map.value( QStringLiteral("mode") ).toInt(), int(Mode::Whitelist) ) );
		rules.domains = normalizedDomains( map.value( QStringLiteral("domains") ).toStringList() );
		rules.useHostsFile = map.value( QStringLiteral("useHostsFile") ).toBool();
		rules.closeBrowsers = map.value( QStringLiteral("closeBrowsers") ).toBool();
		rules.keepAfterRestart = map.value( QStringLiteral("keepAfterRestart") ).toBool();
		return rules;
	}

	bool operator==( const WebsiteRules& other ) const
	{
		return mode == other.mode && domains == other.domains && useHostsFile == other.useHostsFile &&
				closeBrowsers == other.closeBrowsers && keepAfterRestart == other.keepAfterRestart;
	}
};
