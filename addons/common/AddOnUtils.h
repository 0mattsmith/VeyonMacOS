/*
 * AddOnUtils.h - common helper functions for add-ons
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

#include <QProcessEnvironment>
#include <QStringList>

#include "ComputerControlInterface.h"

namespace AddOnUtils
{

struct ProcessResult
{
	int exitCode{-1};
	QByteArray output;
	bool timedOut{false};
	bool started{false};
};

//! \brief Runs a program and returns its exit code and combined stdout/stderr output
ProcessResult runProcess( const QString& program, const QStringList& arguments, int timeout = 30000,
						  const QByteArray& standardInput = {}, const QProcessEnvironment& environment = {} );

bool isServer();
bool isWorker();
bool isMaster();

QString computerDisplayName( const ComputerControlInterface::Pointer& computerControlInterface );

//! \brief Returns a key identifying a computer which can be used in hashes/maps
QString computerKey( const ComputerControlInterface::Pointer& computerControlInterface );

ComputerControlInterface::Pointer findComputer( const ComputerControlInterfaceList& interfaces, const QString& key );

ComputerControlInterfaceList connectedInterfaces( const ComputerControlInterfaceList& interfaces );

QString formatFileSize( qint64 size );

//! \brief Checks whether a name matches any of the given wildcard patterns (case insensitive)
bool matchesAnyPattern( const QString& name, const QStringList& patterns );

//! \brief Splits user input (one entry per line or comma separated) into a clean list
QStringList splitList( const QString& text );

//! \brief Returns the time the computer has been booted (seconds since epoch)
qint64 systemBootTime();

//! \brief Extracts the host name from user input like "https://www.example.com/path" or "example.com"
QString hostFromUrlOrDomain( const QString& input );

//! \brief Returns the login name of the user logged on at the computer
QString sessionUserName();

}
