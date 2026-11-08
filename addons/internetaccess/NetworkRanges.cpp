/*
 * NetworkRanges.cpp - helpers for working with IP address ranges
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

#include <QFile>
#include <QHostAddress>
#include <QHostInfo>
#include <QRegularExpression>

#if defined(Q_OS_WIN)
#include <winsock2.h>
#include <iphlpapi.h>
#endif

#include "AddOnUtils.h"
#include "NetworkRanges.h"


bool Ipv4RangeSet::add( const QString& specification )
{
	const auto text = specification.trimmed();

	const auto dashIndex = text.indexOf( QLatin1Char('-') );
	if( dashIndex > 0 )
	{
		const QHostAddress first( text.left( dashIndex ).trimmed() );
		const QHostAddress last( text.mid( dashIndex + 1 ).trimmed() );
		if( first.protocol() != QAbstractSocket::IPv4Protocol || last.protocol() != QAbstractSocket::IPv4Protocol )
		{
			return false;
		}
		add( qMin( first.toIPv4Address(), last.toIPv4Address() ), qMax( first.toIPv4Address(), last.toIPv4Address() ) );
		return true;
	}

	if( text.contains( QLatin1Char('/') ) )
	{
		const auto subnet = QHostAddress::parseSubnet( text );
		if( subnet.first.protocol() != QAbstractSocket::IPv4Protocol || subnet.second < 0 || subnet.second > 32 )
		{
			return false;
		}
		const auto mask = subnet.second == 0 ? quint32(0) : quint32( 0xffffffffU << ( 32 - subnet.second ) );
		const auto first = subnet.first.toIPv4Address() & mask;
		add( first, first | ~mask );
		return true;
	}

	const QHostAddress address( text );
	if( address.protocol() != QAbstractSocket::IPv4Protocol )
	{
		return false;
	}

	add( address.toIPv4Address(), address.toIPv4Address() );
	return true;
}



void Ipv4RangeSet::add( quint32 first, quint32 last )
{
	m_ranges.append( { first, last } );
	normalize();
}



Ipv4RangeSet Ipv4RangeSet::complement() const
{
	Ipv4RangeSet result;

	quint64 next = 0;
	for( const auto& range : m_ranges )
	{
		if( range.first > next )
		{
			result.m_ranges.append( { quint32( next ), range.first - 1 } );
		}
		next = quint64( range.second ) + 1;
	}

	if( next <= 0xffffffffULL )
	{
		result.m_ranges.append( { quint32( next ), 0xffffffffU } );
	}

	return result;
}



QStringList Ipv4RangeSet::toRangeStrings() const
{
	QStringList strings;
	for( const auto& range : m_ranges )
	{
		strings.append( range.first == range.second ? addressToString( range.first )
													: addressToString( range.first ) + QLatin1Char('-') + addressToString( range.second ) );
	}
	return strings;
}



QStringList Ipv4RangeSet::toCidrStrings() const
{
	QStringList strings;

	for( const auto& range : m_ranges )
	{
		quint64 start = range.first;
		const quint64 end = range.second;

		while( start <= end )
		{
			// largest block aligned at start which fits into the remaining range
			int prefixLength = 32;
			while( prefixLength > 0 )
			{
				const auto blockSize = quint64(1) << ( 32 - ( prefixLength - 1 ) );
				if( ( start % blockSize ) != 0 || start + blockSize - 1 > end )
				{
					break;
				}
				--prefixLength;
			}

			strings.append( QStringLiteral("%1/%2").arg( addressToString( quint32( start ) ) ).arg( prefixLength ) );
			start += quint64(1) << ( 32 - prefixLength );
		}
	}

	return strings;
}



QString Ipv4RangeSet::addressToString( quint32 address )
{
	return QHostAddress( address ).toString();
}



void Ipv4RangeSet::normalize()
{
	std::sort( m_ranges.begin(), m_ranges.end() );

	QList<Range> merged;
	for( const auto& range : std::as_const(m_ranges) )
	{
		if( merged.isEmpty() == false && quint64( range.first ) <= quint64( merged.last().second ) + 1 )
		{
			merged.last().second = qMax( merged.last().second, range.second );
		}
		else
		{
			merged.append( range );
		}
	}

	m_ranges = merged;
}



QStringList NetworkRanges::localIpv4Networks()
{
	return {
		QStringLiteral("0.0.0.0/8"),			// "this" network
		QStringLiteral("10.0.0.0/8"),			// private
		QStringLiteral("100.64.0.0/10"),		// carrier-grade NAT (often used in school networks)
		QStringLiteral("127.0.0.0/8"),			// loopback
		QStringLiteral("169.254.0.0/16"),		// link-local
		QStringLiteral("172.16.0.0/12"),		// private
		QStringLiteral("192.168.0.0/16"),		// private
		QStringLiteral("224.0.0.0/4"),			// multicast
		QStringLiteral("255.255.255.255/32"),	// broadcast
	};
}



void NetworkRanges::resolveHosts( const QStringList& hosts, QStringList* ipv4Addresses, QStringList* ipv6Addresses )
{
	for( const auto& host : hosts )
	{
		const auto trimmedHost = host.trimmed();

		// IP addresses and networks don't need to be resolved
		if( trimmedHost.contains( QLatin1Char('/') ) || QHostAddress( trimmedHost ).isNull() == false ||
			trimmedHost.contains( QRegularExpression( QStringLiteral("^[\\d.]+-[\\d.]+$") ) ) )
		{
			if( trimmedHost.contains( QLatin1Char(':') ) )
			{
				ipv6Addresses->append( trimmedHost );
			}
			else
			{
				ipv4Addresses->append( trimmedHost );
			}
			continue;
		}

		const auto hostName = AddOnUtils::hostFromUrlOrDomain( trimmedHost );
		if( hostName.isEmpty() )
		{
			continue;
		}

		// resolve the domain itself and its "www" variant
		const QStringList names{ hostName, QStringLiteral("www.") + hostName };
		for( const auto& name : names )
		{
			const auto hostInfo = QHostInfo::fromName( name );
			const auto addresses = hostInfo.addresses();
			for( const auto& address : addresses )
			{
				if( address.protocol() == QAbstractSocket::IPv4Protocol )
				{
					ipv4Addresses->append( address.toString() );
				}
				else if( address.protocol() == QAbstractSocket::IPv6Protocol )
				{
					ipv6Addresses->append( address.toString() );
				}
			}
		}
	}

	ipv4Addresses->removeDuplicates();
	ipv6Addresses->removeDuplicates();
}



QStringList NetworkRanges::dnsServers()
{
	QStringList servers;

#if defined(Q_OS_WIN)
	ULONG size = 0;
	if( GetNetworkParams( nullptr, &size ) == ERROR_BUFFER_OVERFLOW && size > 0 )
	{
		QByteArray buffer( int(size), 0 );
		auto info = reinterpret_cast<FIXED_INFO *>( buffer.data() );
		if( GetNetworkParams( info, &size ) == NO_ERROR )
		{
			for( auto server = &info->DnsServerList; server; server = server->Next )
			{
				const auto address = QString::fromLatin1( server->IpAddress.String );
				if( address.isEmpty() == false )
				{
					servers.append( address );
				}
			}
		}
	}
#elif defined(Q_OS_MACOS)
	const auto output = AddOnUtils::runProcess( QStringLiteral("/usr/sbin/scutil"), { QStringLiteral("--dns") }, 10000 ).output;
	static const QRegularExpression nameserverRX( QStringLiteral("nameserver\\[\\d+\\]\\s*:\\s*(\\S+)") );
	auto it = nameserverRX.globalMatch( QString::fromUtf8( output ) );
	while( it.hasNext() )
	{
		servers.append( it.next().captured( 1 ) );
	}
#else
	QFile resolvConf( QStringLiteral("/etc/resolv.conf") );
	if( resolvConf.open( QFile::ReadOnly ) )
	{
		const auto lines = QString::fromUtf8( resolvConf.readAll() ).split( QLatin1Char('\n') );
		for( const auto& line : lines )
		{
			const auto parts = line.simplified().split( QLatin1Char(' ') );
			if( parts.size() >= 2 && parts.first() == QLatin1String("nameserver") )
			{
				servers.append( parts.at( 1 ) );
			}
		}
	}
#endif

	servers.removeDuplicates();
	return servers;
}
