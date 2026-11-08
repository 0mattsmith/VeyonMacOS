/*
 * FirewallBackendLinux.cpp - blocks internet access using nftables
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
#include <QStandardPaths>

#include "AddOnUtils.h"
#include "FirewallBackend.h"
#include "NetworkRanges.h"


namespace {

const auto TableName = QStringLiteral("veyon_internet_access");

QString nftPath()
{
	auto path = QStandardPaths::findExecutable( QStringLiteral("nft") );
	if( path.isEmpty() )
	{
		path = QStandardPaths::findExecutable( QStringLiteral("nft"), { QStringLiteral("/usr/sbin"), QStringLiteral("/sbin") } );
	}
	return path;
}

}



bool FirewallBackend::block( const Rules& rules, bool enableFirewall, QString* errorString )
{
	Q_UNUSED(enableFirewall)

	const auto nft = nftPath();
	if( nft.isEmpty() )
	{
		*errorString = QCoreApplication::translate( "FirewallBackend", "nftables (nft) is not installed on this computer." );
		return false;
	}

	Ipv4RangeSet allowedRanges;
	for( const auto& specification : rules.allowedIpv4 )
	{
		allowedRanges.add( specification );
	}

	QStringList chainRules;
	if( rules.allowedIpv6.isEmpty() == false )
	{
		chainRules.append( QStringLiteral("ip6 daddr { %1 } accept").arg( rules.allowedIpv6.join( QStringLiteral(", ") ) ) );
	}
	chainRules.append( QStringLiteral("ip daddr != { %1 } drop").arg( allowedRanges.toCidrStrings().join( QStringLiteral(", ") ) ) );
	chainRules.append( QStringLiteral("ip6 daddr 2000::/3 drop") );

	// create the table if it does not exist yet so it can be deleted and recreated atomically
	const auto ruleset = QStringLiteral(
		"table inet %1 {}\n"
		"delete table inet %1\n"
		"table inet %1 {\n"
		"  chain output {\n"
		"    type filter hook output priority 0; policy accept;\n"
		"    %2\n"
		"  }\n"
		"}\n" ).arg( TableName, chainRules.join( QStringLiteral("\n    ") ) );

	const auto result = AddOnUtils::runProcess( nft, { QStringLiteral("-f"), QStringLiteral("-") }, 30000, ruleset.toUtf8() );
	if( result.exitCode != 0 )
	{
		*errorString = QCoreApplication::translate( "FirewallBackend", "Could not load firewall rules: %1" ).
					   arg( QString::fromUtf8( result.output ).trimmed() );
		return false;
	}

	return true;
}



bool FirewallBackend::unblock( QString* errorString )
{
	Q_UNUSED(errorString)

	const auto nft = nftPath();
	if( nft.isEmpty() == false )
	{
		AddOnUtils::runProcess( nft, { QStringLiteral("delete"), QStringLiteral("table"), QStringLiteral("inet"), TableName }, 30000 );
	}

	return true;
}
