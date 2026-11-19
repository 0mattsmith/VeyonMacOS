/*
 * MacOsVncServerCore.h - VNC server for the current macOS session based on ScreenCaptureKit and LibVNCServer
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

#include <atomic>
#include <functional>
#include <string>

// Qt-free on purpose so that the whole server can be built and tried without Veyon (see README.md)
class MacOsVncServerCore
{
public:
	enum class LogLevel
	{
		Debug,
		Info,
		Warning,
		Critical
	};

	using Logger = std::function<void( LogLevel level, const std::string& message )>;

	struct Options
	{
		int port{0};
		std::string password; // empty: no authentication (for testing only)
		bool localhostOnly{true};
		int framesPerSecond{30};
		Logger log;
	};

	// blocks until stop() is called from another thread
	bool run( const Options& options );

	void stop()
	{
		m_stopRequested = true;
	}

private:
	std::atomic<bool> m_stopRequested{false};

};
