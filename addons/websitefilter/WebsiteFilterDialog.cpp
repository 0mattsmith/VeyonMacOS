/*
 * WebsiteFilterDialog.cpp - implementation of WebsiteFilterDialog class
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

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSplitter>
#include <QVBoxLayout>

#include "AddOnUtils.h"
#include "Configuration/Object.h"
#include "VeyonCore.h"
#include "VeyonMasterInterface.h"
#include "WebsiteFilterDialog.h"
#include "WebsiteFilterPlugin.h"


namespace {

struct BuiltinPreset
{
	const char* name;
	WebsiteRules::Mode mode;
	const char* domains;
};

static const BuiltinPreset builtinPresets[] = { // clazy:exclude=non-pod-global-static
	{ QT_TRANSLATE_NOOP("WebsiteFilterDialog", "Block social media"), WebsiteRules::Mode::Blacklist,
	  "facebook.com\ninstagram.com\ntiktok.com\nsnapchat.com\nx.com\ntwitter.com\nreddit.com\ndiscord.com\n"
	  "pinterest.com\ntumblr.com\nthreads.net\nbsky.app" },
	{ QT_TRANSLATE_NOOP("WebsiteFilterDialog", "Block video and games"), WebsiteRules::Mode::Blacklist,
	  "youtube.com\nyoutu.be\ntwitch.tv\nnetflix.com\ndisneyplus.com\nroblox.com\nminiclip.com\npoki.com\n"
	  "crazygames.com\ncoolmathgames.com\nfriv.com\nsteampowered.com\nepicgames.com" },
	{ QT_TRANSLATE_NOOP("WebsiteFilterDialog", "Block AI assistants"), WebsiteRules::Mode::Blacklist,
	  "chatgpt.com\nchat.openai.com\ngemini.google.com\nclaude.ai\ncopilot.microsoft.com\nperplexity.ai\n"
	  "poe.com\ncharacter.ai\nchat.deepseek.com\nmeta.ai\nchat.mistral.ai" },
	{ QT_TRANSLATE_NOOP("WebsiteFilterDialog", "Search engines and Wikipedia only"), WebsiteRules::Mode::Whitelist,
	  "google.com\ngstatic.com\ngoogleapis.com\nbing.com\nduckduckgo.com\nwikipedia.org\nwikimedia.org" },
};

constexpr auto PresetsKey = "Presets";
constexpr auto ConfigParentKey = "WebsiteFilter";

}



WebsiteFilterDialog::WebsiteFilterDialog( WebsiteFilterPlugin& plugin, VeyonMasterInterface& master,
										  const ComputerControlInterfaceList& computerControlInterfaces, QWidget* parent ) :
	QDialog( parent ),
	m_plugin( plugin ),
	m_master( master )
{
	setWindowTitle( tr( "Website Filter" ) );
	setWindowIcon( VeyonCore::icon( QStringLiteral("veyon-website-filter") ) );
	resize( 960, 640 );

	auto mainLayout = new QVBoxLayout( this );

	auto splitter = new QSplitter( Qt::Horizontal );

	auto computerPanel = new QWidget;
	auto computerLayout = new QVBoxLayout( computerPanel );
	computerLayout->setContentsMargins( 0, 0, 0, 0 );
	computerLayout->addWidget( new QLabel( tr( "Computers" ) ) );
	m_computerList = new QListWidget;
	m_computerList->setSelectionMode( QAbstractItemView::ExtendedSelection );
	m_computerList->setToolTip( tr( "The rules are applied to the selected computers. Select a single computer "
									"to view the rules which are active on it." ) );
	computerLayout->addWidget( m_computerList );
	auto selectAllButton = new QPushButton( tr( "Select all" ) );
	connect( selectAllButton, &QPushButton::clicked, m_computerList, &QListWidget::selectAll );
	computerLayout->addWidget( selectAllButton );
	splitter->addWidget( computerPanel );

	splitter->addWidget( createRulesPanel() );
	splitter->setStretchFactor( 0, 1 );
	splitter->setStretchFactor( 1, 3 );
	mainLayout->addWidget( splitter, 1 );

	auto buttonBox = new QDialogButtonBox( QDialogButtonBox::Close );
	connect( buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject );
	mainLayout->addWidget( buttonBox );

	connect( m_computerList, &QListWidget::currentRowChanged, this, [this]() {
		if( const auto computer = currentComputer() )
		{
			const auto state = m_states.value( AddOnUtils::computerKey( computer ) );
			if( state.active )
			{
				setRulesInUi( state.rules );
			}
		}
		updateStatus();
	} );
	connect( m_computerList, &QListWidget::itemSelectionChanged, this, &WebsiteFilterDialog::updateStatus );

	connect( &m_plugin, &WebsiteFilterPlugin::stateReceived, this, &WebsiteFilterDialog::handleState );

	loadPresets();
	updateOptions();
	setComputerControlInterfaces( computerControlInterfaces );
}



void WebsiteFilterDialog::setComputerControlInterfaces( const ComputerControlInterfaceList& computerControlInterfaces )
{
	m_computerControlInterfaces = computerControlInterfaces;
	updateComputerList();

	// apply rules to all computers of the current view by default
	m_computerList->selectAll();

	m_plugin.queryState( AddOnUtils::connectedInterfaces( m_computerControlInterfaces ) );
}



QWidget* WebsiteFilterDialog::createRulesPanel()
{
	auto panel = new QWidget;
	auto layout = new QVBoxLayout( panel );
	layout->setContentsMargins( 0, 0, 0, 0 );

	auto presetLayout = new QHBoxLayout;
	presetLayout->addWidget( new QLabel( tr( "Preset" ) ) );
	m_presetComboBox = new QComboBox;
	connect( m_presetComboBox, QOverload<int>::of( &QComboBox::activated ), this, &WebsiteFilterDialog::loadPreset );
	presetLayout->addWidget( m_presetComboBox, 1 );
	auto savePresetButton = new QPushButton( tr( "Save as preset…" ) );
	connect( savePresetButton, &QPushButton::clicked, this, &WebsiteFilterDialog::savePreset );
	presetLayout->addWidget( savePresetButton );
	auto deletePresetButton = new QPushButton( tr( "Delete preset" ) );
	connect( deletePresetButton, &QPushButton::clicked, this, &WebsiteFilterDialog::deletePreset );
	presetLayout->addWidget( deletePresetButton );
	layout->addLayout( presetLayout );

	auto modeGroup = new QGroupBox( tr( "Mode" ) );
	auto modeLayout = new QVBoxLayout( modeGroup );
	m_modeDisabledButton = new QRadioButton( tr( "No restrictions" ) );
	m_modeBlacklistButton = new QRadioButton( tr( "Blacklist: block the websites listed below" ) );
	m_modeWhitelistButton = new QRadioButton( tr( "Whitelist: allow only the websites listed below" ) );
	auto buttonGroup = new QButtonGroup( modeGroup );
	for( auto button : { m_modeDisabledButton, m_modeBlacklistButton, m_modeWhitelistButton } )
	{
		buttonGroup->addButton( button );
		modeLayout->addWidget( button );
		connect( button, &QRadioButton::toggled, this, &WebsiteFilterDialog::updateOptions );
	}
	m_modeBlacklistButton->setChecked( true );
	layout->addWidget( modeGroup );

	auto websitesGroup = new QGroupBox( tr( "Websites" ) );
	auto websitesLayout = new QVBoxLayout( websitesGroup );
	m_domainsHintLabel = new QLabel( tr( "One website per line, e.g. <tt>youtube.com</tt> or <tt>https://www.example.org/page</tt>. "
										 "Subdomains are included automatically." ) );
	m_domainsHintLabel->setWordWrap( true );
	websitesLayout->addWidget( m_domainsHintLabel );
	m_domainsEdit = new QPlainTextEdit;
	websitesLayout->addWidget( m_domainsEdit, 1 );
	layout->addWidget( websitesGroup, 1 );

	auto optionsGroup = new QGroupBox( tr( "Options" ) );
	auto optionsLayout = new QVBoxLayout( optionsGroup );
	m_hostsFileCheckBox = new QCheckBox( tr( "Also block the websites in other browsers and apps (hosts file)" ) );
	m_hostsFileCheckBox->setToolTip( tr( "Adds the websites to a clearly marked section of the hosts file. This also covers "
										 "browsers without policy support (e.g. Safari or Opera). Only available for blacklists." ) );
	optionsLayout->addWidget( m_hostsFileCheckBox );
	m_closeBrowsersCheckBox = new QCheckBox( tr( "Close web browsers so the rules take effect immediately" ) );
	m_closeBrowsersCheckBox->setToolTip( tr( "Web browsers are asked to quit, so students can restore their open tabs. "
											 "Firefox only applies changed rules after it has been restarted." ) );
	optionsLayout->addWidget( m_closeBrowsersCheckBox );
	m_keepAfterRestartCheckBox = new QCheckBox( tr( "Keep rules active after the student computer has been restarted" ) );
	optionsLayout->addWidget( m_keepAfterRestartCheckBox );
	layout->addWidget( optionsGroup );

	auto actionLayout = new QHBoxLayout;
	m_statusLabel = new QLabel;
	m_statusLabel->setWordWrap( true );
	actionLayout->addWidget( m_statusLabel, 1 );
	auto removeButton = new QPushButton( tr( "Remove rules" ) );
	removeButton->setToolTip( tr( "Remove the website rules from the selected computers and restore the previous browser settings" ) );
	connect( removeButton, &QPushButton::clicked, this, &WebsiteFilterDialog::removeRulesFromComputers );
	actionLayout->addWidget( removeButton );
	auto applyButton = new QPushButton( tr( "Apply to selected computers" ) );
	applyButton->setDefault( true );
	connect( applyButton, &QPushButton::clicked, this, &WebsiteFilterDialog::applyRulesToComputers );
	actionLayout->addWidget( applyButton );
	layout->addLayout( actionLayout );

	return panel;
}



void WebsiteFilterDialog::updateComputerList()
{
	const auto currentKey = AddOnUtils::computerKey( currentComputer() );
	QStringList selectedKeys;
	const auto selectedItems = m_computerList->selectedItems();
	for( const auto* item : selectedItems )
	{
		selectedKeys.append( item->data( Qt::UserRole ).toString() );
	}

	m_computerList->blockSignals( true );
	m_computerList->clear();
	for( const auto& computer : std::as_const(m_computerControlInterfaces) )
	{
		const auto key = AddOnUtils::computerKey( computer );
		const auto state = m_states.value( key );

		auto text = AddOnUtils::computerDisplayName( computer );
		if( state.error.isEmpty() == false )
		{
			text += tr( " – error" );
		}
		else if( state.active )
		{
			text += state.rules.mode == WebsiteRules::Mode::Blacklist ? tr( " – blacklist active" ) : tr( " – whitelist active" );
		}

		auto item = new QListWidgetItem( text );
		item->setData( Qt::UserRole, key );
		if( computer->state() != ComputerControlInterface::State::Connected )
		{
			item->setForeground( palette().color( QPalette::Disabled, QPalette::Text ) );
			item->setToolTip( tr( "This computer is not connected." ) );
		}
		else if( state.error.isEmpty() == false )
		{
			item->setToolTip( state.error );
		}
		if( state.active )
		{
			item->setIcon( VeyonCore::icon( QStringLiteral("veyon-website-filter") ) );
		}
		m_computerList->addItem( item );

		item->setSelected( selectedKeys.contains( key ) );
		if( key == currentKey )
		{
			m_computerList->setCurrentItem( item, QItemSelectionModel::NoUpdate );
		}
	}
	m_computerList->blockSignals( false );

	updateStatus();
}



ComputerControlInterface::Pointer WebsiteFilterDialog::currentComputer() const
{
	const auto item = m_computerList->currentItem();
	if( item == nullptr )
	{
		return {};
	}

	return AddOnUtils::findComputer( m_computerControlInterfaces, item->data( Qt::UserRole ).toString() );
}



ComputerControlInterfaceList WebsiteFilterDialog::selectedComputers() const
{
	ComputerControlInterfaceList computers;
	const auto items = m_computerList->selectedItems();
	for( const auto* item : items )
	{
		const auto computer = AddOnUtils::findComputer( m_computerControlInterfaces, item->data( Qt::UserRole ).toString() );
		if( computer )
		{
			computers.append( computer );
		}
	}

	return computers;
}



void WebsiteFilterDialog::updateStatus()
{
	const auto selected = selectedComputers();
	if( selected.size() == 1 )
	{
		const auto& computer = selected.first();
		const auto state = m_states.value( AddOnUtils::computerKey( computer ) );
		if( state.error.isEmpty() == false )
		{
			m_statusLabel->setText( tr( "The rules could not be applied completely on %1: %2" )
									.arg( computer->computerName(), state.error ) );
		}
		else if( state.active )
		{
			m_statusLabel->setText( state.rules.mode == WebsiteRules::Mode::Whitelist ?
										tr( "%1 can only open %n website(s).", "", int(state.rules.domains.size()) ).arg( computer->computerName() ) :
										tr( "%n website(s) blocked on %1.", "", int(state.rules.domains.size()) ).arg( computer->computerName() ) );
		}
		else
		{
			m_statusLabel->setText( tr( "No website rules active on %1." ).arg( computer->computerName() ) );
		}
		return;
	}

	int activeCount = 0;
	int errorCount = 0;
	for( const auto& computer : selected )
	{
		const auto state = m_states.value( AddOnUtils::computerKey( computer ) );
		activeCount += state.active ? 1 : 0;
		errorCount += state.error.isEmpty() ? 0 : 1;
	}

	auto text = tr( "%n computer(s) selected", "", int(selected.size()) );
	if( activeCount > 0 )
	{
		text += QStringLiteral(" · ") + tr( "rules active on %n", "", activeCount );
	}
	if( errorCount > 0 )
	{
		text += QStringLiteral(" · ") + tr( "errors on %n", "", errorCount );
	}
	m_statusLabel->setText( text );
}



WebsiteRules WebsiteFilterDialog::rulesFromUi() const
{
	WebsiteRules rules;
	rules.mode = m_modeBlacklistButton->isChecked() ? WebsiteRules::Mode::Blacklist :
				 m_modeWhitelistButton->isChecked() ? WebsiteRules::Mode::Whitelist :
													  WebsiteRules::Mode::Disabled;
	rules.domains = WebsiteRules::normalizedDomains( AddOnUtils::splitList( m_domainsEdit->toPlainText() ) );
	rules.useHostsFile = m_hostsFileCheckBox->isChecked() && rules.mode == WebsiteRules::Mode::Blacklist;
	rules.closeBrowsers = m_closeBrowsersCheckBox->isChecked();
	rules.keepAfterRestart = m_keepAfterRestartCheckBox->isChecked();
	return rules;
}



void WebsiteFilterDialog::setRulesInUi( const WebsiteRules& rules )
{
	switch( rules.mode )
	{
	case WebsiteRules::Mode::Blacklist: m_modeBlacklistButton->setChecked( true ); break;
	case WebsiteRules::Mode::Whitelist: m_modeWhitelistButton->setChecked( true ); break;
	case WebsiteRules::Mode::Disabled: m_modeDisabledButton->setChecked( true ); break;
	}

	m_domainsEdit->setPlainText( rules.domains.join( QLatin1Char('\n') ) );
	m_hostsFileCheckBox->setChecked( rules.useHostsFile );
	m_keepAfterRestartCheckBox->setChecked( rules.keepAfterRestart );
	updateOptions();
}



void WebsiteFilterDialog::updateOptions()
{
	const auto disabled = m_modeDisabledButton->isChecked();
	m_domainsEdit->setEnabled( disabled == false );
	m_hostsFileCheckBox->setEnabled( m_modeBlacklistButton->isChecked() );
	m_closeBrowsersCheckBox->setEnabled( disabled == false );
	m_keepAfterRestartCheckBox->setEnabled( disabled == false );
}



void WebsiteFilterDialog::applyRulesToComputers()
{
	const auto rules = rulesFromUi();
	const auto computers = AddOnUtils::connectedInterfaces( selectedComputers() );

	if( computers.isEmpty() )
	{
		QMessageBox::information( this, windowTitle(), tr( "Please select at least one connected computer." ) );
		return;
	}

	if( rules.mode == WebsiteRules::Mode::Disabled )
	{
		removeRulesFromComputers();
		return;
	}

	if( rules.mode == WebsiteRules::Mode::Blacklist && rules.domains.isEmpty() )
	{
		QMessageBox::information( this, windowTitle(), tr( "Please enter at least one website to block." ) );
		return;
	}

	if( rules.mode == WebsiteRules::Mode::Whitelist && rules.domains.isEmpty() &&
		QMessageBox::question( this, windowTitle(),
							   tr( "No websites are listed, so all websites will be blocked on %n computer(s) "
								   "(except websites always allowed by the administrator). Continue?", "", int(computers.size()) ) ) != QMessageBox::Yes )
	{
		return;
	}

	if( rules.closeBrowsers &&
		QMessageBox::question( this, windowTitle(),
							   tr( "Web browsers will be closed on %n computer(s). Students can restore their open tabs "
								   "afterwards, but unsaved form input may be lost. Continue?", "", int(computers.size()) ) ) != QMessageBox::Yes )
	{
		return;
	}

	for( const auto& computer : computers )
	{
		m_states[AddOnUtils::computerKey( computer )].error.clear();
	}

	m_plugin.setRules( computers, rules );
}



void WebsiteFilterDialog::removeRulesFromComputers()
{
	const auto computers = AddOnUtils::connectedInterfaces( selectedComputers() );
	if( computers.isEmpty() )
	{
		QMessageBox::information( this, windowTitle(), tr( "Please select at least one connected computer." ) );
		return;
	}

	m_plugin.clearRules( computers );
}



void WebsiteFilterDialog::loadPresets()
{
	m_presetComboBox->clear();
	m_presetComboBox->addItem( tr( "(choose a preset)" ) );

	for( const auto& preset : builtinPresets )
	{
		m_presetComboBox->addItem( tr( preset.name ), QVariantMap{
									   { QStringLiteral("mode"), int(preset.mode) },
									   { QStringLiteral("domains"), QString::fromUtf8( preset.domains ).split( QLatin1Char('\n') ) },
									   { QStringLiteral("builtin"), true } } );
	}

	const auto presets = userPresets();
	for( const auto& preset : presets )
	{
		const auto object = preset.toObject();
		m_presetComboBox->addItem( object[QStringLiteral("name")].toString(), object.toVariantMap() );
	}
}



void WebsiteFilterDialog::savePreset()
{
	bool ok = false;
	const auto name = QInputDialog::getText( this, tr( "Save preset" ), tr( "Name of the preset" ),
											 QLineEdit::Normal, {}, &ok ).trimmed();
	if( ok == false || name.isEmpty() )
	{
		return;
	}

	auto preset = QJsonObject::fromVariantMap( rulesFromUi().toVariantMap() );
	preset.remove( QStringLiteral("closeBrowsers") );
	preset[QStringLiteral("name")] = name;

	auto presets = userPresets();
	for( int i = 0; i < presets.size(); ++i )
	{
		if( presets[i].toObject()[QStringLiteral("name")].toString() == name )
		{
			presets.removeAt( i );
			break;
		}
	}
	presets.append( preset );
	setUserPresets( presets );

	loadPresets();
	m_presetComboBox->setCurrentIndex( m_presetComboBox->findText( name ) );
}



void WebsiteFilterDialog::deletePreset()
{
	const auto data = m_presetComboBox->currentData().toMap();
	if( data.isEmpty() || data.value( QStringLiteral("builtin") ).toBool() )
	{
		QMessageBox::information( this, windowTitle(), tr( "Built-in presets can't be deleted." ) );
		return;
	}

	const auto name = m_presetComboBox->currentText();
	auto presets = userPresets();
	for( int i = 0; i < presets.size(); ++i )
	{
		if( presets[i].toObject()[QStringLiteral("name")].toString() == name )
		{
			presets.removeAt( i );
			break;
		}
	}
	setUserPresets( presets );
	loadPresets();
}



void WebsiteFilterDialog::loadPreset( int index )
{
	const auto data = m_presetComboBox->itemData( index ).toMap();
	if( data.isEmpty() )
	{
		return;
	}

	auto rules = WebsiteRules::fromVariantMap( data );
	if( data.value( QStringLiteral("builtin") ).toBool() )
	{
		// built-in presets only define websites - keep the options chosen by the teacher
		rules.useHostsFile = m_hostsFileCheckBox->isChecked();
		rules.keepAfterRestart = m_keepAfterRestartCheckBox->isChecked();
	}
	setRulesInUi( rules );
}



QJsonArray WebsiteFilterDialog::userPresets() const
{
	return m_master.userConfigurationObject()->value( QLatin1String(PresetsKey), QLatin1String(ConfigParentKey), QJsonArray{} ).toJsonArray();
}



void WebsiteFilterDialog::setUserPresets( const QJsonArray& presets )
{
	m_master.userConfigurationObject()->setValue( QLatin1String(PresetsKey), presets, QLatin1String(ConfigParentKey) );
}



void WebsiteFilterDialog::handleState( ComputerControlInterface::Pointer computerControlInterface,
									   const WebsiteRules& rules, bool active, const QString& error )
{
	const auto key = AddOnUtils::computerKey( computerControlInterface );
	if( AddOnUtils::findComputer( m_computerControlInterfaces, key ).isNull() )
	{
		return;
	}

	auto& state = m_states[key];
	state.rules = rules;
	state.active = active;
	state.error = error;

	updateComputerList();
}
