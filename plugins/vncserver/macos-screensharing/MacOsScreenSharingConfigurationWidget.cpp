/*
 * MacOsScreenSharingConfigurationWidget.cpp - configuration widget for macOS Screen Sharing VNC server plugin
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

#include <QCheckBox>
#include <QDesktopServices>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QUrl>

#include "Configuration/UiMapping.h"
#include "MacOsScreenSharingConfiguration.h"
#include "MacOsScreenSharingConfigurationWidget.h"


MacOsScreenSharingConfigurationWidget::MacOsScreenSharingConfigurationWidget( MacOsScreenSharingConfiguration& configuration,
																			  QWidget* parent ) :
	QWidget( parent ),
	m_configuration( configuration )
{
	auto layout = new QFormLayout( this );
	layout->setContentsMargins( 0, 0, 0, 0 );

	m_manageScreenSharingCheckBox = new QCheckBox( tr( "Enable and configure Screen Sharing automatically" ) );
	m_manageScreenSharingCheckBox->setToolTip( tr( "Veyon enables Screen Sharing and sets a random VNC password "
												   "each time the Veyon Service starts." ) );
	layout->addRow( m_manageScreenSharingCheckBox );

	m_serverPortSpinBox = new QSpinBox;
	m_serverPortSpinBox->setRange( 1, 65535 );
	layout->addRow( tr( "Screen Sharing port" ), m_serverPortSpinBox );

	m_passwordEdit = new QLineEdit;
	m_passwordEdit->setEchoMode( QLineEdit::Password );
	m_passwordEdit->setMaxLength( 8 );
	m_passwordEdit->setPlaceholderText( tr( "generated automatically" ) );
	layout->addRow( tr( "VNC password" ), m_passwordEdit );

	auto infoLabel = new QLabel( tr( "Veyon uses the built-in Screen Sharing service of macOS to capture the screen and "
									 "to control mouse and keyboard. If automatic configuration is disabled, enable "
									 "Screen Sharing in System Settings › General › Sharing, click ⓘ and enable "
									 "\"VNC viewers may control screen with password\" using the password entered above." ) );
	infoLabel->setWordWrap( true );
	layout->addRow( infoLabel );

	auto settingsButton = new QPushButton( tr( "Open Sharing settings" ) );
	connect( settingsButton, &QPushButton::clicked, this, []() {
		QDesktopServices::openUrl( QUrl( QStringLiteral("x-apple.systempreferences:com.apple.Sharing-Settings.extension") ) );
	} );
	layout->addRow( settingsButton );

	Configuration::UiMapping::initWidgetFromProperty( m_configuration.manageScreenSharingProperty(), m_manageScreenSharingCheckBox );
	Configuration::UiMapping::initWidgetFromProperty( m_configuration.serverPortProperty(), m_serverPortSpinBox );
	Configuration::UiMapping::initWidgetFromProperty( m_configuration.passwordProperty(), m_passwordEdit );

	Configuration::UiMapping::connectWidgetToProperty( m_configuration.manageScreenSharingProperty(), m_manageScreenSharingCheckBox );
	Configuration::UiMapping::connectWidgetToProperty( m_configuration.serverPortProperty(), m_serverPortSpinBox );
	Configuration::UiMapping::connectWidgetToProperty( m_configuration.passwordProperty(), m_passwordEdit );
}
