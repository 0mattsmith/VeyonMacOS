/*
 * NetworkRanges.h - helpers for working with IP address ranges
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

#include <QList>
#include <QPair>
#include <QStringList>

//! \brief A set of IPv4 address ranges which can be merged and inverted
class Ipv4RangeSet
{
public:
	using Range = QPair<quint32, quint32>;

	//! \brief Adds an address ("10.1.2.3"), a CIDR network ("10.0.0.0/8") or a range ("10.0.0.1-10.0.0.9")
	bool add( const QString& specification );
	void add( quint32 first, quint32 last );

	Ipv4RangeSet complement() const;

	//! \brief Returns ranges as strings ("a.b.c.d-e.f.g.h" or "a.b.c.d" for single addresses)
	QStringList toRangeStrings() const;

	//! \brief Returns ranges as list of CIDR networks
	QStringList toCidrStrings() const;

	const QList<Range>& ranges() const
	{
		return m_ranges;
	}

	bool isEmpty() const
	{
		return m_ranges.isEmpty();
	}

	static QString addressToString( quint32 address );

private:
	void normalize();

	QList<Range> m_ranges;

};


namespace NetworkRanges
{

//! \brief Networks which never leave the local network (private, loopback, link-local, multicast)
QStringList localIpv4Networks();

//! \brief Resolves host names and returns IPv4 and IPv6 addresses (resolving can take some time)
void resolveHosts( const QStringList& hosts, QStringList* ipv4Addresses, QStringList* ipv6Addresses );

//! \brief Returns the DNS servers configured on this computer
QStringList dnsServers();

}
