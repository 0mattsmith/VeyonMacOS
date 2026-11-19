/*
 * MacOsInputInjector.h - injects pointer and keyboard events received from VNC clients
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

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

// Qt-free on purpose (see MacOsScreenCapture.h)
class MacOsInputInjector
{
public:
	MacOsInputInjector();
	~MacOsInputInjector();

	MacOsInputInjector( const MacOsInputInjector& ) = delete;
	MacOsInputInjector& operator=( const MacOsInputInjector& ) = delete;

	// Accessibility permission (Privacy & Security) of this program - required for posting events
	static bool hasPermission();
	// shows the system prompt and adds the program to the list in System Settings
	static void requestPermission();

	// x and y are framebuffer coordinates, mapped onto the main display of the given framebuffer size
	void pointerEvent( int buttonMask, int x, int y, int framebufferWidth, int framebufferHeight );

	// keySym is an X11 keysym as sent by VNC clients
	void keyEvent( bool down, uint32_t keySym );

	// releases pressed keys and buttons, e.g. after the last client disconnected
	void releaseAll();

	// rebuilds the character to key code table for the current keyboard layout (on the main thread)
	void updateKeyboardLayout();

	struct KeyStroke
	{
		uint16_t keyCode;
		uint64_t flags;
		uint16_t character; // != 0 if the character has to be typed as Unicode string (not on the layout)
	};

private:
	void postKey( uint16_t keyCode, bool down, uint64_t flags, uint16_t character );
	uint64_t modifierFlags() const;

	struct Private;
	std::unique_ptr<Private> d;

};
