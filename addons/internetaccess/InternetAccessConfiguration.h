/*
 * InternetAccessConfiguration.h - configuration values for the Internet Access add-on
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

#include "Configuration/Proxy.h"

#define FOREACH_INTERNET_ACCESS_CONFIG_PROPERTY(OP) \
	OP( InternetAccessConfiguration, m_configuration, QStringList, alwaysAllowed, setAlwaysAllowed, "AlwaysAllowed", "InternetAccess", QStringList(), Configuration::Property::Flag::Standard ) \
	OP( InternetAccessConfiguration, m_configuration, bool, notifyStudents, setNotifyStudents, "NotifyStudents", "InternetAccess", true, Configuration::Property::Flag::Standard ) \
	OP( InternetAccessConfiguration, m_configuration, bool, enableWindowsFirewall, setEnableWindowsFirewall, "EnableWindowsFirewall", "InternetAccess", false, Configuration::Property::Flag::Standard ) \

DECLARE_CONFIG_PROXY(InternetAccessConfiguration, FOREACH_INTERNET_ACCESS_CONFIG_PROPERTY)
