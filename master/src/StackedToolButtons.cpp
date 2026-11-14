/*
 * StackedToolButtons.cpp - implementation of StackedToolButtons class
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

#include <QHBoxLayout>
#include <QMenu>
#include <QVBoxLayout>

#include "StackedToolButtons.h"
#include "ToolButton.h"


StackedToolButtons::StackedToolButtons( const QString& menuDescription, QWidget* parent ) :
	QWidget( parent ),
	m_menuDescription( menuDescription ),
	m_buttonLayout( new QVBoxLayout )
{
	auto layout = new QHBoxLayout( this );
	layout->setContentsMargins( 0, 0, 0, 0 );
	layout->setSpacing( 0 );

	m_buttonLayout->setContentsMargins( 0, 0, 0, 0 );
	m_buttonLayout->setSpacing( 0 );
	layout->addLayout( m_buttonLayout );

	// natural width, but use the full height of the toolbar row
	setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Preferred );
}



void StackedToolButtons::addButton( ToolButton* button )
{
	button->setLabelStyle( Qt::ToolButtonTextBesideIcon );
	button->setIconSize( QSize( IconSize, IconSize ) );
	button->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Expanding );

	// left-aligned with their natural width so that the icons line up
	m_buttonLayout->addWidget( button, 0, Qt::AlignLeft );
}



void StackedToolButtons::setMenu( QMenu* menu )
{
	m_menu = menu;

	if( menu == nullptr )
	{
		if( m_menuButton )
		{
			m_menuButton->hide();
		}
		return;
	}

	if( m_menuButton == nullptr )
	{
		m_menuButton = new ToolButton( QIcon(), {}, {}, m_menuDescription );
		m_menuButton->setObjectName( objectName() + QStringLiteral("MenuButton") );
		m_menuButton->setLabelStyle( Qt::ToolButtonIconOnly );
		m_menuButton->setArrowType( Qt::DownArrow );
		m_menuButton->setIconSize( QSize( MenuArrowSize, MenuArrowSize ) );
		m_menuButton->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Expanding );
		connect( m_menuButton, &QToolButton::clicked, this, &StackedToolButtons::showMenu );
		layout()->addWidget( m_menuButton );
	}

	m_menuButton->show();
}



void StackedToolButtons::showMenu()
{
	if( m_menu )
	{
		// open below the whole slot like the menus of regular tool buttons
		const auto x = isRightToLeft() ? width() - m_menu->sizeHint().width() : 0;
		m_menu->popup( mapToGlobal( QPoint( x, height() ) ) );
	}
}
