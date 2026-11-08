/*
 * FileReceiver.h - writes a file received in chunks
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

#include <QFile>

/*!
 * \brief Receives a file in chunks. Data is written to a temporary ".part" file which is renamed to
 * the target name once the transfer has been completed, so incomplete files never show up.
 */
class FileReceiver
{
public:
	FileReceiver() = default;
	~FileReceiver();

	bool begin( const QString& targetFilePath, bool overwrite, QString* errorString );
	bool write( const QByteArray& data, QString* errorString );
	bool finish( QString* errorString );
	void cancel();

	bool isActive() const
	{
		return m_file.isOpen();
	}

	qint64 bytesWritten() const
	{
		return m_bytesWritten;
	}

	const QString& targetFilePath() const
	{
		return m_targetFilePath;
	}

	//! \brief Returns a file name which does not exist yet in the given directory, e.g. "report (2).docx"
	static QString uniqueFilePath( const QString& directory, const QString& fileName );

	//! \brief Removes path components which could escape the target directory (e.g. "..")
	static QString sanitizeRelativePath( const QString& relativePath );

private:
	static constexpr auto PartFileSuffix = ".veyonpart";

	QFile m_file;
	QString m_targetFilePath;
	qint64 m_bytesWritten{0};
	bool m_overwrite{false};

};
