/*
 * MacOsScreenCapture.h - captures the main display of the current session with ScreenCaptureKit
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
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Qt-free on purpose: the implementation is Objective-C++ (ScreenCaptureKit has no C API) and this header is
// included from plain C++ code
class MacOsScreenCapture
{
public:
	struct Rect
	{
		int x;
		int y;
		int width;
		int height;
	};

	// called with the capture buffer locked - pixels are BGRA (as delivered by ScreenCaptureKit), dirtyRects
	// lists the areas changed since the previous call (the whole frame after a size change or the first frame)
	using FrameConsumer = std::function<void( const uint8_t* pixels, int width, int height, int bytesPerRow,
											   const std::vector<Rect>& dirtyRects )>;

	MacOsScreenCapture();
	~MacOsScreenCapture();

	MacOsScreenCapture( const MacOsScreenCapture& ) = delete;
	MacOsScreenCapture& operator=( const MacOsScreenCapture& ) = delete;

	// Screen Recording permission (Privacy & Security) of this program
	static bool hasPermission();
	// shows the system prompt (once per program) and adds the program to the list in System Settings
	static void requestPermission();

	// size of the main display in points (the capture size)
	static void mainDisplaySize( int* width, int* height );

	// starts capturing the main display (blocks until started or failed)
	bool start( int framesPerSecond );
	void stop();

	// false after the stream has been stopped by the system (e.g. the display configuration changed)
	bool isRunning() const;

	std::string lastError() const;

	// calls consumer if a new frame has arrived since the last call and returns true in that case
	bool consumeFrame( const FrameConsumer& consumer );

	// implementation detail - public only so that the Objective-C stream output class can refer to it
	struct Private;

private:
	std::unique_ptr<Private> d;

};
