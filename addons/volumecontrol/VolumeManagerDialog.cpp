/*
 * VolumeManagerDialog.cpp - implementation of VolumeManagerDialog class
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
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include "AddOnUtils.h"
#include "VeyonCore.h"
#include "VolumeManagerDialog.h"


namespace {

QSlider* createVolumeSlider()
{
	auto slider = new QSlider( Qt::Horizontal );
	slider->setRange( 0, 100 );
	slider->setSingleStep( 5 );
	slider->setPageStep( 10 );
	slider->setMinimumWidth( 180 );
	return slider;
}

QToolButton* createToggleButton( const QString& iconUrl, const QString& toolTip )
{
	auto button = new QToolButton;
	button->setIcon( VeyonCore::icon( iconUrl ) );
	button->setIconSize( QSize( 22, 22 ) );
	button->setCheckable( true );
	button->setAutoRaise( true );
	button->setToolTip( toolTip );
	return button;
}

}



VolumeManagerDialog::VolumeManagerDialog( VolumeControlPlugin& plugin,
										  const ComputerControlInterfaceList& computerControlInterfaces,
										  QWidget* parent ) :
	QDialog( parent ),
	m_plugin( plugin )
{
	setWindowTitle( tr( "Volume Manager" ) );
	setWindowIcon( VeyonCore::icon( QStringLiteral("veyon-volume") ) );
	resize( 640, 560 );

	auto mainLayout = new QVBoxLayout( this );

	// controls for all computers
	auto allGroup = new QGroupBox( tr( "All computers in this window" ) );
	auto allLayout = new QVBoxLayout( allGroup );

	auto sliderLayout = new QHBoxLayout;
	sliderLayout->addWidget( new QLabel( tr( "Volume" ) ) );
	m_allSlider = createVolumeSlider();
	m_allSlider->setValue( 50 );
	sliderLayout->addWidget( m_allSlider, 1 );
	m_allLevelLabel = new QLabel( QStringLiteral("50 %") );
	m_allLevelLabel->setMinimumWidth( 40 );
	sliderLayout->addWidget( m_allLevelLabel );
	auto setAllButton = new QPushButton( tr( "Set for all" ) );
	sliderLayout->addWidget( setAllButton );
	allLayout->addLayout( sliderLayout );

	connect( m_allSlider, &QSlider::valueChanged, this, [this]( int value ) {
		m_allLevelLabel->setText( QStringLiteral("%1 %").arg( value ) );
	} );
	connect( setAllButton, &QPushButton::clicked, this, [this]() {
		m_plugin.setLevel( connectedComputers(), m_allSlider->value() );
		QTimer::singleShot( 500, this, &VolumeManagerDialog::refresh );
	} );

	auto buttonLayout = new QHBoxLayout;
	const auto addButton = [&]( const QString& iconUrl, const QString& text, const std::function<void()>& handler ) {
		auto button = new QPushButton( VeyonCore::icon( iconUrl ), text );
		connect( button, &QPushButton::clicked, this, [this, handler]() {
			handler();
			QTimer::singleShot( 500, this, &VolumeManagerDialog::refresh );
		} );
		buttonLayout->addWidget( button );
	};
	addButton( QStringLiteral("veyon-volume-mute"), tr( "Mute all" ),
			   [this]() { m_plugin.setMuted( connectedComputers(), true ); } );
	addButton( QStringLiteral("veyon-volume"), tr( "Unmute all" ),
			   [this]() { m_plugin.setMuted( connectedComputers(), false ); } );
	addButton( QStringLiteral("veyon-volume-lock"), tr( "Lock all" ),
			   [this]() { m_plugin.lockCurrent( connectedComputers(), m_persistentCheckBox->isChecked() ); } );
	addButton( QStringLiteral("veyon-volume"), tr( "Unlock all" ),
			   [this]() { m_plugin.unlock( connectedComputers() ); } );
	allLayout->addLayout( buttonLayout );

	m_persistentCheckBox = new QCheckBox( tr( "Keep volume locked after the student computer has been restarted" ) );
	allLayout->addWidget( m_persistentCheckBox );

	mainLayout->addWidget( allGroup );

	// per computer controls
	auto scrollArea = new QScrollArea;
	scrollArea->setWidgetResizable( true );
	auto rowsWidget = new QWidget;
	m_rowsLayout = new QGridLayout( rowsWidget );
	m_rowsLayout->setColumnStretch( 1, 1 );
	scrollArea->setWidget( rowsWidget );
	mainLayout->addWidget( scrollArea, 1 );

	auto hintLabel = new QLabel( tr( "Locked computers are reset to the configured volume immediately "
									 "whenever students try to change it." ) );
	hintLabel->setWordWrap( true );
	mainLayout->addWidget( hintLabel );

	auto buttonBox = new QDialogButtonBox( QDialogButtonBox::Close );
	connect( buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject );
	mainLayout->addWidget( buttonBox );

	connect( &m_plugin, &VolumeControlPlugin::stateReceived, this, &VolumeManagerDialog::handleState );
	connect( &m_refreshTimer, &QTimer::timeout, this, &VolumeManagerDialog::refresh );
	m_refreshTimer.start( RefreshInterval );

	setComputerControlInterfaces( computerControlInterfaces );
}



VolumeManagerDialog::~VolumeManagerDialog()
{
	qDeleteAll( m_rows );
}



void VolumeManagerDialog::setComputerControlInterfaces( const ComputerControlInterfaceList& computerControlInterfaces )
{
	m_computerControlInterfaces = computerControlInterfaces;
	rebuildRows();
	refresh();
}



void VolumeManagerDialog::rebuildRows()
{
	qDeleteAll( m_rows );
	m_rows.clear();

	while( auto item = m_rowsLayout->takeAt( 0 ) )
	{
		delete item->widget();
		delete item;
	}

	int rowIndex = 0;
	for( const auto& computer : std::as_const(m_computerControlInterfaces) )
	{
		auto row = new Row;
		row->computer = computer;
		row->nameLabel = new QLabel( AddOnUtils::computerDisplayName( computer ) );
		row->slider = createVolumeSlider();
		row->slider->setEnabled( false );
		row->levelLabel = new QLabel( QStringLiteral("–") );
		row->levelLabel->setMinimumWidth( 40 );
		row->muteButton = createToggleButton( QStringLiteral("veyon-volume-mute"), tr( "Mute" ) );
		row->lockButton = createToggleButton( QStringLiteral("veyon-volume-lock"), tr( "Lock volume" ) );
		row->muteButton->setEnabled( false );
		row->lockButton->setEnabled( false );

		m_rowsLayout->addWidget( row->nameLabel, rowIndex, 0 );
		m_rowsLayout->addWidget( row->slider, rowIndex, 1 );
		m_rowsLayout->addWidget( row->levelLabel, rowIndex, 2 );
		m_rowsLayout->addWidget( row->muteButton, rowIndex, 3 );
		m_rowsLayout->addWidget( row->lockButton, rowIndex, 4 );

		const auto rowComputer = computer;
		connect( row->slider, &QSlider::valueChanged, this, [this, row]( int value ) {
			row->levelLabel->setText( QStringLiteral("%1 %").arg( value ) );
			row->lastInteraction.restart();
			if( row->slider->isSliderDown() == false )
			{
				m_plugin.setLevel( { row->computer }, value );
			}
		} );
		connect( row->slider, &QSlider::sliderReleased, this, [this, row]() {
			row->lastInteraction.restart();
			m_plugin.setLevel( { row->computer }, row->slider->value() );
		} );
		connect( row->muteButton, &QToolButton::clicked, this, [this, row]( bool checked ) {
			row->lastInteraction.restart();
			m_plugin.setMuted( { row->computer }, checked );
		} );
		connect( row->lockButton, &QToolButton::clicked, this, [this, row]( bool checked ) {
			row->lastInteraction.restart();
			if( checked )
			{
				m_plugin.lock( { row->computer }, row->slider->value(), row->muteButton->isChecked(),
							   m_persistentCheckBox->isChecked() );
			}
			else
			{
				m_plugin.unlock( { row->computer } );
			}
		} );

		m_rows.append( row );
		++rowIndex;
	}

	m_rowsLayout->setRowStretch( rowIndex, 1 );
}



void VolumeManagerDialog::refresh()
{
	const auto computers = connectedComputers();
	if( computers.isEmpty() == false )
	{
		m_plugin.queryState( computers );
	}

	for( auto row : std::as_const(m_rows) )
	{
		if( row->computer->state() != ComputerControlInterface::State::Connected )
		{
			row->slider->setEnabled( false );
			row->muteButton->setEnabled( false );
			row->lockButton->setEnabled( false );
			row->levelLabel->setText( tr( "offline" ) );
		}
	}
}



void VolumeManagerDialog::handleState( ComputerControlInterface::Pointer computerControlInterface,
									   const VolumeControlPlugin::VolumeState& state )
{
	auto row = findRow( computerControlInterface );
	if( row == nullptr )
	{
		return;
	}

	row->slider->setEnabled( state.available );
	row->muteButton->setEnabled( state.available );
	row->lockButton->setEnabled( state.available );

	if( state.available == false )
	{
		row->levelLabel->setText( tr( "no audio" ) );
		return;
	}

	// don't override values the user is currently changing
	if( row->slider->isSliderDown() ||
		( row->lastInteraction.isValid() && row->lastInteraction.elapsed() < UserInteractionHoldTime ) )
	{
		return;
	}

	row->slider->blockSignals( true );
	row->slider->setValue( state.level );
	row->slider->blockSignals( false );
	row->levelLabel->setText( QStringLiteral("%1 %").arg( state.level ) );
	row->muteButton->setChecked( state.muted );
	row->lockButton->setChecked( state.locked );
	row->nameLabel->setText( AddOnUtils::computerDisplayName( computerControlInterface ) +
							 ( state.locked ? QStringLiteral(" 🔒") : QString() ) );
}



VolumeManagerDialog::Row* VolumeManagerDialog::findRow( const ComputerControlInterface::Pointer& computerControlInterface )
{
	for( auto row : std::as_const(m_rows) )
	{
		if( row->computer == computerControlInterface )
		{
			return row;
		}
	}

	return nullptr;
}



ComputerControlInterfaceList VolumeManagerDialog::connectedComputers() const
{
	return AddOnUtils::connectedInterfaces( m_computerControlInterfaces );
}
