/*
 * WebPageSenderDialog.cpp - implementation of WebPageSenderDialog class
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
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include "Configuration/Object.h"
#include "VeyonCore.h"
#include "VeyonMasterInterface.h"
#include "WebPageSenderDialog.h"


namespace {

constexpr auto ConfigParentKey = "WebPageSender";
constexpr auto HistoryKey = "History";
constexpr auto SettingsKey = "LastSettings";

}



WebPageSenderDialog::WebPageSenderDialog( VeyonMasterInterface& master, int computerCount, QWidget* parent ) :
	QDialog( parent ),
	m_master( master )
{
	setWindowTitle( tr( "Send web page" ) );
	setWindowIcon( VeyonCore::icon( QStringLiteral("veyon-webpage-send") ) );
	setMinimumWidth( 560 );

	auto mainLayout = new QVBoxLayout( this );

	auto introLabel = new QLabel( tr( "The web page will be sent to %n computer(s).", "", computerCount ) );
	mainLayout->addWidget( introLabel );

	auto formLayout = new QFormLayout;
	m_urlComboBox = new QComboBox;
	m_urlComboBox->setEditable( true );
	m_urlComboBox->setInsertPolicy( QComboBox::NoInsert );
	m_urlComboBox->lineEdit()->setPlaceholderText( tr( "e.g. www.example.com/worksheet" ) );
	const auto history = m_master.userConfigurationObject()->value( QLatin1String(HistoryKey), QLatin1String(ConfigParentKey), {} ).toStringList();
	m_urlComboBox->addItems( history );
	m_urlComboBox->setCurrentText( {} );
	formLayout->addRow( tr( "Address" ), m_urlComboBox );

	m_titleEdit = new QLineEdit;
	m_titleEdit->setPlaceholderText( tr( "optional, shown to the students" ) );
	formLayout->addRow( tr( "Title" ), m_titleEdit );
	mainLayout->addLayout( formLayout );

	auto modeGroup = new QGroupBox( tr( "How should the web page be delivered?" ) );
	auto modeLayout = new QVBoxLayout( modeGroup );
	m_openButton = new QRadioButton( tr( "Open the web page immediately" ) );
	m_notifyButton = new QRadioButton( tr( "Show a message with a button to open the web page" ) );
	m_shortcutButton = new QRadioButton( tr( "Put a shortcut to the web page on the desktop" ) );
	m_openButton->setChecked( true );
	for( auto button : { m_openButton, m_notifyButton, m_shortcutButton } )
	{
		modeLayout->addWidget( button );
		connect( button, &QRadioButton::toggled, this, &WebPageSenderDialog::updateOptions );
	}
	mainLayout->addWidget( modeGroup );

	auto browserGroup = new QGroupBox( tr( "Browser" ) );
	auto browserLayout = new QFormLayout( browserGroup );
	m_browserComboBox = new QComboBox;
	m_browserComboBox->addItem( tr( "Default browser" ), int(WebPageSenderPlugin::Browser::Default) );
	m_browserComboBox->addItem( QStringLiteral("Google Chrome"), int(WebPageSenderPlugin::Browser::Chrome) );
	m_browserComboBox->addItem( QStringLiteral("Microsoft Edge"), int(WebPageSenderPlugin::Browser::Edge) );
	m_browserComboBox->addItem( QStringLiteral("Mozilla Firefox"), int(WebPageSenderPlugin::Browser::Firefox) );
	m_browserComboBox->addItem( tr( "Safari (macOS only)" ), int(WebPageSenderPlugin::Browser::Safari) );
	connect( m_browserComboBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this, &WebPageSenderDialog::updateOptions );
	browserLayout->addRow( tr( "Open with" ), m_browserComboBox );
	m_newWindowCheckBox = new QCheckBox( tr( "Open in a new window" ) );
	m_newWindowCheckBox->setChecked( true );
	m_fullScreenCheckBox = new QCheckBox( tr( "Full screen (kiosk) mode" ) );
	m_fullScreenCheckBox->setToolTip( tr( "Shows only the web page without address bar. Students can leave full screen mode "
										  "by closing the browser (Alt+F4 or ⌘Q)." ) );
	browserLayout->addRow( QString(), m_newWindowCheckBox );
	browserLayout->addRow( QString(), m_fullScreenCheckBox );
	mainLayout->addWidget( browserGroup );

	auto buttonBox = new QDialogButtonBox( QDialogButtonBox::Cancel );
	auto sendButton = buttonBox->addButton( tr( "Send" ), QDialogButtonBox::AcceptRole );
	sendButton->setIcon( VeyonCore::icon( QStringLiteral("veyon-webpage-send") ) );
	sendButton->setDefault( true );
	connect( buttonBox, &QDialogButtonBox::accepted, this, &WebPageSenderDialog::accept );
	connect( buttonBox, &QDialogButtonBox::rejected, this, &WebPageSenderDialog::reject );
	mainLayout->addWidget( buttonBox );

	// restore settings used last time
	const auto lastSettings = m_master.userConfigurationObject()->value( QLatin1String(SettingsKey), QLatin1String(ConfigParentKey), {} ).toMap();
	if( lastSettings.isEmpty() == false )
	{
		m_browserComboBox->setCurrentIndex( qMax( 0, m_browserComboBox->findData( lastSettings.value( QStringLiteral("browser") ).toInt() ) ) );
		m_newWindowCheckBox->setChecked( lastSettings.value( QStringLiteral("newWindow"), true ).toBool() );
		m_fullScreenCheckBox->setChecked( lastSettings.value( QStringLiteral("fullScreen") ).toBool() );
		switch( WebPageSenderPlugin::Mode( lastSettings.value( QStringLiteral("mode") ).toInt() ) )
		{
		case WebPageSenderPlugin::Mode::Open: m_openButton->setChecked( true ); break;
		case WebPageSenderPlugin::Mode::Notify: m_notifyButton->setChecked( true ); break;
		case WebPageSenderPlugin::Mode::DesktopShortcut: m_shortcutButton->setChecked( true ); break;
		}
	}

	updateOptions();
}



void WebPageSenderDialog::accept()
{
	m_webPage.url = WebPageSenderPlugin::normalizeUrl( m_urlComboBox->currentText() );
	if( m_webPage.url.isValid() == false )
	{
		QMessageBox::warning( this, windowTitle(), tr( "Please enter a valid web address (http or https)." ) );
		return;
	}

	m_webPage.title = m_titleEdit->text().trimmed();
	m_webPage.mode = m_notifyButton->isChecked() ? WebPageSenderPlugin::Mode::Notify :
					 m_shortcutButton->isChecked() ? WebPageSenderPlugin::Mode::DesktopShortcut :
													 WebPageSenderPlugin::Mode::Open;
	m_webPage.browser = WebPageSenderPlugin::Browser( m_browserComboBox->currentData().toInt() );
	m_webPage.newWindow = m_newWindowCheckBox->isChecked();
	m_webPage.fullScreen = m_fullScreenCheckBox->isEnabled() && m_fullScreenCheckBox->isChecked();

	// remember address and settings
	auto history = m_master.userConfigurationObject()->value( QLatin1String(HistoryKey), QLatin1String(ConfigParentKey), {} ).toStringList();
	history.removeAll( m_webPage.url.toString() );
	history.prepend( m_webPage.url.toString() );
	m_master.userConfigurationObject()->setValue( QLatin1String(HistoryKey), QStringList( history.mid( 0, MaximumHistorySize ) ),
												  QLatin1String(ConfigParentKey) );
	m_master.userConfigurationObject()->setValue( QLatin1String(SettingsKey), QVariantMap{
													  { QStringLiteral("mode"), int(m_webPage.mode) },
													  { QStringLiteral("browser"), int(m_webPage.browser) },
													  { QStringLiteral("newWindow"), m_webPage.newWindow },
													  { QStringLiteral("fullScreen"), m_webPage.fullScreen } },
												  QLatin1String(ConfigParentKey) );

	QDialog::accept();
}



void WebPageSenderDialog::updateOptions()
{
	const auto opensBrowser = m_shortcutButton->isChecked() == false;
	const auto specificBrowser = m_browserComboBox->currentData().toInt() != int(WebPageSenderPlugin::Browser::Default) &&
								 m_browserComboBox->currentData().toInt() != int(WebPageSenderPlugin::Browser::Safari);

	m_browserComboBox->setEnabled( opensBrowser );
	m_newWindowCheckBox->setEnabled( opensBrowser && specificBrowser );
	m_fullScreenCheckBox->setEnabled( opensBrowser && specificBrowser );
}
