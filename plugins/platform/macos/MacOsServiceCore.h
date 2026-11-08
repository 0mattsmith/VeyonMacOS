/*
 * MacOsServiceCore.h - declaration of MacOsServiceCore class
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

#include <QElapsedTimer>
#include <QTimer>

#include "MacOsSessionFunctions.h"
#include "PlatformSessionManager.h"
#include "ServiceDataManager.h"

class MacOsServerProcess;

// clazy:excludeall=copyable-polymorphic

//! \brief Keeps a Veyon Server instance running for the user logged in at the console
class MacOsServiceCore : public QObject
{
	Q_OBJECT
public:
	explicit MacOsServiceCore( QObject* parent = nullptr );
	~MacOsServiceCore() override;

	void run();

private:
	static constexpr auto ConsoleUserPollingInterval = 2000;
	static constexpr auto ServerRestartInterval = 5000;

	void checkConsoleUser();
	void startServer();
	void stopServer();

	QProcessEnvironment serverEnvironment() const;

	ServiceDataManager m_dataManager{};
	PlatformSessionManager m_sessionManager{};

	MacOsServerProcess* m_serverProcess{nullptr};
	MacOsSessionFunctions::ConsoleUser m_consoleUser{};

	QTimer m_consoleUserPollTimer{this};
	QElapsedTimer m_consoleUserChangedTimer{};
	QElapsedTimer m_lastServerStartTimer{};

};
