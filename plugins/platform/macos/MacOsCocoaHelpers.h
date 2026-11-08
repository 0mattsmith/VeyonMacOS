/*
 * MacOsCocoaHelpers.h - helper functions for accessing Cocoa (AppKit) APIs
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

class QWidget;

// The functions are implemented using the Objective-C runtime API so the plugin can be built
// with plain C++ compilers and build systems (no Objective-C++ toolchain configuration needed).
namespace MacOsCocoaHelpers
{

//! \brief Brings the application to the front (makes it the active application)
void activateApplication();

//! \brief Shows the window above all other windows including the menu bar and Dock and on all spaces
void setWindowAboveEverything( QWidget* widget, bool enabled );

//! \brief Enables or disables a kiosk mode which hides the Dock/menu bar and disables app switching, force quit etc.
void setKioskMode( bool enabled );

//! \brief Removes the application from the Dock and app switcher - windows can still be shown and activated
void hideFromDock();

}
