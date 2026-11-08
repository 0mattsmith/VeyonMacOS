/*
 * AddOnCommands.h - declaration of AddOnCommands class
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

#include <optional>

#include "AddOnManager.h"
#include "CommandLinePluginInterface.h"
#include "CommandLineIO.h"

class AddOnCommands : public QObject, CommandLinePluginInterface, PluginInterface, CommandLineIO
{
	Q_OBJECT
	Q_INTERFACES(PluginInterface CommandLinePluginInterface)
public:
	explicit AddOnCommands( QObject* parent = nullptr );
	~AddOnCommands() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("2b5c0f43-6b3f-4b8e-9a8c-0f2f6a1d77e2") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral( "AddOns" );
	}

	QString description() const override
	{
		return tr( "Add-on related CLI operations" );
	}

	QString vendor() const override
	{
		return QStringLiteral( "Veyon Community" );
	}

	QString copyright() const override
	{
		return QStringLiteral( "Veyon Community" );
	}

	QString commandLineModuleName() const override
	{
		return QStringLiteral( "addons" );
	}

	QString commandLineModuleHelp() const override
	{
		return tr( "Commands for managing add-ons" );
	}

	QStringList commands() const override;
	QString commandHelp( const QString& command ) const override;

public Q_SLOTS:
	CommandLinePluginInterface::RunResult handle_list( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_enable( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_disable( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_install( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_remove( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_info( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_pack( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_repositories( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_revert( const QStringList& arguments );

private:
	CommandLinePluginInterface::RunResult setEnabled( const QStringList& arguments, bool enabled );
	std::optional<AddOnManager::AddOn> findAddOn( const QString& identifier ) const;
	static CommandLinePluginInterface::RunResult operationError( const QString& message );

	AddOnManager m_manager{};
	const QMap<QString, QString> m_commands;

};
