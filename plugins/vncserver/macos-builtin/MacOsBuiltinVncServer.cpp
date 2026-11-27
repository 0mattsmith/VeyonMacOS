/*
 * MacOsBuiltinVncServer.cpp - implementation of MacOsBuiltinVncServer class
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

extern "C" {
#include "rfb/rfb.h"
}

#include <array>
#include <cstdarg>
#include <cstdio>

#include "MacOsBuiltinVncServer.h"
#include "MacOsVncServerCore.h"
#include "PlatformFilesystemFunctions.h"
#include "PlatformPluginInterface.h"
#include "VeyonCore.h"


MacOsBuiltinVncServer::MacOsBuiltinVncServer( QObject* parent ) :
	QObject( parent )
{
}



void MacOsBuiltinVncServer::prepareServer()
{
}



bool MacOsBuiltinVncServer::runServer( int serverPort, const Password& password )
{
	if( VeyonCore::isDebugging() )
	{
		rfbLog = rfbLogDebug;
		rfbErr = rfbLogDebug;
	}
	else
	{
		rfbLog = rfbLogNone;
		rfbErr = rfbLogNone;
	}

	MacOsVncServerCore::Options options;
	options.port = serverPort;
	options.password = password.toByteArray().toStdString();
	options.framesPerSecond = FramesPerSecond;
	options.permissionPromptStateFile = QString( VeyonCore::platform().filesystemFunctions().globalAppDataPath() +
												 QStringLiteral("/PermissionPromptState") ).toStdString();
	options.log = []( MacOsVncServerCore::LogLevel level, const std::string& message ) {
		const auto text = QString::fromStdString( message );
		switch( level )
		{
		case MacOsVncServerCore::LogLevel::Debug: vDebug() << text; break;
		case MacOsVncServerCore::LogLevel::Info: vInfo() << text; break;
		case MacOsVncServerCore::LogLevel::Warning: vWarning() << text; break;
		case MacOsVncServerCore::LogLevel::Critical: vCritical() << text; break;
		}
	};

	MacOsVncServerCore server;
	return server.run( options );
}



void MacOsBuiltinVncServer::rfbLogDebug( const char* format, ... )
{
	va_list args;
	va_start( args, format );

	static constexpr int MaxMessageLength = 256;
	std::array<char, MaxMessageLength> message;

	std::vsnprintf( message.data(), message.size(), format, args ); // Flawfinder: ignore

	va_end( args );

	vDebug() << message.data();
}



void MacOsBuiltinVncServer::rfbLogNone( const char* format, ... )
{
	Q_UNUSED(format);
}
