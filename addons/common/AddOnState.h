/*
 * AddOnState.h - persistent state storage for add-ons
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

#include <QJsonObject>

/*!
 * \brief Stores state of add-ons on student computers (e.g. active application or website rules)
 * so it survives restarts of the Veyon Service. State files are only accessible by administrators.
 */
class AddOnState
{
public:
	static QJsonObject load( const QString& name );
	static bool save( const QString& name, const QJsonObject& state );
	static void remove( const QString& name );

	/*!
	 * \brief Saves state which is restored whenever the Veyon Server restarts (e.g. after the student logged
	 * off and on again). Unless keepAfterReboot is set, the state is discarded after the computer restarted.
	 */
	static bool saveRuntimeState( const QString& name, const QJsonObject& state, bool keepAfterReboot );
	static QJsonObject loadRuntimeState( const QString& name );

	static QString stateDirectory();

private:
	static constexpr auto BootTimeTolerance = 120;

	static QString stateFilePath( const QString& name );

};
