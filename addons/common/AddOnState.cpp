/*
 * AddOnState.cpp - persistent state storage for add-ons
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

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

#include "AddOnState.h"
#include "AddOnUtils.h"
#include "Filesystem.h"
#include "VeyonCore.h"


QJsonObject AddOnState::load( const QString& name )
{
	QFile file( stateFilePath( name ) );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return {};
	}

	return QJsonDocument::fromJson( file.readAll() ).object();
}



bool AddOnState::save( const QString& name, const QJsonObject& state )
{
	const auto directory = stateDirectory();
	if( QDir().mkpath( directory ) == false )
	{
		vWarning() << "could not create state directory" << directory;
		return false;
	}

	QSaveFile file( stateFilePath( name ) );
	if( file.open( QFile::WriteOnly ) == false )
	{
		vWarning() << "could not write state file" << file.fileName();
		return false;
	}

	file.write( QJsonDocument( state ).toJson( QJsonDocument::Compact ) );
	file.setPermissions( QFile::ReadOwner | QFile::WriteOwner );

	return file.commit();
}



void AddOnState::remove( const QString& name )
{
	QFile::remove( stateFilePath( name ) );
}



bool AddOnState::saveRuntimeState( const QString& name, const QJsonObject& state, bool keepAfterReboot )
{
	return save( name, QJsonObject{
					 { QStringLiteral("bootTime"), double( AddOnUtils::systemBootTime() ) },
					 { QStringLiteral("keepAfterReboot"), keepAfterReboot },
					 { QStringLiteral("state"), state },
				 } );
}



QJsonObject AddOnState::loadRuntimeState( const QString& name )
{
	const auto data = load( name );
	if( data.isEmpty() )
	{
		return {};
	}

	const auto savedBootTime = qint64( data[QStringLiteral("bootTime")].toDouble() );
	const auto sameBoot = qAbs( savedBootTime - AddOnUtils::systemBootTime() ) < BootTimeTolerance;

	if( data[QStringLiteral("keepAfterReboot")].toBool() || sameBoot )
	{
		return data[QStringLiteral("state")].toObject();
	}

	remove( name );

	return {};
}



QString AddOnState::stateDirectory()
{
	// directories starting with a dot are ignored when scanning for add-on packages
	return VeyonCore::filesystem().addOnsDirectoryPath() + QDir::separator() + QStringLiteral(".state");
}



QString AddOnState::stateFilePath( const QString& name )
{
	return stateDirectory() + QDir::separator() + name + QStringLiteral(".json");
}
