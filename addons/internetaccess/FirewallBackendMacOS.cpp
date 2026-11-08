/*
 * FirewallBackendMacOS.cpp - blocks internet access using the packet filter (pf) of macOS
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
#include <QRegularExpression>

#include "AddOnUtils.h"
#include "FirewallBackend.h"
#include "NetworkRanges.h"


namespace {

// anchors below "com.apple" are evaluated by the default pf configuration of macOS
const auto AnchorName = QStringLiteral("com.apple/veyon-internet-access");
const auto PfctlPath = QStringLiteral("/sbin/pfctl");

}



bool FirewallBackend::block( const Rules& rules, bool enableFirewall, QString* errorString )
{
	Q_UNUSED(enableFirewall)

	Ipv4RangeSet allowedRanges;
	for( const auto& specification : rules.allowedIpv4 )
	{
		allowedRanges.add( specification );
	}

	QStringList ruleLines{
		QStringLiteral("table <veyon_allowed4> const { %1 }").arg( allowedRanges.toCidrStrings().join( QStringLiteral(", ") ) ),
	};
	if( rules.allowedIpv6.isEmpty() == false )
	{
		ruleLines.append( QStringLiteral("table <veyon_allowed6> const { %1 }").arg( rules.allowedIpv6.join( QStringLiteral(", ") ) ) );
		ruleLines.append( QStringLiteral("pass out quick inet6 to <veyon_allowed6>") );
	}
	ruleLines.append( QStringLiteral("block drop out quick inet to ! <veyon_allowed4>") );
	ruleLines.append( QStringLiteral("block drop out quick inet6 to 2000::/3") );

	const auto loadResult = AddOnUtils::runProcess( PfctlPath, { QStringLiteral("-a"), AnchorName, QStringLiteral("-f"), QStringLiteral("-") },
													30000, ruleLines.join( QLatin1Char('\n') ).toUtf8() + '\n' );
	if( loadResult.exitCode != 0 )
	{
		*errorString = QCoreApplication::translate( "FirewallBackend", "Could not load firewall rules: %1" ).
					   arg( QString::fromUtf8( loadResult.output ).trimmed() );
		return false;
	}

	// enable pf (reference counted - the token is used to release our reference again)
	if( m_state.contains( QStringLiteral("pfToken") ) == false )
	{
		const auto enableResult = AddOnUtils::runProcess( PfctlPath, { QStringLiteral("-E") }, 30000 );
		static const QRegularExpression tokenRX( QStringLiteral("Token\\s*:\\s*(\\d+)") );
		const auto match = tokenRX.match( QString::fromUtf8( enableResult.output ) );
		if( match.hasMatch() )
		{
			m_state[QStringLiteral("pfToken")] = match.captured( 1 );
		}
	}

	return true;
}



bool FirewallBackend::unblock( QString* errorString )
{
	Q_UNUSED(errorString)

	AddOnUtils::runProcess( PfctlPath, { QStringLiteral("-a"), AnchorName, QStringLiteral("-F"), QStringLiteral("all") }, 30000 );

	const auto token = m_state[QStringLiteral("pfToken")].toString();
	if( token.isEmpty() == false )
	{
		AddOnUtils::runProcess( PfctlPath, { QStringLiteral("-X"), token }, 30000 );
		m_state.remove( QStringLiteral("pfToken") );
	}

	return true;
}
