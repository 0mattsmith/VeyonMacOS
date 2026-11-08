/*
 * SimpleConfigurationPage.cpp - generic configuration page for add-ons
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
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include "AddOnUtils.h"
#include "Configuration/UiMapping.h"
#include "SimpleConfigurationPage.h"


SimpleConfigurationPage::SimpleConfigurationPage( const QString& title, const QString& iconUrl, QWidget* parent ) :
	ConfigurationPage( parent ),
	m_layout( new QVBoxLayout( this ) )
{
	setWindowTitle( title );
	setWindowIcon( QIcon( iconUrl ) );
}



void SimpleConfigurationPage::addDescription( const QString& text )
{
	auto label = new QLabel( text );
	label->setWordWrap( true );
	m_layout->addWidget( label );
}



void SimpleConfigurationPage::addCheckBox( const Configuration::TypedProperty<bool>& property, const QString& text,
										   const QString& toolTip )
{
	auto checkBox = new QCheckBox( text );
	checkBox->setToolTip( toolTip );
	m_layout->addWidget( checkBox );

	const auto* propertyPointer = &property;
	m_resetFunctions.append( [=]() { Configuration::UiMapping::initWidgetFromProperty( *propertyPointer, checkBox ); } );
	m_connectFunctions.append( [=]() { Configuration::UiMapping::connectWidgetToProperty( *propertyPointer, checkBox ); } );
}



void SimpleConfigurationPage::addLineEdit( const Configuration::TypedProperty<QString>& property, const QString& label,
										   const QString& placeholder )
{
	m_layout->addWidget( new QLabel( label ) );
	auto lineEdit = new QLineEdit;
	lineEdit->setPlaceholderText( placeholder );
	m_layout->addWidget( lineEdit );

	const auto* propertyPointer = &property;
	m_resetFunctions.append( [=]() { Configuration::UiMapping::initWidgetFromProperty( *propertyPointer, lineEdit ); } );
	m_connectFunctions.append( [=]() { Configuration::UiMapping::connectWidgetToProperty( *propertyPointer, lineEdit ); } );
}



void SimpleConfigurationPage::addStringList( const Configuration::TypedProperty<QStringList>& property, const QString& label,
											 const QString& placeholder )
{
	m_layout->addWidget( new QLabel( label ) );
	auto textEdit = new QPlainTextEdit;
	textEdit->setPlaceholderText( placeholder );
	textEdit->setMaximumHeight( 120 );
	m_layout->addWidget( textEdit );

	const auto* propertyPointer = &property;
	m_resetFunctions.append( [=]() {
		textEdit->blockSignals( true );
		textEdit->setPlainText( propertyPointer->value().join( QLatin1Char('\n') ) );
		textEdit->blockSignals( false );
	} );
	m_connectFunctions.append( [=]() {
		connect( textEdit, &QPlainTextEdit::textChanged, textEdit, [=]() {
			propertyPointer->setValue( AddOnUtils::splitList( textEdit->toPlainText() ) );
		} );
	} );
}



void SimpleConfigurationPage::addStretch()
{
	m_layout->addStretch( 1 );
}



void SimpleConfigurationPage::resetWidgets()
{
	for( const auto& function : std::as_const(m_resetFunctions) )
	{
		function();
	}
}



void SimpleConfigurationPage::connectWidgetsToProperties()
{
	for( const auto& function : std::as_const(m_connectFunctions) )
	{
		function();
	}
}



void SimpleConfigurationPage::applyConfiguration()
{
}
