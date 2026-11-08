/*
 * VolumeBackendLinux.cpp - volume control for Linux using PulseAudio/PipeWire (pactl)
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

#include <QRegularExpression>

#include <pwd.h>
#include <unistd.h>

#include "AddOnUtils.h"
#include "PlatformUserFunctions.h"
#include "VolumeBackend.h"


namespace {

// the sound server runs per user so pactl has to be run as the session user
AddOnUtils::ProcessResult pactl( const QStringList& arguments )
{
	const auto pactlPath = QStringLiteral("/usr/bin/pactl");

	if( geteuid() != 0 )
	{
		return AddOnUtils::runProcess( pactlPath, arguments, 5000 );
	}

	const auto userName = VeyonCore::platform().userFunctions().queryCurrentUserProperty( PlatformUserFunctions::UserProperty::LoginName );
	const auto passwordEntry = userName.isEmpty() ? nullptr : getpwnam( userName.toUtf8().constData() );
	if( passwordEntry == nullptr )
	{
		return {};
	}

	return AddOnUtils::runProcess( QStringLiteral("/usr/sbin/runuser"),
								   QStringList{ QStringLiteral("-u"), userName, QStringLiteral("--"),
												QStringLiteral("env"),
												QStringLiteral("XDG_RUNTIME_DIR=/run/user/%1").arg( passwordEntry->pw_uid ),
												pactlPath } + arguments, 5000 );
}

}



VolumeBackend::VolumeBackend() = default;



VolumeBackend::~VolumeBackend() = default;



VolumeBackend::State VolumeBackend::state()
{
	State state;

	const auto volume = pactl( { QStringLiteral("get-sink-volume"), QStringLiteral("@DEFAULT_SINK@") } );
	static const QRegularExpression percentRX( QStringLiteral("(\\d+)%") );
	const auto volumeMatch = percentRX.match( QString::fromUtf8( volume.output ) );
	if( volume.exitCode == 0 && volumeMatch.hasMatch() )
	{
		state.available = true;
		state.level = qBound( 0, volumeMatch.captured( 1 ).toInt(), 100 );
	}

	const auto mute = pactl( { QStringLiteral("get-sink-mute"), QStringLiteral("@DEFAULT_SINK@") } );
	if( mute.exitCode == 0 )
	{
		state.muted = mute.output.contains( "yes" );
	}

	return state;
}



bool VolumeBackend::setLevel( int level )
{
	return pactl( { QStringLiteral("set-sink-volume"), QStringLiteral("@DEFAULT_SINK@"),
					QStringLiteral("%1%").arg( qBound( 0, level, 100 ) ) } ).exitCode == 0;
}



bool VolumeBackend::setMuted( bool muted )
{
	return pactl( { QStringLiteral("set-sink-mute"), QStringLiteral("@DEFAULT_SINK@"),
					muted ? QStringLiteral("1") : QStringLiteral("0") } ).exitCode == 0;
}
