/*
 * MacOsCoreFunctions.h - declaration of MacOsCoreFunctions class
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

#include "PlatformCoreFunctions.h"

// clazy:excludeall=copyable-polymorphic

class MacOsCoreFunctions : public PlatformCoreFunctions
{
public:
	MacOsCoreFunctions() = default;
	~MacOsCoreFunctions() override;

	bool applyConfiguration() override;

	bool prepareSessionBusAccess() override;

	void initNativeLoggingSystem( const QString& appName ) override;
	void writeToNativeLoggingSystem( const QString& message, Logger::LogLevel loglevel ) override;

	StandardInputReader* createStandardInputReader( QObject* parent ) override;

	void reboot() override;
	void powerDown( bool installUpdates ) override;

	void raiseWindow( QWidget* widget, bool stayOnTop ) override;

	void disableScreenSaver() override;
	void restoreScreenSaverSettings() override;

	void setSystemUiState( bool enabled ) override;

	QString activeDesktopName() override;

	bool isRunningAsAdmin() const override;
	bool runProgramAsAdmin( const QString& program, const QStringList& parameters ) override;

	bool runProgramAsUser( const QString& program, const QStringList& parameters,
						   const QString& username,
						   const QString& desktop,
						   const QByteArray& stdInData ) override;

	QString genericUrlHandler() const override;

	QString queryDisplayDeviceName( const QScreen& screen ) const override;

	QString getApplicationName( ProcessId processId ) const override;

	//! \brief Quotes a string for use in a POSIX shell command line
	static QString shellQuote( const QString& argument );

	//! \brief Escapes a string for use inside an AppleScript string literal
	static QString appleScriptEscape( const QString& text );

private:
	static constexpr auto ShutdownCommand = "/sbin/shutdown";

	uint32_t m_powerAssertionId{0};
	bool m_powerAssertionActive{false};

	QByteArray m_syslogIdentity;

};
