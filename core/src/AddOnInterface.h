/*
 * AddOnInterface.h - interface class for Veyon add-ons
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

#include <QStringList>

#include "PluginInterface.h"

// clazy:excludeall=copyable-polymorphic

/*!
 * \brief Interface for plugins which should be presented as add-ons in the Add-On Store.
 *
 * Every plugin implementing this interface (in addition to PluginInterface) shows up in
 * the Add-On Store where it can be enabled and disabled by the user. Plugins which do not
 * implement this interface but provide features are listed as built-in features.
 */
class VEYON_CORE_EXPORT AddOnInterface
{
	Q_GADGET
public:
	enum class Category
	{
		Other,
		Monitoring,
		Control,
		Files,
		Network,
		Communication,
		Scripting,
	};
	Q_ENUM(Category)

	virtual ~AddOnInterface() = default;

	//! \brief Human readable name of the add-on - PluginInterface::name() is used if empty
	virtual QString addOnName() const
	{
		return {};
	}

	virtual Category addOnCategory() const = 0;

	//! \brief Resource URL of an icon representing the add-on (at least 64x64 pixels)
	virtual QString addOnIconUrl() const = 0;

	//! \brief Long description which is displayed in the Add-On Store
	virtual QString addOnSummary() const
	{
		return {};
	}

	//! \brief Platforms on which the client (student computer) part of the add-on is functional
	virtual QStringList addOnClientPlatforms() const
	{
		return { QStringLiteral("windows"), QStringLiteral("macos"), QStringLiteral("linux") };
	}

	/*!
	 * \brief Reverts all changes the add-on made to this computer (e.g. firewall rules or browser policies)
	 * and restores the previous settings. Called by "veyon-cli addons revert" with administrative privileges
	 * while the Veyon Service is stopped, e.g. before Veyon is uninstalled.
	 */
	virtual bool revertSystemChanges( QString* errorString )
	{
		Q_UNUSED(errorString)
		return true;
	}

};

#define AddOnInterface_iid "io.veyon.Veyon.Plugins.AddOnInterface"

Q_DECLARE_INTERFACE(AddOnInterface, AddOnInterface_iid)
