/*
 * AddOnsConfigurationPage.cpp - configuration page for add-ons
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
#include <QGroupBox>
#include <QVBoxLayout>

#include "AddOnStoreWidget.h"
#include "AddOnsConfigurationPage.h"
#include "VeyonConfiguration.h"


AddOnsConfigurationPage::AddOnsConfigurationPage( QWidget* parent ) :
	ConfigurationPage( parent )
{
	setWindowTitle( tr( "Add-ons" ) );
	setWindowIcon( QIcon( QStringLiteral(":/core/addon-store.png") ) );

	auto layout = new QVBoxLayout( this );

	auto settingsGroup = new QGroupBox( tr( "Settings" ) );
	auto settingsLayout = new QVBoxLayout( settingsGroup );
	m_storeForTeachersCheckBox = new QCheckBox( tr( "Allow teachers to manage add-ons in Veyon Master "
													"(administrator credentials are still required)" ) );
	m_userAddOnsCheckBox = new QCheckBox( tr( "Allow teachers to create and install their own script add-ons" ) );
	settingsLayout->addWidget( m_storeForTeachersCheckBox );
	settingsLayout->addWidget( m_userAddOnsCheckBox );
	layout->addWidget( settingsGroup );

	m_storeWidget = new AddOnStoreWidget( AddOnStoreWidget::Mode::Configurator );
	m_storeWidget->setMinimumHeight( 520 );
	layout->addWidget( m_storeWidget, 1 );

	connect( m_storeWidget, &AddOnStoreWidget::configurationChanged, this, &ConfigurationPage::widgetsChanged );
}



void AddOnsConfigurationPage::resetWidgets()
{
	m_storeForTeachersCheckBox->setChecked( VeyonCore::config().addOnStoreForTeachersEnabled() );
	m_userAddOnsCheckBox->setChecked( VeyonCore::config().userAddOnsEnabled() );
	m_storeWidget->refresh();
}



void AddOnsConfigurationPage::connectWidgetsToProperties()
{
	connect( m_storeForTeachersCheckBox, &QCheckBox::toggled, this, []( bool checked ) {
		VeyonCore::config().setAddOnStoreForTeachersEnabled( checked );
	} );
	connect( m_userAddOnsCheckBox, &QCheckBox::toggled, this, []( bool checked ) {
		VeyonCore::config().setUserAddOnsEnabled( checked );
	} );
}



void AddOnsConfigurationPage::applyConfiguration()
{
	m_storeWidget->refresh();
}
