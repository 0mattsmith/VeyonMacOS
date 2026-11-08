/*
 * VolumeBackendWindows.cpp - volume control for Windows using the Core Audio API
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

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

#include "VolumeBackend.h"


namespace {

template<class T>
void safeRelease( T*& object )
{
	if( object )
	{
		object->Release();
		object = nullptr;
	}
}

// the default device may change at any time (e.g. headphones plugged in) so it's resolved for every operation
IAudioEndpointVolume* defaultEndpointVolume()
{
	IMMDeviceEnumerator* enumerator = nullptr;
	if( FAILED( CoCreateInstance( __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
								  __uuidof(IMMDeviceEnumerator), reinterpret_cast<void **>( &enumerator ) ) ) )
	{
		return nullptr;
	}

	IMMDevice* device = nullptr;
	if( FAILED( enumerator->GetDefaultAudioEndpoint( eRender, eConsole, &device ) ) )
	{
		safeRelease( enumerator );
		return nullptr;
	}

	IAudioEndpointVolume* endpointVolume = nullptr;
	if( FAILED( device->Activate( __uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
								  reinterpret_cast<void **>( &endpointVolume ) ) ) )
	{
		endpointVolume = nullptr;
	}

	safeRelease( device );
	safeRelease( enumerator );

	return endpointVolume;
}

}



VolumeBackend::VolumeBackend()
{
	// Qt might already have initialized COM for this thread - then this just increments the reference count
	const auto result = CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED );
	m_initialized = result == S_OK || result == S_FALSE;
}



VolumeBackend::~VolumeBackend()
{
	if( m_initialized )
	{
		CoUninitialize();
	}
}



VolumeBackend::State VolumeBackend::state()
{
	State state;

	auto endpointVolume = defaultEndpointVolume();
	if( endpointVolume == nullptr )
	{
		return state;
	}

	float level = 0;
	BOOL muted = FALSE;
	if( SUCCEEDED( endpointVolume->GetMasterVolumeLevelScalar( &level ) ) &&
		SUCCEEDED( endpointVolume->GetMute( &muted ) ) )
	{
		state.available = true;
		state.level = qRound( level * 100 );
		state.muted = muted;
	}

	safeRelease( endpointVolume );

	return state;
}



bool VolumeBackend::setLevel( int level )
{
	auto endpointVolume = defaultEndpointVolume();
	if( endpointVolume == nullptr )
	{
		return false;
	}

	const auto result = SUCCEEDED( endpointVolume->SetMasterVolumeLevelScalar( float( qBound( 0, level, 100 ) ) / 100.0f, nullptr ) );
	safeRelease( endpointVolume );

	return result;
}



bool VolumeBackend::setMuted( bool muted )
{
	auto endpointVolume = defaultEndpointVolume();
	if( endpointVolume == nullptr )
	{
		return false;
	}

	const auto result = SUCCEEDED( endpointVolume->SetMute( muted ? TRUE : FALSE, nullptr ) );
	safeRelease( endpointVolume );

	return result;
}
