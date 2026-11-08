/*
 * ProcessBackend.cpp - platform independent parts of ProcessBackend
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

#include <QThread>

#include "ProcessBackend.h"


ProcessBackend::ProcessBackend() :
	m_cpuCount( qMax( 1, QThread::idealThreadCount() ) )
{
	m_sampleTimer.start();
}



ProcessBackend::~ProcessBackend() = default;



void ProcessBackend::beginCpuSampling()
{
	const auto now = m_sampleTimer.nsecsElapsed();
	m_wallTimeDelta = m_lastSampleTime > 0 ? now - m_lastSampleTime : 0;
	m_lastSampleTime = now;
	m_currentCpuTimes.clear();
}



double ProcessBackend::cpuUsage( qint64 pid, qint64 cpuTimeNanoseconds )
{
	m_currentCpuTimes[pid] = cpuTimeNanoseconds;

	const auto previous = m_previousCpuTimes.find( pid );
	if( previous == m_previousCpuTimes.end() || m_wallTimeDelta <= 0 || cpuTimeNanoseconds < *previous )
	{
		return 0;
	}

	const auto usage = 100.0 * double( cpuTimeNanoseconds - *previous ) / double( m_wallTimeDelta ) / m_cpuCount;
	return qBound( 0.0, usage, 100.0 );
}



void ProcessBackend::endCpuSampling()
{
	m_previousCpuTimes = m_currentCpuTimes;
}
