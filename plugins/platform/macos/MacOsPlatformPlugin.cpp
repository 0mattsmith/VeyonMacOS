/*
 * MacOsPlatformPlugin.cpp - implementation of MacOsPlatformPlugin class
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

#include <csignal>

#include <QGuiApplication>

#include "MacOsCocoaHelpers.h"
#include "MacOsPlatformConfiguration.h"
#include "MacOsPlatformPlugin.h"


MacOsPlatformPlugin::MacOsPlatformPlugin( QObject* parent ) :
	QObject( parent )
{
	// don't abort with SIGPIPE when writing to closed sockets e.g. while shutting down VncConnection
	::signal( SIGPIPE, SIG_IGN );

	// all programs are located in Veyon.app and therefore share its Info.plist - only Veyon Master and
	// Veyon Configurator should appear in the Dock while the server, workers and veyon-cli run in the background
	const auto component = VeyonCore::component();
	if( qobject_cast<QGuiApplication *>( QCoreApplication::instance() ) &&
		component != VeyonCore::Component::Master && component != VeyonCore::Component::Configurator )
	{
		MacOsCocoaHelpers::hideFromDock();
	}
}


IMPLEMENT_CONFIG_PROXY(MacOsPlatformConfiguration)
