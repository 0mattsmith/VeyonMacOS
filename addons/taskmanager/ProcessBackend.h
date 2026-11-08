/*
 * ProcessBackend.h - platform specific process enumeration and control
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
#include <QHash>

#include "ProcessInfo.h"

/*!
 * \brief Abstraction of process management functions - implemented per platform in
 * ProcessBackendWindows.cpp, ProcessBackendMacOS.cpp and ProcessBackendLinux.cpp
 */
class ProcessBackend
{
public:
	enum class Detail
	{
		Basic,		// PID, name, path, user process flag - sufficient for enforcing rules
		Full,		// additionally memory, CPU usage, user name and window titles
	};

	ProcessBackend();
	~ProcessBackend();

	ProcessInfoList processes( Detail detail );

	//! \brief Terminates a process - if force is not set, the process is only asked to quit (non-blocking)
	bool terminate( qint64 pid, bool force );

	bool isRunning( qint64 pid ) const;

	//! \brief Returns whether a process must never be terminated (system and Veyon processes)
	bool isCriticalProcess( const ProcessInfo& process ) const;

	//! \brief Returns whether a process belongs to the operating system and thus is always allowed by rules
	bool isSystemProcess( const ProcessInfo& process ) const;

	static QString platform();

private:
	// CPU usage is calculated from the CPU time consumed between two calls of processes()
	void beginCpuSampling();
	double cpuUsage( qint64 pid, qint64 cpuTimeNanoseconds );
	void endCpuSampling();

	QHash<qint64, qint64> m_previousCpuTimes;
	QHash<qint64, qint64> m_currentCpuTimes;
	QElapsedTimer m_sampleTimer;
	qint64 m_lastSampleTime{0};
	qint64 m_wallTimeDelta{0};
	int m_cpuCount{1};

};
