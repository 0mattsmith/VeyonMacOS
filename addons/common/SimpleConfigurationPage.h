/*
 * SimpleConfigurationPage.h - generic configuration page for add-ons
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

#include <functional>

#include "ConfigurationPage.h"
#include "Configuration/Property.h"

class QVBoxLayout;

//! \brief Configuration page which is assembled from simple widgets mapped to configuration properties
class SimpleConfigurationPage : public ConfigurationPage
{
	Q_OBJECT
public:
	SimpleConfigurationPage( const QString& title, const QString& iconUrl, QWidget* parent = nullptr );
	~SimpleConfigurationPage() override = default;

	void addDescription( const QString& text );
	void addCheckBox( const Configuration::TypedProperty<bool>& property, const QString& text, const QString& toolTip = {} );
	void addLineEdit( const Configuration::TypedProperty<QString>& property, const QString& label, const QString& placeholder = {} );
	void addStringList( const Configuration::TypedProperty<QStringList>& property, const QString& label, const QString& placeholder = {} );
	void addStretch();

	void resetWidgets() override;
	void connectWidgetsToProperties() override;
	void applyConfiguration() override;

private:
	QVBoxLayout* m_layout{nullptr};
	QList<std::function<void()>> m_resetFunctions;
	QList<std::function<void()>> m_connectFunctions;

};
