/*
 * FileReceiver.cpp - writes a file received in chunks
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

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include "FileReceiver.h"


FileReceiver::~FileReceiver()
{
	cancel();
}



bool FileReceiver::begin( const QString& targetFilePath, bool overwrite, QString* errorString )
{
	cancel();

	const QFileInfo targetFileInfo( targetFilePath );

	if( targetFileInfo.exists() && overwrite == false )
	{
		*errorString = QCoreApplication::translate( "FileReceiver", "The file %1 already exists." ).arg( targetFileInfo.fileName() );
		return false;
	}

	if( targetFileInfo.isDir() )
	{
		*errorString = QCoreApplication::translate( "FileReceiver", "%1 is a folder." ).arg( targetFileInfo.fileName() );
		return false;
	}

	if( QDir().mkpath( targetFileInfo.absolutePath() ) == false )
	{
		*errorString = QCoreApplication::translate( "FileReceiver", "Could not create folder %1." ).arg( targetFileInfo.absolutePath() );
		return false;
	}

	m_targetFilePath = targetFileInfo.absoluteFilePath();
	m_overwrite = overwrite;
	m_bytesWritten = 0;

	m_file.setFileName( m_targetFilePath + QLatin1String(PartFileSuffix) );
	if( m_file.open( QFile::WriteOnly | QFile::Truncate ) == false )
	{
		*errorString = QCoreApplication::translate( "FileReceiver", "Could not write file %1: %2" ).
					   arg( targetFileInfo.fileName(), m_file.errorString() );
		return false;
	}

	return true;
}



bool FileReceiver::write( const QByteArray& data, QString* errorString )
{
	if( m_file.isOpen() == false )
	{
		*errorString = QCoreApplication::translate( "FileReceiver", "No file transfer in progress." );
		return false;
	}

	if( m_file.write( data ) != data.size() )
	{
		*errorString = QCoreApplication::translate( "FileReceiver", "Could not write file %1: %2" ).
					   arg( QFileInfo( m_targetFilePath ).fileName(), m_file.errorString() );
		cancel();
		return false;
	}

	m_bytesWritten += data.size();

	return true;
}



bool FileReceiver::finish( QString* errorString )
{
	if( m_file.isOpen() == false )
	{
		*errorString = QCoreApplication::translate( "FileReceiver", "No file transfer in progress." );
		return false;
	}

	m_file.close();

	if( QFile::exists( m_targetFilePath ) )
	{
		if( m_overwrite == false || QFile::remove( m_targetFilePath ) == false )
		{
			*errorString = QCoreApplication::translate( "FileReceiver", "Could not replace file %1." ).
						   arg( QFileInfo( m_targetFilePath ).fileName() );
			m_file.remove();
			return false;
		}
	}

	if( m_file.rename( m_targetFilePath ) == false )
	{
		*errorString = QCoreApplication::translate( "FileReceiver", "Could not save file %1: %2" ).
					   arg( QFileInfo( m_targetFilePath ).fileName(), m_file.errorString() );
		m_file.remove();
		return false;
	}

	return true;
}



void FileReceiver::cancel()
{
	if( m_file.isOpen() )
	{
		m_file.close();
		m_file.remove();
	}
}



QString FileReceiver::uniqueFilePath( const QString& directory, const QString& fileName )
{
	const QDir dir( directory );
	auto filePath = dir.filePath( fileName );
	if( QFileInfo::exists( filePath ) == false )
	{
		return filePath;
	}

	const QFileInfo fileInfo( fileName );
	const auto baseName = fileInfo.completeBaseName();
	const auto suffix = fileInfo.suffix().isEmpty() ? QString() : QLatin1Char('.') + fileInfo.suffix();

	for( int i = 2; i < 10000; ++i )
	{
		filePath = dir.filePath( QStringLiteral("%1 (%2)%3").arg( baseName ).arg( i ).arg( suffix ) );
		if( QFileInfo::exists( filePath ) == false )
		{
			return filePath;
		}
	}

	return dir.filePath( fileName );
}



QString FileReceiver::sanitizeRelativePath( const QString& relativePath )
{
	QStringList components;
	const auto parts = QDir::fromNativeSeparators( relativePath ).split( QLatin1Char('/'), Qt::SkipEmptyParts );
	for( const auto& part : parts )
	{
		if( part == QLatin1String(".") || part == QLatin1String("..") || part.contains( QLatin1Char(':') ) )
		{
			continue;
		}
		components.append( part );
	}

	return components.join( QLatin1Char('/') );
}
