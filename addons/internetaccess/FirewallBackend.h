/*
 * FirewallBackend.h - blocks internet access using the firewall of the operating system
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

#include <QJsonObject>
#include <QStringList>

/*!
 * \brief Blocks outgoing connections to all addresses except the local network and explicitly allowed
 * addresses. Implemented per platform in FirewallBackendWindows.cpp (Windows Defender Firewall rules named
 * "Veyon-Internet-Access-Block"), FirewallBackendMacOS.cpp (pf anchor "com.apple/veyon-internet-access")
 * and FirewallBackendLinux.cpp (nftables table "veyon_internet_access").
 */
class FirewallBackend
{
public:
	struct Rules
	{
		QStringList allowedIpv4;	// addresses, networks (CIDR) and ranges
		QStringList allowedIpv6;	// addresses
	};

	bool block( const Rules& rules, bool enableFirewall, QString* errorString );
	bool unblock( QString* errorString );

	//! \brief State to be restored by unblock() after the Veyon Server has been restarted
	QJsonObject state() const
	{
		return m_state;
	}

	void setState( const QJsonObject& state )
	{
		m_state = state;
	}

private:
	QJsonObject m_state;

};
