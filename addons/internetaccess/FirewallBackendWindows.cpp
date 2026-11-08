/*
 * FirewallBackendWindows.cpp - blocks internet access using Windows Defender Firewall
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
#include <QJsonArray>
#include <QSettings>

#include "AddOnUtils.h"
#include "FirewallBackend.h"
#include "NetworkRanges.h"


namespace {

// rule name without spaces to avoid any quoting issues with netsh
const auto RuleName = QStringLiteral("name=Veyon-Internet-Access-Block");

constexpr int MaximumRangesPerRule = 100;

int netsh( const QStringList& arguments )
{
	return AddOnUtils::runProcess( QStringLiteral("netsh.exe"), arguments, 30000 ).exitCode;
}

// returns the firewall profiles (netsh names) which are currently turned off
QStringList disabledFirewallProfiles()
{
	const QList<std::pair<QString, QString>> profiles{
		{ QStringLiteral("DomainProfile"), QStringLiteral("domainprofile") },
		{ QStringLiteral("StandardProfile"), QStringLiteral("privateprofile") },
		{ QStringLiteral("PublicProfile"), QStringLiteral("publicprofile") },
	};

	QStringList disabledProfiles;
	for( const auto& [registryName, netshName] : profiles )
	{
		const QSettings settings( QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\%1").arg( registryName ),
								  QSettings::Registry64Format );
		if( settings.value( QStringLiteral("EnableFirewall"), 1 ).toInt() == 0 )
		{
			disabledProfiles.append( netshName );
		}
	}

	return disabledProfiles;
}

}



bool FirewallBackend::block( const Rules& rules, bool enableFirewall, QString* errorString )
{
	unblock( nullptr );

	const auto disabledProfiles = disabledFirewallProfiles();
	if( disabledProfiles.isEmpty() == false )
	{
		if( enableFirewall == false )
		{
			*errorString = QCoreApplication::translate( "FirewallBackend",
														"Windows Firewall is turned off on this computer, so internet access can't be "
														"blocked. Turn on Windows Firewall or allow Veyon to turn it on in Veyon Configurator." );
			return false;
		}

		for( const auto& profile : disabledProfiles )
		{
			netsh( { QStringLiteral("advfirewall"), QStringLiteral("set"), profile, QStringLiteral("state"), QStringLiteral("on") } );
		}

		// remember to turn the firewall off again when internet access is allowed again
		m_state[QStringLiteral("enabledProfiles")] = QJsonArray::fromStringList( disabledProfiles );
	}

	Ipv4RangeSet allowedRanges;
	for( const auto& specification : rules.allowedIpv4 )
	{
		allowedRanges.add( specification );
	}

	// block rules take precedence over allow rules, so only ranges outside the allowed ones are blocked
	const auto blockedRanges = allowedRanges.complement().toRangeStrings();

	for( int i = 0; i < blockedRanges.size(); i += MaximumRangesPerRule )
	{
		const auto rangeList = blockedRanges.mid( i, MaximumRangesPerRule ).join( QLatin1Char(',') );
		if( netsh( { QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"), QStringLiteral("rule"),
					 RuleName, QStringLiteral("dir=out"), QStringLiteral("action=block"), QStringLiteral("enable=yes"),
					 QStringLiteral("profile=any"), QStringLiteral("remoteip=%1").arg( rangeList ) } ) != 0 )
		{
			*errorString = QCoreApplication::translate( "FirewallBackend", "Could not add Windows Firewall rule." );
			unblock( nullptr );
			return false;
		}
	}

	// global IPv6 addresses - allowed websites are still reachable via IPv4
	netsh( { QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"), QStringLiteral("rule"),
			 RuleName, QStringLiteral("dir=out"), QStringLiteral("action=block"), QStringLiteral("enable=yes"),
			 QStringLiteral("profile=any"), QStringLiteral("remoteip=2000::/3") } );

	return true;
}



bool FirewallBackend::unblock( QString* errorString )
{
	Q_UNUSED(errorString)

	// removes all rules with this name - succeeds also if no rule exists
	netsh( { QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("delete"), QStringLiteral("rule"), RuleName } );

	const auto enabledProfiles = m_state[QStringLiteral("enabledProfiles")].toArray();
	for( const auto& profile : enabledProfiles )
	{
		netsh( { QStringLiteral("advfirewall"), QStringLiteral("set"), profile.toString(), QStringLiteral("state"), QStringLiteral("off") } );
	}
	m_state.remove( QStringLiteral("enabledProfiles") );

	return true;
}
