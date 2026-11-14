/*
 * main.cpp - main file for Veyon Configurator
 *
 * Copyright (c) 2010-2026 Tobias Junghans <tobydox@veyon.io>
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

#include <QApplication>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>

#include "VeyonConfiguration.h"
#include "VeyonCore.h"
#include "MainWindow.h"
#include "PlatformCoreFunctions.h"
#include "Logger.h"


namespace {

// The Configurator runs with administrative privileges, i.e. as a different user (root) on macOS and Linux, so the
// socket is created at a fixed location accessible for all users instead of the temporary directory of the user
QString instanceServerName()
{
#ifdef Q_OS_WIN
	return QStringLiteral("veyon-configurator");
#else
	return QStringLiteral("/tmp/veyon-configurator");
#endif
}

// brings a running Configurator to the front - returns false if no Configurator is running
bool activateRunningInstance()
{
	QLocalSocket socket;
	socket.connectToServer( instanceServerName() );
	if( socket.waitForConnected( 1000 ) == false )
	{
		return false;
	}

	// connecting is the request to activate - no data has to be sent
	socket.disconnectFromServer();
	return true;
}

}



int main( int argc, char **argv )
{
	VeyonCore::setupApplicationParameters( VeyonCore::Component::Configurator );

	QApplication app( argc, argv );

#ifdef Q_OS_MACOS
	// the Configurator isn't the main program of Veyon.app, so set its Dock icon explicitly
	QApplication::setWindowIcon( QIcon( QStringLiteral(":/configurator/veyon-configurator.png") ) );
#endif

	// only allow one Configurator at a time - checked before asking for administrative privileges
	if( activateRunningInstance() )
	{
		return 0;
	}

	VeyonCore core( &app, VeyonCore::Component::Configurator, QStringLiteral("Configurator") );

	// make sure to run as admin
	if( qEnvironmentVariableIntValue( "VEYON_CONFIGURATOR_NO_ELEVATION" ) == 0 &&
		VeyonCore::platform().coreFunctions().isRunningAsAdmin() == false &&
		app.arguments().size() <= 1 )
	{
		if( VeyonCore::platform().coreFunctions().runProgramAsAdmin( QCoreApplication::applicationFilePath(), {
																	 QStringLiteral("-elevated") } ) )
		{
			return 0;
		}

#ifdef Q_OS_MACOS
		QMessageBox::warning( nullptr, MainWindow::tr( "Insufficient privileges" ),
							  MainWindow::tr( "Could not start with administrative privileges. "
											  "Please enter the name and password of an administrator account "
											  "when macOS asks for them. The program will be run with normal "
											  "user privileges, so the configuration and authentication keys "
											  "cannot be saved." ) );
#else
		QMessageBox::warning( nullptr, MainWindow::tr( "Insufficient privileges" ),
							  MainWindow::tr( "Could not start with administrative privileges. "
											  "Please make sure a sudo-like program is installed "
											  "for your desktop environment! The program will "
											  "be run with normal user privileges.") );
#endif
	}

	if( VeyonConfiguration().isStoreWritable() == false &&
		VeyonCore::config().logLevel() != Logger::LogLevel::Debug )
	{
		QMessageBox::critical(nullptr,
							  MainWindow::tr("Configuration not writable"),
							  MainWindow::tr("The local configuration backend reported that the "
											 "configuration is not writable! Please run Veyon "
											 "Configurator with higher privileges."));
		return -1;
	}

#ifdef Q_OS_MACOS
	// the system-wide directories (/Library/Application Support/Veyon) can only be written as root - without it
	// saving authentication keys or installing add-ons fails with hard to understand permission errors
	if( VeyonCore::platform().coreFunctions().isRunningAsAdmin() == false &&
		VeyonCore::config().logLevel() != Logger::LogLevel::Debug )
	{
		QMessageBox::warning( nullptr, MainWindow::tr( "Insufficient privileges" ),
							  MainWindow::tr( "Veyon Configurator is not running with administrative privileges. "
											  "Changes to the configuration and authentication keys cannot be saved. "
											  "Please start Veyon Configurator from the Applications folder and "
											  "authenticate as administrator." ) );
	}
#endif

	QLocalServer instanceServer;
	instanceServer.setSocketOptions( QLocalServer::WorldAccessOption );
	if( instanceServer.listen( instanceServerName() ) == false )
	{
		// another Configurator may have been started at the same time
		if( activateRunningInstance() )
		{
			return 0;
		}

		// left over by a Configurator which hasn't been closed properly
		QLocalServer::removeServer( instanceServerName() );
		if( instanceServer.listen( instanceServerName() ) == false )
		{
			vWarning() << "could not listen for other instances:" << instanceServer.errorString();
		}
	}

	// now create the main window
	auto mainWindow = new MainWindow;
	mainWindow->show();

	// bring the window to the front whenever another instance has been started
	QObject::connect( &instanceServer, &QLocalServer::newConnection, mainWindow, [&instanceServer, mainWindow]() {
		while( auto connection = instanceServer.nextPendingConnection() )
		{
			connection->disconnectFromServer();
			connection->deleteLater();
		}
		mainWindow->setWindowState( ( mainWindow->windowState() & ~Qt::WindowMinimized ) | Qt::WindowActive );
		mainWindow->show();
		VeyonCore::platform().coreFunctions().raiseWindow( mainWindow, false );
	} );

	return core.exec();
}
