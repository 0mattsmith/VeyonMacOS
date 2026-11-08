/*
 * MacOsInputDeviceFunctions.cpp - implementation of MacOsInputDeviceFunctions class
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

#include <QSemaphore>
#include <QThread>

#include <ApplicationServices/ApplicationServices.h>

#include "MacOsInputDeviceFunctions.h"
#include "MacOsKeyboardShortcutTrapper.h"
#include "MacOsPlatformConfiguration.h"
#include "VeyonConfiguration.h"


// Swallows all keyboard and mouse events using a Quartz event tap. Creating an event tap at
// HID level requires root privileges (which the Veyon Server has) or the Accessibility permission.
class MacOsInputBlocker : public QThread
{
public:
	MacOsInputBlocker() = default;

	~MacOsInputBlocker() override
	{
		stop();
	}

	void stop()
	{
		if( isRunning() == false )
		{
			return;
		}

		if( m_runLoop )
		{
			CFRunLoopStop( m_runLoop );
		}
		wait();
	}

	bool startAndWait()
	{
		start();
		m_started.acquire();
		return m_tap != nullptr;
	}

protected:
	void run() override
	{
		const CGEventMask mask =
			CGEventMaskBit( kCGEventKeyDown ) |
			CGEventMaskBit( kCGEventKeyUp ) |
			CGEventMaskBit( kCGEventFlagsChanged ) |
			CGEventMaskBit( kCGEventLeftMouseDown ) |
			CGEventMaskBit( kCGEventLeftMouseUp ) |
			CGEventMaskBit( kCGEventRightMouseDown ) |
			CGEventMaskBit( kCGEventRightMouseUp ) |
			CGEventMaskBit( kCGEventOtherMouseDown ) |
			CGEventMaskBit( kCGEventOtherMouseUp ) |
			CGEventMaskBit( kCGEventMouseMoved ) |
			CGEventMaskBit( kCGEventLeftMouseDragged ) |
			CGEventMaskBit( kCGEventRightMouseDragged ) |
			CGEventMaskBit( kCGEventOtherMouseDragged ) |
			CGEventMaskBit( kCGEventScrollWheel ) |
			CGEventMaskBit( 14 ); // NX_SYSDEFINED - media/brightness/volume keys

		m_tap = CGEventTapCreate( kCGHIDEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
								  mask, &MacOsInputBlocker::eventCallback, this );
		if( m_tap == nullptr )
		{
			vWarning() << "could not create event tap - input devices can't be blocked "
						  "(root privileges or Accessibility permission required)";
			m_started.release();
			return;
		}

		auto source = CFMachPortCreateRunLoopSource( kCFAllocatorDefault, m_tap, 0 );
		m_runLoop = CFRunLoopGetCurrent();
		CFRunLoopAddSource( m_runLoop, source, kCFRunLoopCommonModes );
		CGEventTapEnable( m_tap, true );

		m_started.release();

		CFRunLoopRun();

		CGEventTapEnable( m_tap, false );
		CFRunLoopRemoveSource( m_runLoop, source, kCFRunLoopCommonModes );
		CFRelease( source );
		CFMachPortInvalidate( m_tap );
		CFRelease( m_tap );
		m_tap = nullptr;
		m_runLoop = nullptr;
	}

private:
	static CGEventRef eventCallback( CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* userInfo )
	{
		Q_UNUSED(proxy)

		auto blocker = static_cast<MacOsInputBlocker *>( userInfo );

		// the system disables taps which take too long or on user input - re-enable immediately
		if( type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput )
		{
			if( blocker->m_tap )
			{
				CGEventTapEnable( blocker->m_tap, true );
			}
			return event;
		}

		// drop event
		return nullptr;
	}

	CFMachPortRef m_tap{nullptr};
	CFRunLoopRef m_runLoop{nullptr};
	QSemaphore m_started;

};



MacOsInputDeviceFunctions::~MacOsInputDeviceFunctions()
{
	delete m_inputBlocker;
}



void MacOsInputDeviceFunctions::enableInputDevices()
{
	if( m_inputBlocker )
	{
		m_inputBlocker->stop();
		delete m_inputBlocker;
		m_inputBlocker = nullptr;
	}
}



void MacOsInputDeviceFunctions::disableInputDevices()
{
	if( m_inputBlocker || MacOsPlatformConfiguration( &VeyonCore::config() ).blockInputDevices() == false )
	{
		return;
	}

	m_inputBlocker = new MacOsInputBlocker;
	if( m_inputBlocker->startAndWait() == false )
	{
		m_inputBlocker->wait();
		delete m_inputBlocker;
		m_inputBlocker = nullptr;
	}
}



KeyboardShortcutTrapper* MacOsInputDeviceFunctions::createKeyboardShortcutTrapper( QObject* parent )
{
	return new MacOsKeyboardShortcutTrapper( parent );
}
