/*
 * ApplicationRules.h - rules for allowing/blocking applications on student computers
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

#include <QJsonArray>
#include <QJsonObject>

#include "AddOnUtils.h"
#include "ProcessInfo.h"

struct ApplicationRules
{
	enum class Mode
	{
		Disabled,
		Blacklist,	// block listed applications
		Whitelist,	// allow listed applications only
	};

	Mode mode{Mode::Disabled};
	QStringList patterns;
	QString message;
	bool persistent{false};

	bool isActive() const
	{
		return mode == Mode::Whitelist || ( mode == Mode::Blacklist && patterns.isEmpty() == false );
	}

	//! \brief Checks whether any pattern matches the name (with and without extension), the bundle name or the path
	bool matches( const ProcessInfo& process ) const
	{
		const auto name = process.name;
		const auto nameWithoutExtension = name.endsWith( QLatin1String(".exe"), Qt::CaseInsensitive ) ? name.chopped( 4 ) : name;

		return AddOnUtils::matchesAnyPattern( name, patterns ) ||
				AddOnUtils::matchesAnyPattern( nameWithoutExtension, patterns ) ||
				AddOnUtils::matchesAnyPattern( process.displayName(), patterns ) ||
				( process.path.isEmpty() == false && AddOnUtils::matchesAnyPattern( process.path, patterns ) );
	}

	bool isAllowed( const ProcessInfo& process ) const
	{
		switch( mode )
		{
		case Mode::Blacklist: return matches( process ) == false;
		case Mode::Whitelist: return matches( process );
		case Mode::Disabled: break;
		}
		return true;
	}

	QVariantMap toVariantMap() const
	{
		return {
			{ QStringLiteral("mode"), int(mode) },
			{ QStringLiteral("patterns"), patterns },
			{ QStringLiteral("message"), message },
			{ QStringLiteral("persistent"), persistent },
		};
	}

	static ApplicationRules fromVariantMap( const QVariantMap& map )
	{
		ApplicationRules rules;
		rules.mode = Mode( qBound( 0, map.value( QStringLiteral("mode") ).toInt(), int(Mode::Whitelist) ) );
		rules.patterns = map.value( QStringLiteral("patterns") ).toStringList();
		rules.message = map.value( QStringLiteral("message") ).toString();
		rules.persistent = map.value( QStringLiteral("persistent") ).toBool();
		return rules;
	}

	QJsonObject toJson() const
	{
		return QJsonObject::fromVariantMap( toVariantMap() );
	}

	static ApplicationRules fromJson( const QJsonObject& json )
	{
		return fromVariantMap( json.toVariantMap() );
	}

	bool operator==( const ApplicationRules& other ) const
	{
		return mode == other.mode && patterns == other.patterns && message == other.message && persistent == other.persistent;
	}
};
