/*
 * WorkerIdleTimer.h - ends feature workers which have been idle for a while
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

#include <QCoreApplication>
#include <QTimer>

#include "AddOnUtils.h"

/*!
 * \brief A running feature worker marks its feature as active on the teacher's computer tiles. Workers of
 * add-ons which only perform short operations therefore quit after some time without any requests.
 */
class WorkerIdleTimer
{
public:
	explicit WorkerIdleTimer( int timeout ) :
		m_timeout( timeout )
	{
		m_timer.setSingleShot( true );
		QObject::connect( &m_timer, &QTimer::timeout, QCoreApplication::instance(), &QCoreApplication::quit );
	}

	void restart()
	{
		if( AddOnUtils::isWorker() )
		{
			m_timer.start( m_timeout );
		}
	}

	void stop()
	{
		m_timer.stop();
	}

private:
	const int m_timeout;
	QTimer m_timer;

};
