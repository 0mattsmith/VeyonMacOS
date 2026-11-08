/*
 * ScriptRunner.h - runs scripts of script add-ons
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

#include <functional>

#include <QObject>

class QProcess;
class QTemporaryFile;

//! \brief Runs a script with the given interpreter asynchronously and reports exit code and output
class ScriptRunner : public QObject
{
	Q_OBJECT
public:
	struct Result
	{
		int exitCode{-1};
		QString output;
		QString error;
	};

	using Callback = std::function<void(const Result&)>;

	static void run( const QString& interpreter, const QString& code, const QString& input, const QString& addOnName,
					 const Callback& callback );

private:
	static constexpr auto Timeout = 10 * 60 * 1000;
	static constexpr auto MaximumOutputSize = 64 * 1024;

	ScriptRunner( const Callback& callback );
	~ScriptRunner() override;

	bool start( const QString& interpreter, const QString& code, const QString& input, const QString& addOnName );
	void finish( const QString& error = {} );

	Callback m_callback;
	QProcess* m_process{nullptr};
	QTemporaryFile* m_scriptFile{nullptr};
	bool m_finished{false};

};
