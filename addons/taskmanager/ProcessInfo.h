/*
 * ProcessInfo.h - information about a running process
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

#include <QFileInfo>
#include <QVariantMap>

struct ProcessInfo
{
	qint64 pid{0};
	qint64 parentPid{0};
	QString name;
	QString path;
	QString user;
	QString windowTitle;
	qint64 memory{0};
	double cpu{0};
	bool isUserProcess{false};
	bool isApplication{false};

	//! \brief Name shown to users - e.g. "Google Chrome" instead of "Google Chrome Helper" path components
	QString displayName() const
	{
		const auto bundleIndex = path.indexOf( QLatin1String(".app/") );
		if( bundleIndex > 0 )
		{
			return QFileInfo( path.left( bundleIndex ) ).fileName();
		}
		return name;
	}

	QVariantMap toVariantMap() const
	{
		return {
			{ QStringLiteral("pid"), pid },
			{ QStringLiteral("ppid"), parentPid },
			{ QStringLiteral("name"), name },
			{ QStringLiteral("path"), path },
			{ QStringLiteral("user"), user },
			{ QStringLiteral("title"), windowTitle },
			{ QStringLiteral("memory"), memory },
			{ QStringLiteral("cpu"), cpu },
			{ QStringLiteral("userProcess"), isUserProcess },
			{ QStringLiteral("app"), isApplication },
		};
	}

	static ProcessInfo fromVariantMap( const QVariantMap& map )
	{
		ProcessInfo info;
		info.pid = map.value( QStringLiteral("pid") ).toLongLong();
		info.parentPid = map.value( QStringLiteral("ppid") ).toLongLong();
		info.name = map.value( QStringLiteral("name") ).toString();
		info.path = map.value( QStringLiteral("path") ).toString();
		info.user = map.value( QStringLiteral("user") ).toString();
		info.windowTitle = map.value( QStringLiteral("title") ).toString();
		info.memory = map.value( QStringLiteral("memory") ).toLongLong();
		info.cpu = map.value( QStringLiteral("cpu") ).toDouble();
		info.isUserProcess = map.value( QStringLiteral("userProcess") ).toBool();
		info.isApplication = map.value( QStringLiteral("app") ).toBool();
		return info;
	}
};

using ProcessInfoList = QList<ProcessInfo>;
