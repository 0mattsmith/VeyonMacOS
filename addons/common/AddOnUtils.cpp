/*
 * AddOnUtils.cpp - common helper functions for add-ons
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

#include <QDateTime>
#include <QFile>
#include <QLocale>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>

#if defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <sys/sysctl.h>
#include <sys/time.h>
#endif

#include "AddOnUtils.h"
#include "PlatformUserFunctions.h"
#include "VeyonCore.h"


namespace AddOnUtils
{

ProcessResult runProcess( const QString& program, const QStringList& arguments, int timeout,
						  const QByteArray& standardInput, const QProcessEnvironment& environment )
{
	ProcessResult result;

	QProcess process;
	process.setProcessChannelMode( QProcess::MergedChannels );
	if( environment.isEmpty() == false )
	{
		process.setProcessEnvironment( environment );
	}

	process.start( program, arguments );
	if( process.waitForStarted( 10000 ) == false )
	{
		vWarning() << "could not start" << program << process.errorString();
		return result;
	}

	result.started = true;

	if( standardInput.isEmpty() == false )
	{
		process.write( standardInput );
	}
	process.closeWriteChannel();

	if( process.waitForFinished( timeout ) == false )
	{
		result.timedOut = true;
		process.kill();
		process.waitForFinished( 3000 );
	}

	result.output = process.readAll();
	result.exitCode = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;

	return result;
}



bool isServer()
{
	return VeyonCore::component() == VeyonCore::Component::Server;
}



bool isWorker()
{
	return VeyonCore::component() == VeyonCore::Component::Worker;
}



bool isMaster()
{
	return VeyonCore::component() == VeyonCore::Component::Master;
}



QString computerDisplayName( const ComputerControlInterface::Pointer& computerControlInterface )
{
	if( computerControlInterface.isNull() )
	{
		return {};
	}

	const auto userName = computerControlInterface->userFullName().isEmpty() ? computerControlInterface->userLoginName()
																			  : computerControlInterface->userFullName();
	if( userName.isEmpty() )
	{
		return computerControlInterface->computerName();
	}

	return QStringLiteral("%1 (%2)").arg( computerControlInterface->computerName(), userName );
}



QString computerKey( const ComputerControlInterface::Pointer& computerControlInterface )
{
	if( computerControlInterface.isNull() )
	{
		return {};
	}

	const auto uid = computerControlInterface->computer().networkObjectUid();
	return uid.isNull() ? computerControlInterface->computer().hostName() : uid.toString();
}



ComputerControlInterface::Pointer findComputer( const ComputerControlInterfaceList& interfaces, const QString& key )
{
	for( const auto& controlInterface : interfaces )
	{
		if( computerKey( controlInterface ) == key )
		{
			return controlInterface;
		}
	}

	return {};
}



ComputerControlInterfaceList connectedInterfaces( const ComputerControlInterfaceList& interfaces )
{
	ComputerControlInterfaceList result;
	for( const auto& controlInterface : interfaces )
	{
		if( controlInterface->state() == ComputerControlInterface::State::Connected )
		{
			result.append( controlInterface );
		}
	}
	return result;
}



QString formatFileSize( qint64 size )
{
	return QLocale().formattedDataSize( size, 1 );
}



bool matchesAnyPattern( const QString& name, const QStringList& patterns )
{
	for( const auto& pattern : patterns )
	{
		const auto trimmedPattern = pattern.trimmed();
		if( trimmedPattern.isEmpty() )
		{
			continue;
		}

		if( trimmedPattern.contains( QLatin1Char('*') ) || trimmedPattern.contains( QLatin1Char('?') ) )
		{
			const QRegularExpression rx( QRegularExpression::wildcardToRegularExpression( trimmedPattern ),
										 QRegularExpression::CaseInsensitiveOption );
			if( rx.match( name ).hasMatch() )
			{
				return true;
			}
		}
		else if( name.compare( trimmedPattern, Qt::CaseInsensitive ) == 0 )
		{
			return true;
		}
	}

	return false;
}



qint64 systemBootTime()
{
	const auto now = QDateTime::currentSecsSinceEpoch();

#if defined(Q_OS_WIN)
	return now - qint64( GetTickCount64() / 1000 );
#elif defined(Q_OS_MACOS)
	struct timeval bootTime{};
	size_t size = sizeof(bootTime);
	int mib[2] = { CTL_KERN, KERN_BOOTTIME };
	if( sysctl( mib, 2, &bootTime, &size, nullptr, 0 ) == 0 )
	{
		return bootTime.tv_sec;
	}
	return 0;
#else
	QFile uptimeFile( QStringLiteral("/proc/uptime") );
	if( uptimeFile.open( QFile::ReadOnly ) )
	{
		const auto uptime = uptimeFile.readAll().split( ' ' ).value( 0 ).toDouble();
		return now - qint64( uptime );
	}
	return 0;
#endif
}



QString hostFromUrlOrDomain( const QString& input )
{
	auto text = input.trimmed();
	if( text.isEmpty() )
	{
		return {};
	}

	// wildcards are not valid in URLs - all subdomains are included anyway
	if( text.startsWith( QLatin1String("*.") ) )
	{
		text = text.mid( 2 );
	}

	if( text.contains( QLatin1String("://") ) == false )
	{
		text.prepend( QStringLiteral("http://") );
	}

	return QUrl( text ).host().toLower();
}



QString sessionUserName()
{
	return VeyonCore::platform().userFunctions().queryCurrentUserProperty( PlatformUserFunctions::UserProperty::LoginName );
}



QStringList splitList( const QString& text )
{
	QStringList result;
	static const QRegularExpression separatorRX( QStringLiteral("[\\r\\n,;]+") );
	const auto parts = text.split( separatorRX );
	for( const auto& part : parts )
	{
		const auto trimmed = part.trimmed();
		if( trimmed.isEmpty() == false && result.contains( trimmed, Qt::CaseInsensitive ) == false )
		{
			result.append( trimmed );
		}
	}
	return result;
}

}
