/*
 * ScriptedAddOnsPlugin.h - declaration of ScriptedAddOnsPlugin class
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

#include <QJsonObject>

#include "AddOnInterface.h"
#include "ConfigurationPagePluginInterface.h"
#include "FeatureProviderInterface.h"
#include "ScriptedAddOnsConfiguration.h"
#include "WorkerIdleTimer.h"

/*!
 * \brief Runtime for script add-ons created with the add-on editor of the Add-On Store
 *
 * On the teacher's computer every installed script add-on is turned into a toolbar button (with a menu
 * if it contains multiple actions). The script for the operating system of each student computer is
 * sent along with the request, so student computers only need this plugin (no installed packages).
 * Students' computers can restrict scripts in the configuration (scripts with administrator privileges
 * are disabled by default).
 */
class ScriptedAddOnsPlugin : public QObject, FeatureProviderInterface, PluginInterface, AddOnInterface,
		ConfigurationPagePluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.AddOns.ScriptedAddOns")
	Q_INTERFACES(PluginInterface FeatureProviderInterface AddOnInterface ConfigurationPagePluginInterface)
public:
	enum class Command
	{
		RunScript,
		Result,
	};
	Q_ENUM(Command)

	enum class Argument
	{
		RequestId,
		AddOnName,
		ActionName,
		RunAs,
		Scripts,
		Input,
		ExitCode,
		Output,
		Error,
	};
	Q_ENUM(Argument)

	explicit ScriptedAddOnsPlugin( QObject* parent = nullptr );
	~ScriptedAddOnsPlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("d9e3b6a1-0f54-4c28-9a7d-3e6c1b8f4a95") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral( "ScriptedAddOns" );
	}

	QString description() const override
	{
		return tr( "Runtime for add-ons created from scripts" );
	}

	QString vendor() const override
	{
		return QStringLiteral( "Veyon Community" );
	}

	QString copyright() const override
	{
		return QStringLiteral( "Veyon Community" );
	}

	QString addOnName() const override
	{
		return tr( "Script Add-Ons" );
	}

	Category addOnCategory() const override
	{
		return Category::Scripting;
	}

	QString addOnIconUrl() const override
	{
		return QStringLiteral("veyon-scripted-addons");
	}

	QString addOnSummary() const override
	{
		return tr( "Required for custom add-ons created with \"Create add-on…\". Each custom add-on adds a button "
				   "to Veyon Master which runs a script (PowerShell, cmd, zsh, bash, AppleScript or Python) on the "
				   "selected student computers." );
	}

	const FeatureList& featureList() const override
	{
		return m_features;
	}

	bool controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						 const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool startFeature( VeyonMasterInterface& master, const Feature& feature,
					   const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
							   const FeatureMessage& message ) override;

	bool handleFeatureMessage( VeyonServerInterface& server,
							   const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

	bool handleFeatureMessageFromWorker( VeyonServerInterface& server, const FeatureMessage& message ) override;

	bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message ) override;

	ConfigurationPage* createConfigurationPage() override;

Q_SIGNALS:
	void resultReceived( ComputerControlInterface::Pointer computerControlInterface, const QUuid& requestId,
						 int exitCode, const QString& output, const QString& error );

private:
	struct Action
	{
		QJsonObject addOn;
		QJsonObject action;
	};

	static constexpr auto WorkerIdleTimeout = 5 * 60 * 1000;

	void loadScriptAddOns();
	QUuid runAction( const Action& action, const QString& input, const ComputerControlInterfaceList& computerControlInterfaces );
	FeatureMessage resultMessage( const FeatureMessage& request, const QString& error ) const;

	const Feature m_runScriptFeature;
	FeatureList m_features;
	QHash<Feature::Uid, Action> m_actions;

	ScriptedAddOnsConfiguration m_configuration;

	QHash<QUuid, MessageContext> m_pendingRequests;
	WorkerIdleTimer m_idleTimer{WorkerIdleTimeout};

};
