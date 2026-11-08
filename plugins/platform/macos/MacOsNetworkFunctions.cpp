/*
 * MacOsNetworkFunctions.cpp - implementation of MacOsNetworkFunctions class
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

#include <QProcess>
#include <QRegularExpression>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include "MacOsNetworkFunctions.h"
#include "ProcessHelper.h"


MacOsNetworkFunctions::PingResult MacOsNetworkFunctions::ping( const QString& hostAddress )
{
	QProcess pingProcess;
	pingProcess.start( QStringLiteral("/sbin/ping"),
					   { QStringLiteral("-c"), QStringLiteral("1"),
						 QStringLiteral("-t"), QString::number( qMax( 1, PingTimeout / 1000 ) ),
						 hostAddress } );
	if( pingProcess.waitForFinished( PingProcessTimeout ) )
	{
		switch( pingProcess.exitCode() )
		{
		case 0: return PingResult::ReplyReceived;
		case 2: return PingResult::TimedOut;
		case 68: return PingResult::NameResolutionFailed; // EX_NOHOST
		default:
			break;
		}
	}

	return PingResult::Unknown;
}



bool MacOsNetworkFunctions::configureFirewallException( const QString& applicationPath, const QString& description, bool enabled )
{
	Q_UNUSED(description)

	// the application firewall can only be configured with root privileges
	if( geteuid() != 0 )
	{
		return true;
	}

	const auto socketFilterFw = QStringLiteral("/usr/libexec/ApplicationFirewall/socketfilterfw");

	if( enabled )
	{
		QProcess::execute( socketFilterFw, { QStringLiteral("--add"), applicationPath } );
		QProcess::execute( socketFilterFw, { QStringLiteral("--unblockapp"), applicationPath } );
	}
	else
	{
		QProcess::execute( socketFilterFw, { QStringLiteral("--remove"), applicationPath } );
	}

	return true;
}



bool MacOsNetworkFunctions::configureSocketKeepalive( Socket socket, bool enabled, int idleTime, int interval, int probes )
{
	const auto fd = static_cast<int>( socket );
	int optval = enabled ? 1 : 0;
	const socklen_t optlen = sizeof(optval);

	if( setsockopt( fd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen ) < 0 )
	{
		vWarning() << "could not set SO_KEEPALIVE";
		return false;
	}

	// TCP_KEEPALIVE is the macOS equivalent of TCP_KEEPIDLE
	optval = std::max<int>( 1, idleTime / 1000 );
	if( setsockopt( fd, IPPROTO_TCP, TCP_KEEPALIVE, &optval, optlen ) < 0 )
	{
		vWarning() << "could not set TCP_KEEPALIVE";
		return false;
	}

	optval = std::max<int>( 1, interval / 1000 );
	if( setsockopt( fd, IPPROTO_TCP, TCP_KEEPINTVL, &optval, optlen ) < 0 )
	{
		vWarning() << "could not set TCP_KEEPINTVL";
		return false;
	}

	optval = probes;
	if( setsockopt( fd, IPPROTO_TCP, TCP_KEEPCNT, &optval, optlen ) < 0 )
	{
		vWarning() << "could not set TCP_KEEPCNT";
		return false;
	}

	return true;
}



QNetworkInterface MacOsNetworkFunctions::defaultRouteNetworkInterface()
{
	const auto routeInfo = QString::fromUtf8( ProcessHelper( QStringLiteral("/sbin/route"),
															{ QStringLiteral("-n"), QStringLiteral("get"), QStringLiteral("default") } ).runAndReadAll() );

	static const QRegularExpression interfaceRX( QStringLiteral("interface:\\s*(\\S+)") );
	const auto match = interfaceRX.match( routeInfo );
	if( match.hasMatch() )
	{
		return QNetworkInterface::interfaceFromName( match.captured( 1 ) );
	}

	return {};
}



int MacOsNetworkFunctions::networkInterfaceSpeedInMBitPerSecond( const QNetworkInterface& networkInterface )
{
	if( networkInterface.isValid() == false )
	{
		return 0;
	}

	// e.g. "media: autoselect (1000baseT <full-duplex>)" or "media: autoselect (10GbaseT <full-duplex>)"
	const auto interfaceInfo = QString::fromUtf8( ProcessHelper( QStringLiteral("/sbin/ifconfig"),
																{ networkInterface.name() } ).runAndReadAll() );

	static const QRegularExpression mediaRX( QStringLiteral("media:.*\\((\\d+)(G?)base") );
	const auto match = mediaRX.match( interfaceInfo );
	if( match.hasMatch() )
	{
		const auto speed = match.captured( 1 ).toInt();
		return match.captured( 2 ).isEmpty() ? speed : speed * 1000;
	}

	return 0;
}
