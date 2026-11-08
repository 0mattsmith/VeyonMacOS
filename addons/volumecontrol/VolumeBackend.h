/*
 * VolumeBackend.h - platform specific access to the system's output volume
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

#include <QString>

/*!
 * \brief Controls the volume of the default audio output device. Implemented per platform in
 * VolumeBackendWindows.cpp (Core Audio), VolumeBackendMacOS.cpp (CoreAudio HAL) and
 * VolumeBackendLinux.cpp (PulseAudio/PipeWire via pactl).
 */
class VolumeBackend
{
public:
	struct State
	{
		bool available{false};
		int level{0};		// 0..100
		bool muted{false};
	};

	VolumeBackend();
	~VolumeBackend();

	State state();
	bool setLevel( int level );
	bool setMuted( bool muted );

private:
	bool m_initialized{false};

};
