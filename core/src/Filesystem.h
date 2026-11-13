/*
 * Filesystem.h - filesystem related query and manipulation functions
 *
 * Copyright (c) 2017-2026 Tobias Junghans <tobydox@veyon.io>
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

#include <QStandardPaths>

#include "VeyonCore.h"

// clazy:excludeall=rule-of-three

// clazy:excludeall=ctor-missing-parent-argument
class VEYON_CORE_EXPORT Filesystem : public QObject
{
	Q_OBJECT
public:
	QString expandPath( QString path ) const;
	QString shrinkPath( QString path ) const;

	//! \brief Returns a (rich text) description of the placeholders supported by expandPath(), e.g. for tooltips
	static QString pathPlaceholdersDescription();
	bool ensurePathExists( const QString &path ) const;

	//! \brief Home directory of the user working with the program. Differs from QDir::homePath() only while
	//! Veyon Configurator runs with administrative privileges (as root) on macOS and Linux, where files are
	//! still browsed, imported and exported on behalf of the user who started it.
	QString userHomePath() const;

	//! \brief Standard location (e.g. Documents) of the user working with the program (see userHomePath())
	QString userStandardLocation( QStandardPaths::StandardLocation location ) const;

	//! \brief Hands a file written on behalf of the user (e.g. an exported key) over to them - otherwise it would
	//! belong to root while Veyon Configurator runs with administrative privileges
	void transferOwnershipToUser( const QString& filePath ) const;

	QString privateKeyPath( const QString& name ) const;
	QString publicKeyPath( const QString& name ) const;

	QString screenshotDirectoryPath() const;

	//! \brief Path of a Veyon program (e.g. "veyon-configurator") - the executable suffix is added if required
	QString programFilePath( const QString& name ) const;
	QString serverFilePath() const;
	QString workerFilePath() const;
	QString cliFilePath() const;

	QString addOnsDirectoryPath() const;
	QString userAddOnsDirectoryPath() const;

Q_SIGNALS:
	void screenshotDirectoryModified();

};
