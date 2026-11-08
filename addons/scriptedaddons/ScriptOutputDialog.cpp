/*
 * ScriptOutputDialog.cpp - shows the results of a script add-on action
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

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QHeaderView>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "AddOnUtils.h"
#include "ScriptOutputDialog.h"
#include "ScriptedAddOnsPlugin.h"


ScriptOutputDialog::ScriptOutputDialog( ScriptedAddOnsPlugin& plugin, QUuid requestId, const QString& actionName,
										const ComputerControlInterfaceList& computerControlInterfaces, QWidget* parent ) :
	QDialog( parent ),
	m_requestId( requestId )
{
	setWindowTitle( tr( "Results of \"%1\"" ).arg( actionName ) );
	setWindowIcon( QIcon( QStringLiteral(":/scriptedaddons/scripted-addons.png") ) );
	resize( 800, 560 );

	auto layout = new QVBoxLayout( this );
	auto splitter = new QSplitter( Qt::Vertical );

	m_resultTree = new QTreeWidget;
	m_resultTree->setHeaderLabels( { tr( "Computer" ), tr( "Result" ) } );
	m_resultTree->setRootIsDecorated( false );
	m_resultTree->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
	connect( m_resultTree, &QTreeWidget::currentItemChanged, this, &ScriptOutputDialog::showSelectedOutput );
	splitter->addWidget( m_resultTree );

	m_outputEdit = new QPlainTextEdit;
	m_outputEdit->setReadOnly( true );
	m_outputEdit->setFont( QFontDatabase::systemFont( QFontDatabase::FixedFont ) );
	m_outputEdit->setPlaceholderText( tr( "Select a computer to see the output of the script." ) );
	splitter->addWidget( m_outputEdit );
	layout->addWidget( splitter, 1 );

	auto buttonBox = new QDialogButtonBox( QDialogButtonBox::Close );
	auto copyButton = buttonBox->addButton( tr( "Copy all results" ), QDialogButtonBox::ActionRole );
	connect( copyButton, &QPushButton::clicked, this, [this]() {
		QStringList text;
		for( int i = 0; i < m_resultTree->topLevelItemCount(); ++i )
		{
			const auto item = m_resultTree->topLevelItem( i );
			text.append( QStringLiteral("=== %1 (%2) ===\n%3").arg( item->text( 0 ), item->text( 1 ),
																	m_outputs.value( item->data( 0, Qt::UserRole ).toString() ) ) );
		}
		QApplication::clipboard()->setText( text.join( QStringLiteral("\n\n") ) );
	} );
	connect( buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject );
	layout->addWidget( buttonBox );

	for( const auto& computer : computerControlInterfaces )
	{
		auto item = new QTreeWidgetItem( m_resultTree, { AddOnUtils::computerDisplayName( computer ),
														 computer->state() == ComputerControlInterface::State::Connected ?
															 tr( "Running…" ) : tr( "Offline" ) } );
		item->setData( 0, Qt::UserRole, AddOnUtils::computerKey( computer ) );
	}

	connect( &plugin, &ScriptedAddOnsPlugin::resultReceived, this, &ScriptOutputDialog::handleResult );
}



void ScriptOutputDialog::handleResult( ComputerControlInterface::Pointer computerControlInterface, const QUuid& requestId,
									   int exitCode, const QString& output, const QString& error )
{
	if( requestId != m_requestId )
	{
		return;
	}

	const auto key = AddOnUtils::computerKey( computerControlInterface );
	m_outputs[key] = error.isEmpty() ? output : error + QStringLiteral("\n\n") + output;

	for( int i = 0; i < m_resultTree->topLevelItemCount(); ++i )
	{
		auto item = m_resultTree->topLevelItem( i );
		if( item->data( 0, Qt::UserRole ).toString() == key )
		{
			item->setText( 1, error.isEmpty() ? ( exitCode == 0 ? tr( "Success" ) : tr( "Exit code %1" ).arg( exitCode ) )
											  : tr( "Error" ) );
			item->setForeground( 1, error.isEmpty() && exitCode == 0 ? QColor( 0x2e, 0x8b, 0x3a ) : QColor( 0xc0, 0x39, 0x2b ) );
			if( m_resultTree->currentItem() == nullptr )
			{
				m_resultTree->setCurrentItem( item );
			}
			else if( m_resultTree->currentItem() == item )
			{
				showSelectedOutput();
			}
		}
	}
}



void ScriptOutputDialog::showSelectedOutput()
{
	const auto item = m_resultTree->currentItem();
	m_outputEdit->setPlainText( item ? m_outputs.value( item->data( 0, Qt::UserRole ).toString() ) : QString() );
}
