/*
 * ProcessBackendLinux.cpp - process management for Linux
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
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cerrno>
#include <csignal>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include "PlatformUserFunctions.h"
#include "ProcessBackend.h"


namespace {

uid_t sessionUserId()
{
	const auto userName = VeyonCore::platform().userFunctions().queryCurrentUserProperty( PlatformUserFunctions::UserProperty::LoginName );
	if( userName.isEmpty() == false )
	{
		if( const auto passwordEntry = getpwnam( userName.toUtf8().constData() ) )
		{
			return passwordEntry->pw_uid;
		}
	}

	return uid_t(-1);
}

}



QString ProcessBackend::platform()
{
	return QStringLiteral("linux");
}



ProcessInfoList ProcessBackend::processes( Detail detail )
{
	ProcessInfoList processList;

	const auto userId = sessionUserId();
	static const auto ticksPerSecond = qMax<long>( 1, sysconf( _SC_CLK_TCK ) );
	static const auto pageSize = qMax<long>( 1, sysconf( _SC_PAGESIZE ) );

	if( detail == Detail::Full )
	{
		beginCpuSampling();
	}

	const auto entries = QDir( QStringLiteral("/proc") ).entryList( QDir::Dirs | QDir::NoDotAndDotDot );
	for( const auto& entry : entries )
	{
		bool ok = false;
		const auto pid = entry.toLongLong( &ok );
		if( ok == false || pid <= 0 )
		{
			continue;
		}

		const auto procPath = QStringLiteral("/proc/%1").arg( pid );

		QFile statFile( procPath + QStringLiteral("/stat") );
		if( statFile.open( QFile::ReadOnly ) == false )
		{
			continue;
		}
		const auto statContent = QString::fromUtf8( statFile.readAll() );

		// format: pid (comm) state ppid ... - comm may contain spaces and parentheses
		const auto commStart = statContent.indexOf( QLatin1Char('(') );
		const auto commEnd = statContent.lastIndexOf( QLatin1Char(')') );
		if( commStart < 0 || commEnd < commStart )
		{
			continue;
		}

		const auto fields = statContent.mid( commEnd + 2 ).split( QLatin1Char(' ') );

		ProcessInfo info;
		info.pid = pid;
		info.name = statContent.mid( commStart + 1, commEnd - commStart - 1 );
		info.parentPid = fields.value( 1 ).toLongLong();
		info.path = QFileInfo( procPath + QStringLiteral("/exe") ).symLinkTarget();
		if( info.path.isEmpty() == false )
		{
			info.name = QFileInfo( info.path ).fileName();
		}

		struct stat procStat{};
		const auto haveOwner = ::stat( procPath.toUtf8().constData(), &procStat ) == 0;
		info.isUserProcess = haveOwner && procStat.st_uid == userId;

		if( detail == Detail::Full )
		{
			const auto ticks = fields.value( 11 ).toLongLong() + fields.value( 12 ).toLongLong();
			info.cpu = cpuUsage( pid, ticks * 1000000000LL / ticksPerSecond );
			info.memory = fields.value( 21 ).toLongLong() * pageSize;
			if( haveOwner )
			{
				if( const auto passwordEntry = getpwuid( procStat.st_uid ) )
				{
					info.user = QString::fromUtf8( passwordEntry->pw_name );
				}
			}
		}

		info.isApplication = info.isUserProcess && isSystemProcess( info ) == false;

		processList.append( info );
	}

	if( detail == Detail::Full )
	{
		endCpuSampling();
	}

	return processList;
}



bool ProcessBackend::terminate( qint64 pid, bool force )
{
	return ::kill( pid_t(pid), force ? SIGKILL : SIGTERM ) == 0;
}



bool ProcessBackend::isRunning( qint64 pid ) const
{
	return ::kill( pid_t(pid), 0 ) == 0 || errno == EPERM;
}



bool ProcessBackend::isCriticalProcess( const ProcessInfo& process ) const
{
	static const QStringList criticalProcesses{
		QStringLiteral("systemd"), QStringLiteral("init"), QStringLiteral("Xorg"), QStringLiteral("Xwayland"),
		QStringLiteral("gnome-shell"), QStringLiteral("kwin_x11"), QStringLiteral("kwin_wayland"),
		QStringLiteral("plasmashell"), QStringLiteral("dbus-daemon"), QStringLiteral("dbus-broker"),
		QStringLiteral("pipewire"), QStringLiteral("wireplumber"), QStringLiteral("pulseaudio"),
		QStringLiteral("gdm"), QStringLiteral("sddm"), QStringLiteral("lightdm"), QStringLiteral("polkitd"),
		QStringLiteral("x11vnc"),
	};

	return process.pid <= 1 ||
			criticalProcesses.contains( process.name ) ||
			process.name.startsWith( QLatin1String("veyon-") ) ||
			process.path.startsWith( QCoreApplication::applicationDirPath() + QLatin1Char('/') );
}



bool ProcessBackend::isSystemProcess( const ProcessInfo& process ) const
{
	static const QStringList systemDirectories{
		QStringLiteral("/usr/lib/"), QStringLiteral("/usr/libexec/"), QStringLiteral("/lib/"),
		QStringLiteral("/sbin/"), QStringLiteral("/usr/sbin/"),
	};

	if( isCriticalProcess( process ) || process.path.isEmpty() )
	{
		return true;
	}

	for( const auto& directory : systemDirectories )
	{
		if( process.path.startsWith( directory ) )
		{
			return true;
		}
	}

	return false;
}
