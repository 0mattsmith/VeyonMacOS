/*
 * MacOsInputInjector.cpp - injects pointer and keyboard events received from VNC clients
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

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <dispatch/dispatch.h>

#include <chrono>
#include <cmath>
#include <unordered_map>

#include "MacOsInputInjector.h"


namespace {

// X11 keysyms sent by VNC clients (see rfb/keysym.h) - only the ones handled here
enum KeySym : uint32_t
{
	XK_BackSpace = 0xff08, XK_Tab = 0xff09, XK_Return = 0xff0d, XK_Escape = 0xff1b, XK_Delete = 0xffff,
	XK_Home = 0xff50, XK_Left = 0xff51, XK_Up = 0xff52, XK_Right = 0xff53, XK_Down = 0xff54,
	XK_Page_Up = 0xff55, XK_Page_Down = 0xff56, XK_End = 0xff57, XK_Insert = 0xff63, XK_Help = 0xff6a,
	XK_Mode_switch = 0xff7e, XK_Num_Lock = 0xff7f,
	XK_KP_Enter = 0xff8d, XK_KP_Home = 0xff95, XK_KP_Left = 0xff96, XK_KP_Up = 0xff97, XK_KP_Right = 0xff98,
	XK_KP_Down = 0xff99, XK_KP_Page_Up = 0xff9a, XK_KP_Page_Down = 0xff9b, XK_KP_End = 0xff9c,
	XK_KP_Insert = 0xff9e, XK_KP_Delete = 0xff9f, XK_KP_Equal = 0xffbd, XK_KP_Multiply = 0xffaa,
	XK_KP_Add = 0xffab, XK_KP_Subtract = 0xffad, XK_KP_Decimal = 0xffae, XK_KP_Divide = 0xffaf,
	XK_KP_0 = 0xffb0, XK_KP_9 = 0xffb9,
	XK_F1 = 0xffbe, XK_F20 = 0xffd1,
	XK_Shift_L = 0xffe1, XK_Shift_R = 0xffe2, XK_Control_L = 0xffe3, XK_Control_R = 0xffe4,
	XK_Caps_Lock = 0xffe5, XK_Meta_L = 0xffe7, XK_Meta_R = 0xffe8, XK_Alt_L = 0xffe9, XK_Alt_R = 0xffea,
	XK_Super_L = 0xffeb, XK_Super_R = 0xffec,
	XK_ISO_Level3_Shift = 0xfe03,
	XK_EuroSign = 0x20ac,
};

const std::unordered_map<uint32_t, uint16_t> SpecialKeys = {
	{ XK_BackSpace, kVK_Delete }, { XK_Tab, kVK_Tab }, { XK_Return, kVK_Return }, { XK_Escape, kVK_Escape },
	{ XK_Delete, kVK_ForwardDelete }, { XK_Home, kVK_Home }, { XK_End, kVK_End },
	{ XK_Page_Up, kVK_PageUp }, { XK_Page_Down, kVK_PageDown },
	{ XK_Left, kVK_LeftArrow }, { XK_Right, kVK_RightArrow }, { XK_Up, kVK_UpArrow }, { XK_Down, kVK_DownArrow },
	{ XK_Insert, kVK_Help }, { XK_Help, kVK_Help }, { XK_Num_Lock, kVK_ANSI_KeypadClear },
	{ XK_KP_Enter, kVK_ANSI_KeypadEnter }, { XK_KP_Home, kVK_Home }, { XK_KP_End, kVK_End },
	{ XK_KP_Left, kVK_LeftArrow }, { XK_KP_Right, kVK_RightArrow }, { XK_KP_Up, kVK_UpArrow },
	{ XK_KP_Down, kVK_DownArrow }, { XK_KP_Page_Up, kVK_PageUp }, { XK_KP_Page_Down, kVK_PageDown },
	{ XK_KP_Insert, kVK_Help }, { XK_KP_Delete, kVK_ForwardDelete },
	{ XK_KP_Equal, kVK_ANSI_KeypadEquals }, { XK_KP_Multiply, kVK_ANSI_KeypadMultiply },
	{ XK_KP_Add, kVK_ANSI_KeypadPlus }, { XK_KP_Subtract, kVK_ANSI_KeypadMinus },
	{ XK_KP_Decimal, kVK_ANSI_KeypadDecimal }, { XK_KP_Divide, kVK_ANSI_KeypadDivide },
	{ XK_KP_0, kVK_ANSI_Keypad0 }, { XK_KP_0 + 1, kVK_ANSI_Keypad1 }, { XK_KP_0 + 2, kVK_ANSI_Keypad2 },
	{ XK_KP_0 + 3, kVK_ANSI_Keypad3 }, { XK_KP_0 + 4, kVK_ANSI_Keypad4 }, { XK_KP_0 + 5, kVK_ANSI_Keypad5 },
	{ XK_KP_0 + 6, kVK_ANSI_Keypad6 }, { XK_KP_0 + 7, kVK_ANSI_Keypad7 }, { XK_KP_0 + 8, kVK_ANSI_Keypad8 },
	{ XK_KP_9, kVK_ANSI_Keypad9 },
	{ XK_F1, kVK_F1 }, { XK_F1 + 1, kVK_F2 }, { XK_F1 + 2, kVK_F3 }, { XK_F1 + 3, kVK_F4 },
	{ XK_F1 + 4, kVK_F5 }, { XK_F1 + 5, kVK_F6 }, { XK_F1 + 6, kVK_F7 }, { XK_F1 + 7, kVK_F8 },
	{ XK_F1 + 8, kVK_F9 }, { XK_F1 + 9, kVK_F10 }, { XK_F1 + 10, kVK_F11 }, { XK_F1 + 11, kVK_F12 },
	{ XK_F1 + 12, kVK_F13 }, { XK_F1 + 13, kVK_F14 }, { XK_F1 + 14, kVK_F15 }, { XK_F1 + 15, kVK_F16 },
	{ XK_F1 + 16, kVK_F17 }, { XK_F1 + 17, kVK_F18 }, { XK_F1 + 18, kVK_F19 }, { XK_F20, kVK_F20 },
	{ XK_Caps_Lock, kVK_CapsLock },
};

struct Modifier
{
	uint16_t keyCode;
	uint64_t flag;
};

// Windows/Super key = Command, Alt = Option (as Apple's own Screen Sharing does it)
const std::unordered_map<uint32_t, Modifier> ModifierKeys = {
	{ XK_Shift_L, { kVK_Shift, kCGEventFlagMaskShift } },
	{ XK_Shift_R, { kVK_RightShift, kCGEventFlagMaskShift } },
	{ XK_Control_L, { kVK_Control, kCGEventFlagMaskControl } },
	{ XK_Control_R, { kVK_RightControl, kCGEventFlagMaskControl } },
	{ XK_Alt_L, { kVK_Option, kCGEventFlagMaskAlternate } },
	{ XK_Alt_R, { kVK_RightOption, kCGEventFlagMaskAlternate } },
	{ XK_Meta_L, { kVK_Command, kCGEventFlagMaskCommand } },
	{ XK_Meta_R, { kVK_RightCommand, kCGEventFlagMaskCommand } },
	{ XK_Super_L, { kVK_Command, kCGEventFlagMaskCommand } },
	{ XK_Super_R, { kVK_RightCommand, kCGEventFlagMaskCommand } },
};

constexpr auto DoubleClickInterval = std::chrono::milliseconds( 500 );
constexpr double DoubleClickDistance = 4;

// VNC button mask bits
constexpr int ButtonLeft = 1 << 0;
constexpr int ButtonMiddle = 1 << 1;
constexpr int ButtonRight = 1 << 2;
constexpr int WheelUp = 1 << 3;
constexpr int WheelDown = 1 << 4;
constexpr int WheelLeft = 1 << 5;
constexpr int WheelRight = 1 << 6;

uint32_t keySymToUnicode( uint32_t keySym )
{
	if( ( keySym >= 0x20 && keySym <= 0x7e ) || ( keySym >= 0xa0 && keySym <= 0xff ) )
	{
		return keySym; // Latin-1 keysyms equal their Unicode code points
	}
	if( ( keySym & 0xff000000 ) == 0x01000000 )
	{
		return keySym & 0x00ffffff; // directly encoded Unicode
	}
	if( keySym == XK_EuroSign )
	{
		return 0x20ac;
	}
	return 0;
}

}



// shared with the asynchronous layout update on the main thread, which may finish after the injector is gone
struct CharacterTable
{
	std::mutex mutex;
	std::unordered_map<uint16_t, MacOsInputInjector::KeyStroke> strokes;
};

struct MacOsInputInjector::Private
{
	CGEventSourceRef source{nullptr};

	std::shared_ptr<CharacterTable> characterTable{std::make_shared<CharacterTable>()};
	std::chrono::steady_clock::time_point layoutUpdated{};

	// modifier keysyms currently held by the client
	std::unordered_map<uint32_t, uint64_t> heldModifiers;
	// what was pressed for a keysym, so that the matching key is released
	std::unordered_map<uint32_t, KeyStroke> pressedKeys;

	int buttonMask{0};
	CGPoint position{0, 0};

	int clickButton{0};
	int clickCount{0};
	CGPoint clickPosition{0, 0};
	std::chrono::steady_clock::time_point clickTime{};
};



MacOsInputInjector::MacOsInputInjector() :
	d( std::make_unique<Private>() )
{
	d->source = CGEventSourceCreate( kCGEventSourceStateHIDSystemState );
	updateKeyboardLayout();
}



MacOsInputInjector::~MacOsInputInjector()
{
	releaseAll();
	if( d->source )
	{
		CFRelease( d->source );
	}
}



bool MacOsInputInjector::hasPermission()
{
	return AXIsProcessTrusted();
}



void MacOsInputInjector::requestPermission()
{
	const void* keys[] = { kAXTrustedCheckOptionPrompt };
	const void* values[] = { kCFBooleanTrue };
	auto options = CFDictionaryCreate( kCFAllocatorDefault, keys, values, 1,
									   &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	AXIsProcessTrustedWithOptions( options );
	CFRelease( options );
}



void MacOsInputInjector::pointerEvent( int buttonMask, int x, int y, int framebufferWidth, int framebufferHeight )
{
	if( framebufferWidth <= 0 || framebufferHeight <= 0 )
	{
		return;
	}

	const auto bounds = CGDisplayBounds( CGMainDisplayID() );
	const CGPoint position{
		bounds.origin.x + ( x + 0.5 ) * bounds.size.width / framebufferWidth,
		bounds.origin.y + ( y + 0.5 ) * bounds.size.height / framebufferHeight
	};

	const auto previousMask = d->buttonMask;
	const auto pressed = buttonMask & ~previousMask;
	const auto released = previousMask & ~buttonMask;

	if( position.x != d->position.x || position.y != d->position.y || ( pressed | released ) == 0 )
	{
		CGEventType type = kCGEventMouseMoved;
		CGMouseButton button = kCGMouseButtonLeft;
		if( previousMask & ButtonLeft )
		{
			type = kCGEventLeftMouseDragged;
		}
		else if( previousMask & ButtonRight )
		{
			type = kCGEventRightMouseDragged;
			button = kCGMouseButtonRight;
		}
		else if( previousMask & ButtonMiddle )
		{
			type = kCGEventOtherMouseDragged;
			button = kCGMouseButtonCenter;
		}

		if( auto event = CGEventCreateMouseEvent( d->source, type, position, button ) )
		{
			CGEventSetFlags( event, CGEventFlags( modifierFlags() ) );
			CGEventPost( kCGHIDEventTap, event );
			CFRelease( event );
		}
		d->position = position;
	}

	struct ButtonMapping
	{
		int mask;
		CGMouseButton button;
		CGEventType downType;
		CGEventType upType;
	};
	static constexpr ButtonMapping buttons[] = {
		{ ButtonLeft, kCGMouseButtonLeft, kCGEventLeftMouseDown, kCGEventLeftMouseUp },
		{ ButtonRight, kCGMouseButtonRight, kCGEventRightMouseDown, kCGEventRightMouseUp },
		{ ButtonMiddle, kCGMouseButtonCenter, kCGEventOtherMouseDown, kCGEventOtherMouseUp },
	};

	for( const auto& mapping : buttons )
	{
		const bool down = pressed & mapping.mask;
		const bool up = released & mapping.mask;
		if( down == false && up == false )
		{
			continue;
		}

		if( down )
		{
			// macOS needs the click count for double and triple clicks
			const auto now = std::chrono::steady_clock::now();
			if( d->clickButton == mapping.mask && now - d->clickTime < DoubleClickInterval &&
				std::hypot( position.x - d->clickPosition.x, position.y - d->clickPosition.y ) < DoubleClickDistance )
			{
				++d->clickCount;
			}
			else
			{
				d->clickCount = 1;
			}
			d->clickButton = mapping.mask;
			d->clickTime = now;
			d->clickPosition = position;
		}

		if( auto event = CGEventCreateMouseEvent( d->source, down ? mapping.downType : mapping.upType,
												  position, mapping.button ) )
		{
			CGEventSetIntegerValueField( event, kCGMouseEventClickState,
										 d->clickButton == mapping.mask ? d->clickCount : 1 );
			CGEventSetFlags( event, CGEventFlags( modifierFlags() ) );
			CGEventPost( kCGHIDEventTap, event );
			CFRelease( event );
		}
	}

	// VNC clients send a press and a release for every step of the wheel
	const int verticalSteps = ( ( pressed & WheelUp ) ? 1 : 0 ) - ( ( pressed & WheelDown ) ? 1 : 0 );
	const int horizontalSteps = ( ( pressed & WheelLeft ) ? 1 : 0 ) - ( ( pressed & WheelRight ) ? 1 : 0 );
	if( verticalSteps != 0 || horizontalSteps != 0 )
	{
		if( auto event = CGEventCreateScrollWheelEvent( d->source, kCGScrollEventUnitLine, 2,
														verticalSteps, horizontalSteps ) )
		{
			CGEventSetFlags( event, CGEventFlags( modifierFlags() ) );
			CGEventPost( kCGHIDEventTap, event );
			CFRelease( event );
		}
	}

	d->buttonMask = buttonMask;
}



void MacOsInputInjector::keyEvent( bool down, uint32_t keySym )
{
	if( const auto modifier = ModifierKeys.find( keySym ); modifier != ModifierKeys.end() )
	{
		if( down )
		{
			d->heldModifiers[keySym] = modifier->second.flag;
		}
		else
		{
			d->heldModifiers.erase( keySym );
		}
		postKey( modifier->second.keyCode, down, modifierFlags(), 0 );
		return;
	}

	// AltGr: the character table already contains the Option key where a character needs it
	if( keySym == XK_ISO_Level3_Shift || keySym == XK_Mode_switch )
	{
		return;
	}

	if( down == false )
	{
		if( const auto pressed = d->pressedKeys.find( keySym ); pressed != d->pressedKeys.end() )
		{
			postKey( pressed->second.keyCode, false, pressed->second.flags, pressed->second.character );
			d->pressedKeys.erase( pressed );
		}
		return;
	}

	KeyStroke stroke{ 0, modifierFlags(), 0 };

	if( const auto special = SpecialKeys.find( keySym ); special != SpecialKeys.end() )
	{
		stroke.keyCode = special->second;
	}
	else if( const auto character = keySymToUnicode( keySym ); character != 0 )
	{
		auto flags = stroke.flags;
		// Windows clients send AltGr as Control+Alt - drop both unless they are part of a shortcut with Command
		if( ( flags & kCGEventFlagMaskControl ) && ( flags & kCGEventFlagMaskAlternate ) &&
			( flags & kCGEventFlagMaskCommand ) == 0 )
		{
			flags &= ~uint64_t( kCGEventFlagMaskControl | kCGEventFlagMaskAlternate );
		}

		if( std::chrono::steady_clock::now() - d->layoutUpdated > std::chrono::seconds( 5 ) )
		{
			updateKeyboardLayout(); // picks up a changed keyboard layout for the next key press
		}

		std::lock_guard<std::mutex> lock( d->characterTable->mutex );
		const auto& strokes = d->characterTable->strokes;
		if( character <= 0xffff && strokes.count( uint16_t( character ) ) )
		{
			const auto& entry = strokes.at( uint16_t( character ) );
			stroke.keyCode = entry.keyCode;
			// the character table decides about Shift and Option, the client's state about the rest
			stroke.flags = ( flags & ~uint64_t( kCGEventFlagMaskShift | kCGEventFlagMaskAlternate ) ) | entry.flags;
		}
		else if( character <= 0xffff )
		{
			stroke.flags = flags;
			stroke.character = uint16_t( character );
		}
		else
		{
			return;
		}
	}
	else
	{
		return;
	}

	d->pressedKeys[keySym] = stroke;
	postKey( stroke.keyCode, true, stroke.flags, stroke.character );
}



void MacOsInputInjector::releaseAll()
{
	for( const auto& [keySym, stroke] : d->pressedKeys )
	{
		postKey( stroke.keyCode, false, stroke.flags, stroke.character );
	}
	d->pressedKeys.clear();

	for( const auto& [keySym, flag] : d->heldModifiers )
	{
		postKey( ModifierKeys.at( keySym ).keyCode, false, 0, 0 );
	}
	d->heldModifiers.clear();

	if( d->buttonMask & ( ButtonLeft | ButtonRight | ButtonMiddle ) )
	{
		struct
		{
			int mask;
			CGEventType type;
			CGMouseButton button;
		} const ups[] = {
			{ ButtonLeft, kCGEventLeftMouseUp, kCGMouseButtonLeft },
			{ ButtonRight, kCGEventRightMouseUp, kCGMouseButtonRight },
			{ ButtonMiddle, kCGEventOtherMouseUp, kCGMouseButtonCenter },
		};
		for( const auto& up : ups )
		{
			if( ( d->buttonMask & up.mask ) == 0 )
			{
				continue;
			}
			if( auto event = CGEventCreateMouseEvent( d->source, up.type, d->position, up.button ) )
			{
				CGEventPost( kCGHIDEventTap, event );
				CFRelease( event );
			}
		}
	}
	d->buttonMask = 0;
}



void MacOsInputInjector::updateKeyboardLayout()
{
	d->layoutUpdated = std::chrono::steady_clock::now();

	// the Text Input Sources API must only be used on the main thread - the table is used as soon as it is ready
	dispatch_async_f( dispatch_get_main_queue(), new std::shared_ptr<CharacterTable>( d->characterTable ),
					  []( void* context ) {
		const std::unique_ptr<std::shared_ptr<CharacterTable>> characterTable(
			static_cast<std::shared_ptr<CharacterTable>*>( context ) );

		std::unordered_map<uint16_t, KeyStroke> table;

		auto inputSource = TISCopyCurrentKeyboardLayoutInputSource();
		if( inputSource == nullptr )
		{
			return;
		}

		auto layoutData = static_cast<CFDataRef>( TISGetInputSourceProperty( inputSource,
																			   kTISPropertyUnicodeKeyLayoutData ) );
		if( layoutData )
		{
			const auto layout = reinterpret_cast<const UCKeyboardLayout*>( CFDataGetBytePtr( layoutData ) );

			struct
			{
				UInt32 modifierKeyState;
				uint64_t flags;
			} const states[] = {
				{ 0, 0 },
				{ ( shiftKey >> 8 ) & 0xff, kCGEventFlagMaskShift },
				{ ( optionKey >> 8 ) & 0xff, kCGEventFlagMaskAlternate },
				{ ( ( shiftKey | optionKey ) >> 8 ) & 0xff, kCGEventFlagMaskShift | kCGEventFlagMaskAlternate },
			};

			for( const auto& state : states )
			{
				for( uint16_t keyCode = 0; keyCode < 128; ++keyCode )
				{
					UInt32 deadKeyState = 0;
					UniChar characters[4]{};
					UniCharCount length = 0;
					if( UCKeyTranslate( layout, keyCode, kUCKeyActionDown, state.modifierKeyState, LMGetKbdType(),
										kUCKeyTranslateNoDeadKeysBit, &deadKeyState, 4, &length,
										characters ) == noErr &&
						length == 1 && characters[0] >= 0x20 && table.count( characters[0] ) == 0 )
					{
						table[characters[0]] = KeyStroke{ keyCode, state.flags, 0 };
					}
				}
			}
		}

		CFRelease( inputSource );

		std::lock_guard<std::mutex> lock( ( *characterTable )->mutex );
		( *characterTable )->strokes = std::move( table );
	} );
}



void MacOsInputInjector::postKey( uint16_t keyCode, bool down, uint64_t flags, uint16_t character )
{
	auto event = CGEventCreateKeyboardEvent( d->source, keyCode, down );
	if( event == nullptr )
	{
		return;
	}

	if( character != 0 )
	{
		CGEventKeyboardSetUnicodeString( event, 1, &character );
	}
	CGEventSetFlags( event, CGEventFlags( flags ) );
	CGEventPost( kCGHIDEventTap, event );
	CFRelease( event );
}



uint64_t MacOsInputInjector::modifierFlags() const
{
	uint64_t flags = 0;
	for( const auto& [keySym, flag] : d->heldModifiers )
	{
		flags |= flag;
	}
	return flags;
}
