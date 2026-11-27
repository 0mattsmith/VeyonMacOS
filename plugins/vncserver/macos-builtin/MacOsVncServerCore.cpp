/*
 * MacOsVncServerCore.cpp - VNC server for the current macOS session based on ScreenCaptureKit and LibVNCServer
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
#include <rfb/rfb.h>
}

#include <Accelerate/Accelerate.h>
#include <CommonCrypto/CommonCryptor.h>
#include <arpa/inet.h>
#include <mach-o/dyld.h>
#include <sys/stat.h>

#include <chrono>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "MacOsInputInjector.h"
#include "MacOsScreenCapture.h"
#include "MacOsVncServerCore.h"


namespace {

using Clock = std::chrono::steady_clock;

constexpr auto CaptureRetryInterval = std::chrono::seconds( 5 );
constexpr auto DisplayCheckInterval = std::chrono::seconds( 2 );
constexpr int EventWaitMicroseconds = 10000;

struct ServerState
{
	MacOsInputInjector injector;
	int clientCount{0};
	std::string password;
};

uint8_t reverseBits( uint8_t byte )
{
	uint8_t result = 0;
	for( int i = 0; i < 8; ++i )
	{
		result = uint8_t( ( result << 1 ) | ( ( byte >> i ) & 1 ) );
	}
	return result;
}

// VNC authentication encrypts the challenge with DES using the password (bits of each byte reversed) as key.
// LibVNCServer does this with OpenSSL, whose current versions (as bundled in Veyon.app) no longer provide DES by
// default, so every password - even the correct one - would be rejected. CommonCrypto still provides DES.
rfbBool checkPassword( rfbClientPtr client, const char* response, int length )
{
	auto state = static_cast<ServerState*>( client->screen->screenData );
	if( length != CHALLENGESIZE || state->password.empty() )
	{
		return false;
	}

	uint8_t key[kCCKeySizeDES]{};
	for( size_t i = 0; i < sizeof(key) && i < state->password.size(); ++i )
	{
		key[i] = reverseBits( uint8_t( state->password[i] ) );
	}

	uint8_t expected[CHALLENGESIZE]{};
	size_t written = 0;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	const auto status = CCCrypt( kCCEncrypt, kCCAlgorithmDES, kCCOptionECBMode, key, sizeof(key), nullptr,
								 client->authChallenge, CHALLENGESIZE, expected, sizeof(expected), &written );
#pragma clang diagnostic pop

	return status == kCCSuccess && written == CHALLENGESIZE &&
		   timingsafe_bcmp( expected, response, CHALLENGESIZE ) == 0;
}

void onPointerEvent( int buttonMask, int x, int y, rfbClientPtr client )
{
	auto state = static_cast<ServerState*>( client->screen->screenData );
	state->injector.pointerEvent( buttonMask, x, y, client->screen->width, client->screen->height );
	rfbDefaultPtrAddEvent( buttonMask, x, y, client );
}

void onKeyEvent( rfbBool down, rfbKeySym keySym, rfbClientPtr client )
{
	auto state = static_cast<ServerState*>( client->screen->screenData );
	state->injector.keyEvent( down, uint32_t( keySym ) );
}

void onClientGone( rfbClientPtr client )
{
	auto state = static_cast<ServerState*>( client->screen->screenData );
	if( --state->clientCount <= 0 )
	{
		state->clientCount = 0;
		// avoid stuck keys or buttons when a client disconnects in the middle of a key press
		state->injector.releaseAll();
	}
}

rfbNewClientAction onNewClient( rfbClientPtr client )
{
	auto state = static_cast<ServerState*>( client->screen->screenData );
	++state->clientCount;
	client->clientGoneHook = onClientGone;
	return RFB_CLIENT_ACCEPT;
}

// identifies the installed build of the running program (modification time and size of the executable)
std::string executableStamp()
{
	char path[PATH_MAX]{};
	uint32_t size = sizeof(path);
	struct stat info{};
	if( _NSGetExecutablePath( path, &size ) != 0 || stat( path, &info ) != 0 )
	{
		return {};
	}
	return std::to_string( info.st_mtimespec.tv_sec ) + ":" + std::to_string( info.st_size );
}

std::string readFirstLine( const std::string& fileName )
{
	std::ifstream file( fileName );
	std::string line;
	std::getline( file, line );
	return line;
}

// ScreenCaptureKit delivers BGRA, LibVNCServer's default format on little endian machines is RGBX
void copyRect( const uint8_t* source, int sourceBytesPerRow, uint8_t* target, int targetBytesPerRow,
			   const MacOsScreenCapture::Rect& rect )
{
	vImage_Buffer sourceBuffer{
		const_cast<uint8_t*>( source ) + size_t(rect.y) * size_t(sourceBytesPerRow) + size_t(rect.x) * 4,
		vImagePixelCount( rect.height ), vImagePixelCount( rect.width ), size_t(sourceBytesPerRow) };
	vImage_Buffer targetBuffer{
		target + size_t(rect.y) * size_t(targetBytesPerRow) + size_t(rect.x) * 4,
		vImagePixelCount( rect.height ), vImagePixelCount( rect.width ), size_t(targetBytesPerRow) };
	const uint8_t permuteMap[4] = { 2, 1, 0, 3 };
	vImagePermuteChannels_ARGB8888( &sourceBuffer, &targetBuffer, permuteMap, kvImageNoFlags );
}

}



bool MacOsVncServerCore::run( const Options& options )
{
	const auto log = [&options]( LogLevel level, const std::string& message ) {
		if( options.log )
		{
			options.log( level, message );
		}
	};

	int width = 0;
	int height = 0;
	MacOsScreenCapture::captureSize( &width, &height );
	if( width <= 0 || height <= 0 )
	{
		width = 1024;
		height = 768;
	}

	auto screen = rfbGetScreen( nullptr, nullptr, width, height, 8, 3, 4 );
	if( screen == nullptr )
	{
		log( LogLevel::Critical, "could not create the VNC server" );
		return false;
	}

	ServerState state;
	state.password = options.password;

	if( options.password.empty() == false )
	{
		// LibVNCServer only offers VNC authentication if authPasswdData is set - checkPassword() doesn't use it
		screen->authPasswdData = &state.password;
		screen->passwordCheck = checkPassword;
	}

	screen->desktopName = "VeyonVNC";
	screen->frameBuffer = static_cast<char*>( calloc( size_t(width) * size_t(height), 4 ) );
	screen->port = options.port;
	screen->ipv6port = options.port;
	if( options.localhostOnly )
	{
		screen->listenInterface = htonl( INADDR_LOOPBACK );
		screen->listen6Interface = const_cast<char*>( "::1" );
	}
	screen->alwaysShared = true;
	screen->handleEventsEagerly = true;
	screen->deferUpdateTime = 5;
	screen->cursor = nullptr; // the cursor is part of the captured frames
	screen->screenData = &state;

	rfbInitServer( screen );

	// set after rfbInitServer() which installs the defaults
	screen->ptrAddEvent = onPointerEvent;
	screen->kbdAddEvent = onKeyEvent;
	screen->newClientHook = onNewClient;

	// the system prompts for missing permissions are shown once per installed build only - not on every start of the
	// Veyon Service, which would otherwise ask again and again while a permission is denied
	const auto buildStamp = executableStamp();
	bool mayPrompt = options.permissionPromptStateFile.empty() ||
					 readFirstLine( options.permissionPromptStateFile ) != buildStamp;
	const auto rememberPrompt = [&]() {
		if( mayPrompt && options.permissionPromptStateFile.empty() == false )
		{
			std::ofstream( options.permissionPromptStateFile, std::ios::trunc ) << buildStamp << '\n';
		}
		mayPrompt = false;
	};

	if( MacOsInputInjector::hasPermission() == false )
	{
		log( LogLevel::Warning, "the Accessibility permission is missing - remote control will not work until it is "
								"granted to Veyon in System Settings > Privacy & Security > Accessibility" );
		if( mayPrompt )
		{
			MacOsInputInjector::requestPermission();
		}
	}

	MacOsScreenCapture capture;
	auto nextCaptureStart = Clock::now();
	auto nextDisplayCheck = Clock::now() + DisplayCheckInterval;
	std::string reportedError;
	bool screenRecordingPermissionReported = false;

	m_stopRequested = false;

	while( m_stopRequested == false )
	{
		const auto now = Clock::now();

		if( capture.isRunning() == false && now >= nextCaptureStart )
		{
			if( MacOsScreenCapture::hasPermission() == false && screenRecordingPermissionReported == false )
			{
				log( LogLevel::Warning, "the Screen Recording permission is missing - please grant it to Veyon in System "
										"Settings > Privacy & Security > Screen & System Audio Recording" );
				if( mayPrompt )
				{
					MacOsScreenCapture::requestPermission();
				}
				screenRecordingPermissionReported = true;
			}
			rememberPrompt();

			if( capture.start( options.framesPerSecond ) )
			{
				log( LogLevel::Info, "screen capture started" );
				reportedError.clear();
			}
			else
			{
				const auto error = capture.lastError();
				if( error != reportedError )
				{
					log( LogLevel::Warning, error );
					reportedError = error;
				}
				nextCaptureStart = now + CaptureRetryInterval;
			}
		}

		if( capture.isRunning() && now >= nextDisplayCheck )
		{
			// the capture size is fixed when the stream starts, so restart it after a resolution change
			int displayWidth = 0;
			int displayHeight = 0;
			MacOsScreenCapture::captureSize( &displayWidth, &displayHeight );
			if( displayWidth > 0 && displayHeight > 0 &&
				( displayWidth != screen->width || displayHeight != screen->height ) )
			{
				log( LogLevel::Info, "display size changed - restarting the screen capture" );
				capture.stop();
				nextCaptureStart = now;
			}
			nextDisplayCheck = now + DisplayCheckInterval;
		}

		rfbProcessEvents( screen, EventWaitMicroseconds );

		capture.consumeFrame( [screen]( const uint8_t* pixels, int frameWidth, int frameHeight, int bytesPerRow,
										const std::vector<MacOsScreenCapture::Rect>& dirtyRects ) {
			if( frameWidth != screen->width || frameHeight != screen->height )
			{
				auto oldFramebuffer = screen->frameBuffer;
				auto newFramebuffer = static_cast<char*>( calloc( size_t(frameWidth) * size_t(frameHeight), 4 ) );
				rfbNewFramebuffer( screen, newFramebuffer, frameWidth, frameHeight, 8, 3, 4 );
				free( oldFramebuffer );
			}

			for( const auto& rect : dirtyRects )
			{
				copyRect( pixels, bytesPerRow, reinterpret_cast<uint8_t*>( screen->frameBuffer ), screen->width * 4,
						  rect );
				rfbMarkRectAsModified( screen, rect.x, rect.y, rect.x + rect.width, rect.y + rect.height );
			}
		} );
	}

	capture.stop();
	state.injector.releaseAll();

	rfbShutdownServer( screen, true );
	free( screen->frameBuffer );
	screen->frameBuffer = nullptr;
	rfbScreenCleanup( screen );

	return true;
}
