/*
 * MacOsCocoaHelpers.cpp - helper functions for accessing Cocoa (AppKit) APIs
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

#include <QWidget>

#include <objc/message.h>
#include <objc/runtime.h>

#include "MacOsCocoaHelpers.h"

namespace
{

// values from AppKit's NSApplication.h / NSWindow.h
enum : unsigned long {
	NSApplicationPresentationDefault = 0,
	NSApplicationPresentationHideDock = 1 << 1,
	NSApplicationPresentationHideMenuBar = 1 << 3,
	NSApplicationPresentationDisableAppleMenu = 1 << 4,
	NSApplicationPresentationDisableProcessSwitching = 1 << 5,
	NSApplicationPresentationDisableForceQuit = 1 << 6,
	NSApplicationPresentationDisableSessionTermination = 1 << 7,
	NSApplicationPresentationDisableHideApplication = 1 << 8,
};

enum : unsigned long {
	NSWindowCollectionBehaviorDefault = 0,
	NSWindowCollectionBehaviorCanJoinAllSpaces = 1 << 0,
	NSWindowCollectionBehaviorStationary = 1 << 4,
	NSWindowCollectionBehaviorIgnoresCycle = 1 << 6,
	NSWindowCollectionBehaviorFullScreenAuxiliary = 1 << 8,
};

constexpr long NSApplicationActivationPolicyAccessory = 1;

constexpr long NSNormalWindowLevel = 0;
constexpr long NSScreenSaverWindowLevel = 1000;

id sharedApplication()
{
	const auto applicationClass = reinterpret_cast<id>( objc_getClass( "NSApplication" ) );
	if( applicationClass == nullptr )
	{
		return nullptr;
	}

	return reinterpret_cast<id(*)(id, SEL)>( objc_msgSend )( applicationClass, sel_registerName( "sharedApplication" ) );
}

bool respondsToSelector( id object, SEL selector )
{
	return object && reinterpret_cast<BOOL(*)(id, SEL, SEL)>( objc_msgSend )(
				object, sel_registerName( "respondsToSelector:" ), selector );
}

id nativeWindow( QWidget* widget )
{
	if( widget == nullptr )
	{
		return nullptr;
	}

	// on macOS, QWidget::winId() returns the NSView of the native window
	const auto view = reinterpret_cast<id>( widget->winId() );
	if( view == nullptr )
	{
		return nullptr;
	}

	return reinterpret_cast<id(*)(id, SEL)>( objc_msgSend )( view, sel_registerName( "window" ) );
}

}



void MacOsCocoaHelpers::activateApplication()
{
	const auto application = sharedApplication();
	if( application == nullptr )
	{
		return;
	}

	// -[NSApplication activate] is available since macOS 14
	const auto activateSelector = sel_registerName( "activate" );
	if( respondsToSelector( application, activateSelector ) )
	{
		reinterpret_cast<void(*)(id, SEL)>( objc_msgSend )( application, activateSelector );
	}
	else
	{
		reinterpret_cast<void(*)(id, SEL, BOOL)>( objc_msgSend )( application, sel_registerName( "activateIgnoringOtherApps:" ), YES );
	}
}



void MacOsCocoaHelpers::setWindowAboveEverything( QWidget* widget, bool enabled )
{
	const auto window = nativeWindow( widget );
	if( window == nullptr )
	{
		return;
	}

	reinterpret_cast<void(*)(id, SEL, long)>( objc_msgSend )( window, sel_registerName( "setLevel:" ),
															   enabled ? NSScreenSaverWindowLevel : NSNormalWindowLevel );

	reinterpret_cast<void(*)(id, SEL, unsigned long)>( objc_msgSend )(
		window, sel_registerName( "setCollectionBehavior:" ),
		enabled ? ( NSWindowCollectionBehaviorCanJoinAllSpaces |
					NSWindowCollectionBehaviorStationary |
					NSWindowCollectionBehaviorIgnoresCycle |
					NSWindowCollectionBehaviorFullScreenAuxiliary )
				: NSWindowCollectionBehaviorDefault );
}



void MacOsCocoaHelpers::setKioskMode( bool enabled )
{
	const auto application = sharedApplication();
	if( application == nullptr )
	{
		return;
	}

	// presentation options are only applied while the application is active
	if( enabled )
	{
		activateApplication();
	}

	const unsigned long options = enabled ?
									  ( NSApplicationPresentationHideDock |
										NSApplicationPresentationHideMenuBar |
										NSApplicationPresentationDisableAppleMenu |
										NSApplicationPresentationDisableProcessSwitching |
										NSApplicationPresentationDisableForceQuit |
										NSApplicationPresentationDisableSessionTermination |
										NSApplicationPresentationDisableHideApplication )
									: NSApplicationPresentationDefault;

	reinterpret_cast<void(*)(id, SEL, unsigned long)>( objc_msgSend )(
		application, sel_registerName( "setPresentationOptions:" ), options );
}



void MacOsCocoaHelpers::hideFromDock()
{
	const auto application = sharedApplication();
	if( application == nullptr )
	{
		return;
	}

	reinterpret_cast<BOOL(*)(id, SEL, long)>( objc_msgSend )( application, sel_registerName( "setActivationPolicy:" ),
															 NSApplicationActivationPolicyAccessory );
}
