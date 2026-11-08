/*
 * VolumeBackendMacOS.cpp - volume control for macOS using the CoreAudio HAL
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

#include <QtGlobal>

#include <AudioToolbox/AudioServices.h>
#include <CoreAudio/CoreAudio.h>

#include "VolumeBackend.h"


namespace {

AudioObjectID defaultOutputDevice()
{
	AudioObjectPropertyAddress address{ kAudioHardwarePropertyDefaultOutputDevice,
										kAudioObjectPropertyScopeGlobal,
										kAudioObjectPropertyElementMain };

	AudioObjectID device = kAudioObjectUnknown;
	UInt32 size = sizeof(device);
	if( AudioObjectGetPropertyData( kAudioObjectSystemObject, &address, 0, nullptr, &size, &device ) != noErr )
	{
		return kAudioObjectUnknown;
	}

	return device;
}

AudioObjectPropertyAddress volumeAddress()
{
	return { kAudioHardwareServiceDeviceProperty_VirtualMainVolume,
			 kAudioDevicePropertyScopeOutput,
			 kAudioObjectPropertyElementMain };
}

AudioObjectPropertyAddress muteAddress()
{
	return { kAudioDevicePropertyMute,
			 kAudioDevicePropertyScopeOutput,
			 kAudioObjectPropertyElementMain };
}

}



VolumeBackend::VolumeBackend() = default;



VolumeBackend::~VolumeBackend() = default;



VolumeBackend::State VolumeBackend::state()
{
	State state;

	const auto device = defaultOutputDevice();
	if( device == kAudioObjectUnknown )
	{
		return state;
	}

	auto address = volumeAddress();
	if( AudioObjectHasProperty( device, &address ) )
	{
		Float32 level = 0;
		UInt32 size = sizeof(level);
		if( AudioObjectGetPropertyData( device, &address, 0, nullptr, &size, &level ) == noErr )
		{
			state.available = true;
			state.level = qRound( level * 100 );
		}
	}

	address = muteAddress();
	if( AudioObjectHasProperty( device, &address ) )
	{
		UInt32 muted = 0;
		UInt32 size = sizeof(muted);
		if( AudioObjectGetPropertyData( device, &address, 0, nullptr, &size, &muted ) == noErr )
		{
			state.available = true;
			state.muted = muted != 0;
		}
	}

	return state;
}



bool VolumeBackend::setLevel( int level )
{
	const auto device = defaultOutputDevice();
	auto address = volumeAddress();
	if( device == kAudioObjectUnknown || AudioObjectHasProperty( device, &address ) == false )
	{
		return false;
	}

	Float32 value = Float32( qBound( 0, level, 100 ) ) / 100.0f;
	return AudioObjectSetPropertyData( device, &address, 0, nullptr, sizeof(value), &value ) == noErr;
}



bool VolumeBackend::setMuted( bool muted )
{
	const auto device = defaultOutputDevice();
	auto address = muteAddress();
	if( device == kAudioObjectUnknown || AudioObjectHasProperty( device, &address ) == false )
	{
		return false;
	}

	UInt32 value = muted ? 1 : 0;
	return AudioObjectSetPropertyData( device, &address, 0, nullptr, sizeof(value), &value ) == noErr;
}
