/*
 * MacOsPlatformConfiguration.h - configuration values for MacOsPlatform plugin
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

#define FOREACH_MACOS_PLATFORM_CONFIG_PROPERTY(OP) \
	OP( MacOsPlatformConfiguration, m_configuration, QString, pamServiceName, setPamServiceName, "PamServiceName", "MacOS", QStringLiteral("checkpw"), Configuration::Property::Flag::Advanced ) \
	OP( MacOsPlatformConfiguration, m_configuration, bool, blockInputDevices, setBlockInputDevices, "BlockInputDevices", "MacOS", true, Configuration::Property::Flag::Advanced ) \
	OP( MacOsPlatformConfiguration, m_configuration, int, minimumUserSessionLifetime, setMinimumUserSessionLifetime, "MinimumUserSessionLifetime", "MacOS", 3, Configuration::Property::Flag::Advanced ) \

DECLARE_CONFIG_PROXY(MacOsPlatformConfiguration, FOREACH_MACOS_PLATFORM_CONFIG_PROPERTY)
