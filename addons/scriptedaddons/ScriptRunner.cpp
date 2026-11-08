/*
 * ScriptRunner.cpp - runs scripts of script add-ons
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
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>

#include "ScriptRunner.h"
#include "VeyonCore.h"


void ScriptRunner::run( const QString& interpreter, const QString& code, const QString& input, const QString& addOnName,
						const Callback& callback )
{
	// deletes itself once finished
	auto runner = new ScriptRunner( callback );
	if( runner->start( interpreter, code, input, addOnName ) == false )
	{
		runner->finish( tr( "The interpreter \"%1\" is not available on this computer." ).arg( interpreter ) );
	}
}



ScriptRunner::ScriptRunner( const Callback& callback ) :
	QObject( nullptr ),
	m_callback( callback )
{
}



ScriptRunner::~ScriptRunner()
{
	delete m_scriptFile;
}



bool ScriptRunner::start( const QString& interpreter, const QString& code, const QString& input, const QString& addOnName )
{
	QString program;
	QStringList arguments;
	QString suffix = QStringLiteral(".sh");
	auto content = code.toUtf8();

#if defined(Q_OS_WIN)
	if( interpreter == QLatin1String("powershell") )
	{
		program = QStringLiteral("powershell.exe");
		suffix = QStringLiteral(".ps1");
		// UTF-8 BOM so PowerShell 5 reads non-ASCII characters correctly
		content.prepend( "\xEF\xBB\xBF" );
		arguments = { QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
					  QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"), QStringLiteral("-File") };
	}
	else if( interpreter == QLatin1String("cmd") )
	{
		program = QStringLiteral("cmd.exe");
		suffix = QStringLiteral(".cmd");
		content.replace( "\r\n", "\n" ).replace( "\n", "\r\n" );
		arguments = { QStringLiteral("/d"), QStringLiteral("/c") };
	}
#elif defined(Q_OS_MACOS)
	if( interpreter == QLatin1String("zsh") || interpreter == QLatin1String("bash") || interpreter == QLatin1String("sh") )
	{
		program = QStringLiteral("/bin/") + interpreter;
	}
	else if( interpreter == QLatin1String("osascript") )
	{
		program = QStringLiteral("/usr/bin/osascript");
		suffix = QStringLiteral(".applescript");
	}
#else
	if( interpreter == QLatin1String("bash") || interpreter == QLatin1String("sh") )
	{
		program = QStandardPaths::findExecutable( interpreter );
	}
	else if( interpreter == QLatin1String("python3") )
	{
		program = QStandardPaths::findExecutable( interpreter );
		suffix = QStringLiteral(".py");
	}
#endif

	if( program.isEmpty() )
	{
		return false;
	}

	m_scriptFile = new QTemporaryFile( QDir::tempPath() + QStringLiteral("/veyon-script-XXXXXX") + suffix );
	if( m_scriptFile->open() == false || m_scriptFile->write( content ) != content.size() )
	{
		return false;
	}
	m_scriptFile->close();

	auto environment = QProcessEnvironment::systemEnvironment();
	environment.insert( QStringLiteral("VEYON_INPUT"), input );
	environment.insert( QStringLiteral("VEYON_ADDON"), addOnName );

	m_process = new QProcess( this );
	m_process->setProcessChannelMode( QProcess::MergedChannels );
	m_process->setProcessEnvironment( environment );
	m_process->setWorkingDirectory( QDir::homePath() );

	connect( m_process, QOverload<int, QProcess::ExitStatus>::of( &QProcess::finished ), this, [this]() { finish(); } );
	connect( m_process, &QProcess::errorOccurred, this, [this]( QProcess::ProcessError error ) {
		if( error == QProcess::FailedToStart )
		{
			finish( tr( "The script could not be started: %1" ).arg( m_process->errorString() ) );
		}
	} );

	QTimer::singleShot( Timeout, this, [this]() {
		if( m_process->state() != QProcess::NotRunning )
		{
			m_process->kill();
			finish( tr( "The script has been stopped because it did not finish within %1 minutes." ).arg( Timeout / 60000 ) );
		}
	} );

	m_process->start( program, arguments + QStringList{ m_scriptFile->fileName() } );
	m_process->closeWriteChannel();

	return true;
}



void ScriptRunner::finish( const QString& error )
{
	if( m_finished )
	{
		return;
	}
	m_finished = true;

	Result result;
	result.error = error;

	if( m_process )
	{
		const auto output = m_process->readAll();
		result.output = QString::fromUtf8( output.left( MaximumOutputSize ) );
		if( output.size() > MaximumOutputSize )
		{
			result.output += QStringLiteral("\n") + tr( "(output truncated)" );
		}
		if( m_process->state() == QProcess::NotRunning && m_process->exitStatus() == QProcess::NormalExit )
		{
			result.exitCode = m_process->exitCode();
		}
	}

	if( m_callback )
	{
		m_callback( result );
	}

	deleteLater();
}
