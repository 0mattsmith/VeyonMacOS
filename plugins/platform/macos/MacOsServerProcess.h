/*
 * MacOsServerProcess.h - declaration of MacOsServerProcess class
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

#include <QProcess>

#include "MacOsSessionFunctions.h"

// clazy:excludeall=copyable-polymorphic

//! \brief Runs veyon-server (as root) inside the GUI bootstrap namespace of the console user
class MacOsServerProcess : public QProcess
{
	Q_OBJECT
public:
	MacOsServerProcess( const MacOsSessionFunctions::ConsoleUser& user,
						const QProcessEnvironment& processEnvironment,
						QObject* parent = nullptr );
	~MacOsServerProcess() override;

	void start();
	void stop();

private:
	static constexpr auto ServerTerminateTimeout = 3000;
	static constexpr auto ServerKillTimeout = 2000;

	void signalProcessTree( int signal );

	const MacOsSessionFunctions::ConsoleUser m_user;

};
