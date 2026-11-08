/*
 * WebPageSenderPlugin.cpp - implementation of WebPageSenderPlugin class
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

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

#include "FeatureWorkerManager.h"
#include "FileReceiver.h"
#include "PlatformCoreFunctions.h"
#include "PlatformPluginInterface.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"
#include "WebPageSenderDialog.h"
#include "WebPageSenderPlugin.h"


WebPageSenderPlugin::WebPageSenderPlugin( QObject* parent ) :
	QObject( parent ),
	m_webPageSenderFeature( QStringLiteral("WebPageSender"),
							Feature::Flag::Action | Feature::Flag::AllComponents,
							Feature::Uid( "7c3a9e15-4f62-4d08-8b1e-5a9d2c6f3e81" ),
							Feature::Uid(),
							tr( "Web page" ), {},
							tr( "Open a web page on the selected student computers" ),
							QStringLiteral(":/webpagesender/webpage-send.png") ),
	m_features( { m_webPageSenderFeature } )
{
}



bool WebPageSenderPlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
										  const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( featureUid != m_webPageSenderFeature.uid() || operation != Operation::Start )
	{
		return false;
	}

	WebPage webPage;
	webPage.url = normalizeUrl( arguments.value( QStringLiteral("url") ).toString() );
	webPage.title = arguments.value( QStringLiteral("title") ).toString();
	webPage.mode = Mode( arguments.value( QStringLiteral("mode"), int(Mode::Open) ).toInt() );
	webPage.browser = Browser( arguments.value( QStringLiteral("browser"), int(Browser::Default) ).toInt() );
	webPage.newWindow = arguments.value( QStringLiteral("newWindow"), true ).toBool();
	webPage.fullScreen = arguments.value( QStringLiteral("fullScreen"), false ).toBool();

	if( webPage.url.isValid() == false )
	{
		return false;
	}

	sendWebPage( computerControlInterfaces, webPage );

	return true;
}



bool WebPageSenderPlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
										const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( feature != m_webPageSenderFeature )
	{
		return false;
	}

	auto interfaces = computerControlInterfaces;
	interfaces.removeLocalHostInterfaces();
	if( interfaces.isEmpty() )
	{
		interfaces = master.filteredComputerControlInterfaces();
		interfaces.removeLocalHostInterfaces();
	}

	WebPageSenderDialog dialog( master, interfaces.size(), master.mainWindow() );
	if( dialog.exec() == QDialog::Accepted )
	{
		sendWebPage( interfaces, dialog.webPage() );
	}

	return true;
}



bool WebPageSenderPlugin::handleFeatureMessage( VeyonServerInterface& server,
												const MessageContext& messageContext,
												const FeatureMessage& message )
{
	Q_UNUSED(messageContext)

	if( message.featureUid() != m_webPageSenderFeature.uid() )
	{
		return false;
	}

	// the web page is opened by a worker running as the logged on student
	server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );

	return true;
}



bool WebPageSenderPlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	Q_UNUSED(worker)

	if( message.featureUid() != m_webPageSenderFeature.uid() )
	{
		return false;
	}

	m_idleTimer.restart();

	WebPage webPage;
	webPage.url = normalizeUrl( message.argument( Argument::Url ).toString() );
	webPage.title = message.argument( Argument::Title ).toString();
	webPage.mode = Mode( message.argument( Argument::Mode ).toInt() );
	webPage.browser = Browser( message.argument( Argument::Browser ).toInt() );
	webPage.newWindow = message.argument( Argument::NewWindow ).toBool();
	webPage.fullScreen = message.argument( Argument::FullScreen ).toBool();

	// only web pages can be opened (no local files or other URL schemes)
	if( webPage.url.isValid() == false )
	{
		vWarning() << "ignoring invalid URL" << message.argument( Argument::Url );
		return true;
	}

	switch( webPage.mode )
	{
	case Mode::Open:
		openWebPage( webPage );
		break;
	case Mode::Notify:
		showNotification( webPage );
		break;
	case Mode::DesktopShortcut:
		createDesktopShortcut( webPage );
		break;
	}

	return true;
}



void WebPageSenderPlugin::sendWebPage( const ComputerControlInterfaceList& computerControlInterfaces, const WebPage& webPage )
{
	sendFeatureMessage( FeatureMessage( m_webPageSenderFeature.uid(), Command::SendWebPage )
						.addArgument( Argument::Url, webPage.url.toString() )
						.addArgument( Argument::Title, webPage.title )
						.addArgument( Argument::Mode, int(webPage.mode) )
						.addArgument( Argument::Browser, int(webPage.browser) )
						.addArgument( Argument::NewWindow, webPage.newWindow )
						.addArgument( Argument::FullScreen, webPage.fullScreen ),
						computerControlInterfaces );
}



QUrl WebPageSenderPlugin::normalizeUrl( const QString& input )
{
	auto text = input.trimmed();
	if( text.isEmpty() )
	{
		return {};
	}

	if( text.contains( QLatin1String("://") ) == false )
	{
		text.prepend( QStringLiteral("https://") );
	}

	const QUrl url( text, QUrl::StrictMode );
	if( url.isValid() == false || url.host().isEmpty() ||
		( url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https") ) )
	{
		return {};
	}

	return url;
}



void WebPageSenderPlugin::openWebPage( const WebPage& webPage )
{
	if( launchBrowser( webPage ) == false )
	{
		QDesktopServices::openUrl( webPage.url );
	}
}



void WebPageSenderPlugin::showNotification( const WebPage& webPage )
{
	const auto title = webPage.title.isEmpty() ? webPage.url.host() : webPage.title;

	auto messageBox = new QMessageBox( QMessageBox::Information, tr( "Web page from your teacher" ),
									   tr( "Your teacher has shared a web page with you:\n\n%1\n%2" )
									   .arg( title, webPage.url.toDisplayString() ) );
	auto openButton = messageBox->addButton( tr( "Open" ), QMessageBox::AcceptRole );
	messageBox->addButton( QMessageBox::Close );
	messageBox->setDefaultButton( openButton );
	messageBox->setAttribute( Qt::WA_DeleteOnClose );
	connect( openButton, &QPushButton::clicked, this, [this, webPage]() { openWebPage( webPage ); } );
	messageBox->show();

	VeyonCore::platform().coreFunctions().raiseWindow( messageBox, true );
}



bool WebPageSenderPlugin::createDesktopShortcut( const WebPage& webPage )
{
	const auto desktop = QStandardPaths::writableLocation( QStandardPaths::DesktopLocation );

	auto name = webPage.title.isEmpty() ? webPage.url.host() : webPage.title;
	name.replace( QRegularExpression( QStringLiteral("[\\\\/:*?\"<>|]") ), QStringLiteral("_") );

#if defined(Q_OS_WIN)
	const auto filePath = FileReceiver::uniqueFilePath( desktop, name + QStringLiteral(".url") );
	const auto content = QStringLiteral("[InternetShortcut]\r\nURL=%1\r\n").arg( webPage.url.toString() );
#elif defined(Q_OS_MACOS)
	const auto filePath = FileReceiver::uniqueFilePath( desktop, name + QStringLiteral(".webloc") );
	const auto content = QStringLiteral(
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
		"<plist version=\"1.0\">\n<dict>\n\t<key>URL</key>\n\t<string>%1</string>\n</dict>\n</plist>\n" )
						 .arg( webPage.url.toString().toHtmlEscaped() );
#else
	const auto filePath = FileReceiver::uniqueFilePath( desktop, name + QStringLiteral(".desktop") );
	const auto content = QStringLiteral( "[Desktop Entry]\nType=Link\nName=%1\nURL=%2\nIcon=text-html\n" )
						 .arg( name, webPage.url.toString() );
#endif

	QFile file( filePath );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) == false )
	{
		vWarning() << "could not create desktop shortcut" << filePath;
		return false;
	}

	file.write( content.toUtf8() );
	file.close();

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
	file.setPermissions( file.permissions() | QFile::ExeOwner );
#endif

	return true;
}



bool WebPageSenderPlugin::launchBrowser( const WebPage& webPage )
{
	// the default browser is opened via the desktop environment
	if( webPage.browser == Browser::Default )
	{
		return false;
	}

	const auto url = webPage.url.toString();
	const auto isFirefox = webPage.browser == Browser::Firefox;

	QStringList arguments;
	if( webPage.fullScreen )
	{
		arguments.append( QStringLiteral("--kiosk") );
		if( webPage.browser == Browser::Edge )
		{
			arguments.append( QStringLiteral("--edge-kiosk-type=fullscreen") );
		}
	}
	else if( webPage.newWindow )
	{
		arguments.append( isFirefox ? QStringLiteral("-new-window") : QStringLiteral("--new-window") );
	}

#if defined(Q_OS_WIN)
	QString executable;
	switch( webPage.browser )
	{
	case Browser::Chrome: executable = QStringLiteral("chrome.exe"); break;
	case Browser::Edge: executable = QStringLiteral("msedge.exe"); break;
	case Browser::Firefox: executable = QStringLiteral("firefox.exe"); break;
	case Browser::Default:
	case Browser::Safari:
		return false;
	}

	// installed browsers register their location in the "App Paths" registry key
	QString executablePath;
	for( const auto& root : { QStringLiteral("HKEY_CURRENT_USER"), QStringLiteral("HKEY_LOCAL_MACHINE") } )
	{
		const QSettings appPaths( QStringLiteral("%1\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\%2").arg( root, executable ),
								  QSettings::NativeFormat );
		executablePath = appPaths.value( QStringLiteral("Default") ).toString();
		if( executablePath.isEmpty() == false )
		{
			break;
		}
	}

	if( executablePath.isEmpty() )
	{
		return false;
	}

	return QProcess::startDetached( executablePath, arguments + QStringList{ url } );

#elif defined(Q_OS_MACOS)
	QString application;
	switch( webPage.browser )
	{
	case Browser::Chrome: application = QStringLiteral("Google Chrome"); break;
	case Browser::Edge: application = QStringLiteral("Microsoft Edge"); break;
	case Browser::Firefox: application = QStringLiteral("Firefox"); break;
	case Browser::Safari: application = QStringLiteral("Safari"); break;
	case Browser::Default: return false;
	}

	if( webPage.browser == Browser::Safari || arguments.isEmpty() )
	{
		return QProcess::startDetached( QStringLiteral("/usr/bin/open"), { QStringLiteral("-a"), application, url } );
	}

	return QProcess::startDetached( QStringLiteral("/usr/bin/open"),
									QStringList{ QStringLiteral("-na"), application, QStringLiteral("--args") } + arguments + QStringList{ url } );

#else
	QStringList candidates;
	switch( webPage.browser )
	{
	case Browser::Chrome: candidates = { QStringLiteral("google-chrome"), QStringLiteral("chromium"), QStringLiteral("chromium-browser") }; break;
	case Browser::Edge: candidates = { QStringLiteral("microsoft-edge") }; break;
	case Browser::Firefox: candidates = { QStringLiteral("firefox") }; break;
	case Browser::Default:
	case Browser::Safari:
		return false;
	}

	for( const auto& candidate : std::as_const(candidates) )
	{
		const auto executablePath = QStandardPaths::findExecutable( candidate );
		if( executablePath.isEmpty() == false )
		{
			return QProcess::startDetached( executablePath, arguments + QStringList{ url } );
		}
	}

	return false;
#endif
}
