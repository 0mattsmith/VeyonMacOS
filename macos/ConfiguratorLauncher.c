/*
 * ConfiguratorLauncher.c - starts Veyon Configurator from Veyon.app
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

/*
 * All Veyon programs share the libraries and plugins in Veyon.app. "Veyon Configurator.app" therefore only
 * contains this launcher, which makes the Configurator available in Launchpad and Finder with its own icon.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/dyld.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char* const ConfiguratorPath = "Veyon.app/Contents/MacOS/veyon-configurator";
static const char* const DefaultApplicationsDirectory = "/Applications";

// Determines the directory containing "Veyon Configurator.app" (usually /Applications)
static int applicationsDirectory( char* directory, size_t size )
{
	char executablePath[PATH_MAX];
	uint32_t executablePathSize = sizeof(executablePath);
	if( _NSGetExecutablePath( executablePath, &executablePathSize ) != 0 ||
		realpath( executablePath, directory ) == NULL ||
		strlen( directory ) >= size )
	{
		return 0;
	}

	// strip "/Veyon Configurator.app/Contents/MacOS/veyon-configurator-launcher"
	for( int i = 0; i < 4; ++i )
	{
		char* separator = strrchr( directory, '/' );
		if( separator == NULL )
		{
			return 0;
		}
		*separator = '\0';
	}

	return 1;
}

static void showError( const char* message )
{
	CFStringRef text = CFStringCreateWithCString( kCFAllocatorDefault, message, kCFStringEncodingUTF8 );
	CFUserNotificationDisplayAlert( 0, kCFUserNotificationStopAlertLevel, NULL, NULL, NULL,
									CFSTR("Veyon Configurator"), text, NULL, NULL, NULL, NULL );
	if( text )
	{
		CFRelease( text );
	}
}

int main( int argc, char** argv )
{
	(void) argc;

	char directory[PATH_MAX];
	char configurator[PATH_MAX];

	// prefer the Veyon.app next to this application, e.g. if both have been copied to another folder
	if( applicationsDirectory( directory, sizeof(directory) ) &&
		snprintf( configurator, sizeof(configurator), "%s/%s", directory, ConfiguratorPath ) < (int) sizeof(configurator) &&
		access( configurator, X_OK ) == 0 )
	{
		argv[0] = configurator;
		execv( configurator, argv );
	}

	snprintf( configurator, sizeof(configurator), "%s/%s", DefaultApplicationsDirectory, ConfiguratorPath );
	if( access( configurator, X_OK ) == 0 )
	{
		argv[0] = configurator;
		execv( configurator, argv );
	}

	perror( "veyon-configurator-launcher" );
	showError( "Veyon Configurator could not be started because Veyon.app was not found. "
			   "Please install Veyon into the Applications folder." );

	return 1;
}
