/*
 * SystemTrayIcon.cpp - implementation of SystemTrayIcon class
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

#include <QCoreApplication>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <QTimer>

#ifdef Q_OS_MACOS
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QPainterPath>
#include <QPen>
#include <QProcess>

#include "Filesystem.h"
#endif

#include "SystemTrayIcon.h"
#include "FeatureWorkerManager.h"
#include "PlatformCoreFunctions.h"
#include "VeyonCore.h"
#include "VeyonConfiguration.h"
#include "VeyonServerInterface.h"


SystemTrayIcon::SystemTrayIcon( QObject* parent ) :
	QObject( parent ),
	m_systemTrayIconFeature( Feature( QLatin1String( staticMetaObject.className() ),
									  Feature::Flag::Session | Feature::Flag::Service | Feature::Flag::Worker | Feature::Flag::Builtin,
									  Feature::Uid( "8e997d84-ebb9-430f-8f72-d45d9821963d" ),
									  Feature::Uid(),
									  tr( "System tray icon"), {}, {} ) ),
	m_hidden(VeyonCore::config().isTrayIconHidden())
{
}



SystemTrayIcon::~SystemTrayIcon()
{
#ifdef Q_OS_MACOS
	delete m_menu;
#endif
}



void SystemTrayIcon::setToolTip( const QString& toolTipText,
								FeatureWorkerManager& featureWorkerManager )
{
	if (m_hidden == false)
	{
		FeatureMessage featureMessage(m_systemTrayIconFeature.uid(), FeatureCommand::SetToolTip);
		featureMessage.addArgument(Argument::ToolTipText, toolTipText);

		featureWorkerManager.sendMessageToUnmanagedSessionWorker(featureMessage);
	}
}



void SystemTrayIcon::showMessage( const QString& messageTitle,
								 const QString& messageText,
								 FeatureWorkerManager& featureWorkerManager  )
{
	FeatureMessage featureMessage( m_systemTrayIconFeature.uid(), FeatureCommand::ShowMessage);
	featureMessage.addArgument( Argument::MessageTitle, messageTitle );
	featureMessage.addArgument( Argument::MessageText, messageText );

	featureWorkerManager.sendMessageToUnmanagedSessionWorker( featureMessage );
}



void SystemTrayIcon::showMessage(const ComputerControlInterfaceList &computerControlInterfaces,
								 const QString& messageTitle, const QString& messageText)
{
	sendFeatureMessage(FeatureMessage{m_systemTrayIconFeature.uid(), FeatureCommand::ShowMessage}
							.addArgument(Argument::MessageTitle, messageTitle)
							.addArgument(Argument::MessageText, messageText),
						computerControlInterfaces);
}



void SystemTrayIcon::setOverlay(const ComputerControlInterfaceList& computerControlInterfaces,
								const QString &iconUrl)
{
	if (m_hidden == false)
	{
		sendFeatureMessage(FeatureMessage{m_systemTrayIconFeature.uid(), FeatureCommand::SetOverlayIcon}
						   .addArgument(Argument::OverlayIconUrl, iconUrl),
						   computerControlInterfaces);
	}
}



bool SystemTrayIcon::handleFeatureMessage( VeyonServerInterface& server,
										  const MessageContext& messageContext,
										  const FeatureMessage& message )
{
	Q_UNUSED(messageContext)

	if( m_systemTrayIconFeature.uid() == message.featureUid() )
	{
		// forward message to worker
		server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );
		return true;
	}

	return false;
}



bool SystemTrayIcon::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	Q_UNUSED(worker)

	if( message.featureUid() != m_systemTrayIconFeature.uid() )
	{
		return false;
	}

	if (m_systemTrayIcon == nullptr && m_hidden == false)
	{
		m_systemTrayIcon = new QSystemTrayIcon( this );

#ifdef Q_OS_MACOS
		// menu bar extras are expected to offer a menu on macOS
		m_menu = new QMenu;
		m_systemTrayIcon->setContextMenu( m_menu );
		updateMenu( {} );
#endif

		updateIcon();

		m_systemTrayIcon->show();
	}

	switch(message.command<FeatureCommand>())
	{
	case FeatureCommand::SetToolTip:
		if( m_systemTrayIcon )
		{
			m_systemTrayIcon->setToolTip( message.argument( Argument::ToolTipText ).toString() );
#ifdef Q_OS_MACOS
			updateMenu( message.argument( Argument::ToolTipText ).toString() );
#endif
		}
		return true;

	case FeatureCommand::ShowMessage:
		if( m_systemTrayIcon )
		{
			m_systemTrayIcon->showMessage( message.argument( Argument::MessageTitle ).toString(),
										   message.argument( Argument::MessageText ).toString() );
		}
		else
		{
			auto messageBox = new QMessageBox(QMessageBox::Information,
											   message.argument(Argument::MessageTitle).toString(),
											   message.argument(Argument::MessageText).toString());
			messageBox->setAttribute(Qt::WA_DeleteOnClose);
			connect(messageBox, &QObject::destroyed,
					QCoreApplication::instance(), &QCoreApplication::quit);
			QTimer::singleShot(MessageBoxAutoCloseInterval, messageBox, &QWidget::close);
			messageBox->show();
			VeyonCore::platform().coreFunctions().raiseWindow(messageBox, true);
		}
		return true;

	case FeatureCommand::SetOverlayIcon:
		updateIcon(message.argument(Argument::OverlayIconUrl).toString());
		return true;

	default:
		break;
	}

	return false;
}



void SystemTrayIcon::updateIcon(const QString& overlayIconUrl)
{
	if (m_systemTrayIcon)
	{
		QImage overlayIcon;
		if (overlayIconUrl.isEmpty() == false)
		{
			// icon theme name or file of the overlay icon
			overlayIcon = VeyonCore::icon(overlayIconUrl).pixmap(QSize(128, 128)).toImage();
		}

#ifdef Q_OS_MACOS
		// use the monochrome template icon common for menu bar extras - overlays convey their information through
		// colors, so the regular icon is used as long as one is shown
		if (overlayIcon.isNull())
		{
			m_systemTrayIcon->setIcon(menuBarIcon());
			return;
		}
#endif

		QIcon icon;

		for(auto size : {16, 22, 32, 64})
		{
			QPixmap pixmap(QStringLiteral(":/core/icon%1.png").arg(size));
			if (overlayIcon.isNull() == false)
			{
				QPainter painter(&pixmap);
				painter.drawImage(0, 0, overlayIcon.scaled(pixmap.size()));
			}
			icon.addPixmap(pixmap);
		}

		m_systemTrayIcon->setIcon(icon);
	}
}



#ifdef Q_OS_MACOS

void SystemTrayIcon::updateMenu( const QString& statusText )
{
	if( m_menu == nullptr )
	{
		return;
	}

	m_menu->clear();

	// show the status ("Veyon Service <version> at <host>:<port>" and the active connections) as menu items,
	// as tooltips of menu bar extras are hardly discoverable
	bool hasStatusItems = false;
	const auto lines = statusText.split( QLatin1Char('\n') );
	for( const auto& line : lines )
	{
		const auto text = line.trimmed();
		if( text.isEmpty() == false )
		{
			m_menu->addAction( text )->setEnabled( false );
			hasStatusItems = true;
		}
	}

	if( hasStatusItems )
	{
		m_menu->addSeparator();
	}

	m_menu->addAction( tr( "Open Veyon Master" ), this, []() {
		openApplication( veyonBundlePath(), QStringLiteral("veyon-master") );
	} );

	m_menu->addAction( tr( "Open Veyon Configurator" ), this, []() {
		// "Veyon Configurator.app" (a launcher for the Configurator in Veyon.app) is installed next to Veyon.app
		const auto bundlePath = veyonBundlePath();
		openApplication( bundlePath.isEmpty() ? QString{} :
												QFileInfo( bundlePath ).path() + QStringLiteral("/Veyon Configurator.app"),
						 QStringLiteral("veyon-configurator") );
	} );
}



QString SystemTrayIcon::veyonBundlePath()
{
	// the worker is located in Veyon.app/Contents/Helpers
	const auto bundlePath = QDir::cleanPath( QCoreApplication::applicationDirPath() + QStringLiteral("/../..") );

	return bundlePath.endsWith( QLatin1String(".app") ) ? bundlePath : QString{};
}



void SystemTrayIcon::openApplication( const QString& bundlePath, const QString& programName )
{
	if( bundlePath.isEmpty() == false && QFileInfo::exists( bundlePath ) )
	{
		// LaunchServices brings an already running instance to the front instead of starting a second one
		QProcess::startDetached( QStringLiteral("/usr/bin/open"), { bundlePath } );
	}
	else
	{
		// no bundle, e.g. when running from a build directory
		QProcess::startDetached( VeyonCore::filesystem().programFilePath( programName ), {} );
	}
}



QIcon SystemTrayIcon::menuBarIcon()
{
	// Menu bar extras use monochrome template images which macOS tints according to the appearance of the menu
	// bar (light, dark, highlighted). Draw the Veyon logo - an eye inside a circle - as such a template.
	QIcon icon;

	for( auto size : { 16, 18, 22, 32, 36, 44, 64 } )
	{
		const auto s = qreal( size );
		const auto penWidth = s / 12;
		const QPointF center( s / 2, s / 2 );

		QPixmap pixmap( size, size );
		pixmap.fill( Qt::transparent );

		QPainter painter( &pixmap );
		painter.setRenderHint( QPainter::Antialiasing );

		painter.setPen( QPen( Qt::black, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ) );
		painter.setBrush( Qt::NoBrush );
		painter.drawEllipse( center, s / 2 - penWidth, s / 2 - penWidth );

		// the pupil is subtracted from the filled eye shape by the odd-even fill rule
		QPainterPath eye;
		eye.moveTo( s * 0.16, s * 0.5 );
		eye.quadTo( s * 0.5, s * 0.16, s * 0.84, s * 0.5 );
		eye.quadTo( s * 0.5, s * 0.84, s * 0.16, s * 0.5 );
		eye.closeSubpath();
		eye.addEllipse( center, s * 0.09, s * 0.09 );

		painter.setPen( Qt::NoPen );
		painter.setBrush( QBrush( Qt::black ) );
		painter.drawPath( eye );
		painter.end();

		icon.addPixmap( pixmap );
	}

	icon.setIsMask( true );

	return icon;
}

#endif
