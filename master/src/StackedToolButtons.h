/*
 * StackedToolButtons.h - declaration of StackedToolButtons class
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

#pragma once

#include <QMenu>
#include <QPointer>
#include <QWidget>

class QVBoxLayout;
class ToolButton;

// Toolbar slot with half-height tool buttons stacked on top of each other (small icon with the label beside it)
// and an optional drop-down arrow on the right which opens a menu with further functions
class StackedToolButtons : public QWidget
{
	Q_OBJECT
public:
	static constexpr int IconSize = 16;
	static constexpr int MenuArrowSize = 10;

	explicit StackedToolButtons( const QString& menuDescription, QWidget* parent = nullptr );
	~StackedToolButtons() override = default;

	void addButton( ToolButton* button );

	// shows a drop-down arrow for the given menu (should be a child of this widget) or hides it for nullptr -
	// a previously set menu is not deleted
	void setMenu( QMenu* menu );

	QMenu* menu() const
	{
		return m_menu;
	}

private:
	void showMenu();

	const QString m_menuDescription;
	QVBoxLayout* m_buttonLayout;
	ToolButton* m_menuButton{nullptr};
	QPointer<QMenu> m_menu;

} ;
