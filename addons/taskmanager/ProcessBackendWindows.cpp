/*
 * ProcessBackendWindows.cpp - process management for Windows
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

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

#include "ProcessBackend.h"


namespace {

QString normalizedPath( const QString& path )
{
	return QDir::toNativeSeparators( path ).toLower();
}



QString windowsDirectory()
{
	wchar_t buffer[MAX_PATH]{};
	const auto length = GetWindowsDirectoryW( buffer, MAX_PATH );
	return normalizedPath( QString::fromWCharArray( buffer, int(length) ) ) + QLatin1Char('\\');
}



QString veyonDirectory()
{
	return normalizedPath( QCoreApplication::applicationDirPath() ) + QLatin1Char('\\');
}



BOOL CALLBACK collectWindowTitles( HWND window, LPARAM parameter )
{
	auto titles = reinterpret_cast<QHash<qint64, QString> *>( parameter );

	if( IsWindowVisible( window ) == false ||
		GetWindow( window, GW_OWNER ) != nullptr ||
		( GetWindowLongW( window, GWL_EXSTYLE ) & WS_EX_TOOLWINDOW ) )
	{
		return TRUE;
	}

	wchar_t title[512]{};
	const auto length = GetWindowTextW( window, title, 512 );
	if( length <= 0 )
	{
		return TRUE;
	}

	DWORD pid = 0;
	GetWindowThreadProcessId( window, &pid );
	if( titles->contains( pid ) == false )
	{
		titles->insert( pid, QString::fromWCharArray( title, length ) );
	}

	return TRUE;
}



BOOL CALLBACK closeWindowsOfProcess( HWND window, LPARAM parameter )
{
	DWORD pid = 0;
	GetWindowThreadProcessId( window, &pid );
	if( pid == DWORD(parameter) && IsWindowVisible( window ) && GetWindow( window, GW_OWNER ) == nullptr )
	{
		PostMessageW( window, WM_CLOSE, 0, 0 );
	}

	return TRUE;
}



QString processUserName( HANDLE process )
{
	HANDLE token = nullptr;
	if( OpenProcessToken( process, TOKEN_QUERY, &token ) == false )
	{
		return {};
	}

	QString userName;

	DWORD size = 0;
	GetTokenInformation( token, TokenUser, nullptr, 0, &size );
	if( size > 0 )
	{
		QByteArray buffer( int(size), 0 );
		if( GetTokenInformation( token, TokenUser, buffer.data(), size, &size ) )
		{
			const auto tokenUser = reinterpret_cast<TOKEN_USER *>( buffer.data() );
			wchar_t name[256]{};
			wchar_t domain[256]{};
			DWORD nameLength = 256;
			DWORD domainLength = 256;
			SID_NAME_USE use;
			if( LookupAccountSidW( nullptr, tokenUser->User.Sid, name, &nameLength, domain, &domainLength, &use ) )
			{
				userName = QString::fromWCharArray( name, int(nameLength) );
			}
		}
	}

	CloseHandle( token );

	return userName;
}



qint64 fileTimeToNanoseconds( const FILETIME& fileTime )
{
	ULARGE_INTEGER value;
	value.LowPart = fileTime.dwLowDateTime;
	value.HighPart = fileTime.dwHighDateTime;
	return qint64( value.QuadPart ) * 100;
}

}



QString ProcessBackend::platform()
{
	return QStringLiteral("windows");
}



ProcessInfoList ProcessBackend::processes( Detail detail )
{
	DWORD ownSessionId = 0;
	ProcessIdToSessionId( GetCurrentProcessId(), &ownSessionId );

	QHash<qint64, QString> windowTitles;
	if( detail == Detail::Full )
	{
		EnumWindows( collectWindowTitles, LPARAM(&windowTitles) );
		beginCpuSampling();
	}

	ProcessInfoList processList;

	const auto snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	if( snapshot == INVALID_HANDLE_VALUE )
	{
		return processList;
	}

	PROCESSENTRY32W entry{};
	entry.dwSize = sizeof(entry);

	for( auto ok = Process32FirstW( snapshot, &entry ); ok; ok = Process32NextW( snapshot, &entry ) )
	{
		ProcessInfo info;
		info.pid = entry.th32ProcessID;
		info.parentPid = entry.th32ParentProcessID;
		info.name = QString::fromWCharArray( entry.szExeFile );

		DWORD sessionId = 0;
		ProcessIdToSessionId( entry.th32ProcessID, &sessionId );

		const auto process = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID );
		if( process )
		{
			wchar_t path[MAX_PATH*2]{};
			DWORD pathLength = MAX_PATH*2;
			if( QueryFullProcessImageNameW( process, 0, path, &pathLength ) )
			{
				info.path = QString::fromWCharArray( path, int(pathLength) );
			}

			if( detail == Detail::Full )
			{
				PROCESS_MEMORY_COUNTERS counters{};
				if( GetProcessMemoryInfo( process, &counters, sizeof(counters) ) )
				{
					info.memory = qint64(counters.WorkingSetSize);
				}

				FILETIME creationTime, exitTime, kernelTime, userTime;
				if( GetProcessTimes( process, &creationTime, &exitTime, &kernelTime, &userTime ) )
				{
					info.cpu = cpuUsage( info.pid, fileTimeToNanoseconds( kernelTime ) + fileTimeToNanoseconds( userTime ) );
				}

				info.user = processUserName( process );
			}

			CloseHandle( process );
		}

		info.windowTitle = windowTitles.value( info.pid );
		info.isApplication = info.windowTitle.isEmpty() == false;
		info.isUserProcess = sessionId != 0 && sessionId == ownSessionId &&
							 info.user.compare( QLatin1String("SYSTEM"), Qt::CaseInsensitive ) != 0;

		processList.append( info );
	}

	CloseHandle( snapshot );

	if( detail == Detail::Full )
	{
		endCpuSampling();
	}

	return processList;
}



bool ProcessBackend::terminate( qint64 pid, bool force )
{
	if( force == false )
	{
		// ask the application to close its windows (allows saving documents)
		EnumWindows( closeWindowsOfProcess, LPARAM(pid) );
		return true;
	}

	const auto process = OpenProcess( PROCESS_TERMINATE, FALSE, DWORD(pid) );
	if( process == nullptr )
	{
		return false;
	}

	const auto result = TerminateProcess( process, 1 );
	CloseHandle( process );

	return result;
}



bool ProcessBackend::isRunning( qint64 pid ) const
{
	const auto process = OpenProcess( SYNCHRONIZE, FALSE, DWORD(pid) );
	if( process == nullptr )
	{
		return false;
	}

	const auto running = WaitForSingleObject( process, 0 ) == WAIT_TIMEOUT;
	CloseHandle( process );

	return running;
}



bool ProcessBackend::isCriticalProcess( const ProcessInfo& process ) const
{
	static const QStringList criticalProcesses{
		QStringLiteral("system"), QStringLiteral("system idle process"), QStringLiteral("registry"),
		QStringLiteral("secure system"), QStringLiteral("memory compression"), QStringLiteral("smss.exe"),
		QStringLiteral("csrss.exe"), QStringLiteral("wininit.exe"), QStringLiteral("winlogon.exe"),
		QStringLiteral("services.exe"), QStringLiteral("lsass.exe"), QStringLiteral("lsaiso.exe"),
		QStringLiteral("svchost.exe"), QStringLiteral("dwm.exe"), QStringLiteral("fontdrvhost.exe"),
		QStringLiteral("msmpeng.exe"), QStringLiteral("logonui.exe"), QStringLiteral("userinit.exe"),
	};

	const auto name = process.name.toLower();

	return process.pid <= 4 ||
			criticalProcesses.contains( name ) ||
			name.startsWith( QLatin1String("veyon-") ) ||
			( process.path.isEmpty() == false && normalizedPath( process.path ).startsWith( veyonDirectory() ) );
}



bool ProcessBackend::isSystemProcess( const ProcessInfo& process ) const
{
	// helper processes required by many applications and shell components outside the Windows directory
	static const QStringList systemProcesses{
		QStringLiteral("msedgewebview2.exe"),
		QStringLiteral("explorer.exe"),
		QStringLiteral("sihost.exe"),
		QStringLiteral("ctfmon.exe"),
		QStringLiteral("conhost.exe"),
		QStringLiteral("runtimebroker.exe"),
		QStringLiteral("securityhealthsystray.exe"),
	};

	static const auto windowsDir = windowsDirectory();

	return isCriticalProcess( process ) ||
			process.path.isEmpty() ||
			normalizedPath( process.path ).startsWith( windowsDir ) ||
			systemProcesses.contains( process.name.toLower() );
}
