/*
 * ProcessBackendMacOS.cpp - process management for macOS
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
#include <QVector>

#include <cerrno>
#include <csignal>
#include <libproc.h>
#include <mach/mach_time.h>
#include <pwd.h>
#include <sys/proc_info.h>
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



QString veyonBundleDirectory()
{
	// executables are located in Veyon.app/Contents/MacOS
	auto path = QCoreApplication::applicationDirPath();
	const auto bundleIndex = path.indexOf( QLatin1String(".app/") );
	if( bundleIndex > 0 )
	{
		path = path.left( bundleIndex + 4 );
	}
	return path + QLatin1Char('/');
}

}



QString ProcessBackend::platform()
{
	return QStringLiteral("macos");
}



ProcessInfoList ProcessBackend::processes( Detail detail )
{
	ProcessInfoList processList;

	const auto userId = sessionUserId();

	auto count = proc_listallpids( nullptr, 0 );
	if( count <= 0 )
	{
		return processList;
	}

	QVector<pid_t> pids( count + 64 );
	count = proc_listallpids( pids.data(), int( pids.size() * int(sizeof(pid_t)) ) );

	mach_timebase_info_data_t timebase{};
	mach_timebase_info( &timebase );

	if( detail == Detail::Full )
	{
		beginCpuSampling();
	}

	for( int i = 0; i < count; ++i )
	{
		const auto pid = pids[i];
		if( pid <= 0 )
		{
			continue;
		}

		struct proc_bsdinfo bsdInfo{};
		if( proc_pidinfo( pid, PROC_PIDTBSDINFO, 0, &bsdInfo, PROC_PIDTBSDINFO_SIZE ) != PROC_PIDTBSDINFO_SIZE )
		{
			continue;
		}

		ProcessInfo info;
		info.pid = pid;
		info.parentPid = bsdInfo.pbi_ppid;
		info.name = QString::fromUtf8( bsdInfo.pbi_name[0] ? bsdInfo.pbi_name : bsdInfo.pbi_comm );

		char path[PROC_PIDPATHINFO_MAXSIZE]{};
		if( proc_pidpath( pid, path, sizeof(path) ) > 0 )
		{
			info.path = QString::fromUtf8( path );
		}

		info.isUserProcess = bsdInfo.pbi_uid == userId;

		if( detail == Detail::Full )
		{
			struct proc_taskinfo taskInfo{};
			if( proc_pidinfo( pid, PROC_PIDTASKINFO, 0, &taskInfo, PROC_PIDTASKINFO_SIZE ) == PROC_PIDTASKINFO_SIZE )
			{
				info.memory = qint64( taskInfo.pti_resident_size );
				// CPU times are reported in Mach absolute time units
				const auto ticks = taskInfo.pti_total_user + taskInfo.pti_total_system;
				const auto nanoseconds = timebase.denom > 0 ? qint64( ticks * timebase.numer / timebase.denom ) : qint64( ticks );
				info.cpu = cpuUsage( pid, nanoseconds );
			}

			if( const auto passwordEntry = getpwuid( bsdInfo.pbi_uid ) )
			{
				info.user = QString::fromUtf8( passwordEntry->pw_name );
			}
		}

		info.isApplication = info.path.contains( QLatin1String(".app/Contents/MacOS/") ) && isSystemProcess( info ) == false;
		if( info.isApplication )
		{
			info.windowTitle = info.displayName();
		}

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
		QStringLiteral("kernel_task"), QStringLiteral("launchd"), QStringLiteral("WindowServer"),
		QStringLiteral("loginwindow"), QStringLiteral("logd"), QStringLiteral("opendirectoryd"),
		QStringLiteral("coreaudiod"), QStringLiteral("cfprefsd"), QStringLiteral("securityd"),
		QStringLiteral("trustd"), QStringLiteral("distnoted"), QStringLiteral("UserEventAgent"),
		QStringLiteral("screensharingd"), QStringLiteral("ScreenSharingAgent"), QStringLiteral("SystemUIServer"),
		QStringLiteral("Dock"), QStringLiteral("Finder"), QStringLiteral("ControlCenter"),
	};

	static const auto veyonDirectory = veyonBundleDirectory();

	return process.pid <= 1 ||
			criticalProcesses.contains( process.name ) ||
			process.name.startsWith( QLatin1String("veyon-") ) ||
			process.path.startsWith( veyonDirectory );
}



bool ProcessBackend::isSystemProcess( const ProcessInfo& process ) const
{
	static const QStringList systemDirectories{
		QStringLiteral("/System/"), QStringLiteral("/usr/"), QStringLiteral("/bin/"), QStringLiteral("/sbin/"),
		QStringLiteral("/Library/Apple/"), QStringLiteral("/private/"), QStringLiteral("/Library/PrivilegedHelperTools/"),
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
