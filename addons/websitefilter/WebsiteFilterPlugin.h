/*
 * WebsiteFilterPlugin.h - declaration of WebsiteFilterPlugin class
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

#include <QPointer>

#include "AddOnInterface.h"
#include "ConfigurationPagePluginInterface.h"
#include "FeatureProviderInterface.h"
#include "VeyonServerInterface.h"
#include "WebsiteFilterBackend.h"
#include "WebsiteFilterConfiguration.h"
#include "WorkerIdleTimer.h"

class WebsiteFilterDialog;

/*!
 * \brief Blocks websites (blacklist) or allows selected websites only (whitelist) on student computers using
 * the enterprise policies of common web browsers and optionally the hosts file.
 */
class WebsiteFilterPlugin : public QObject, FeatureProviderInterface, PluginInterface, AddOnInterface,
		ConfigurationPagePluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.AddOns.WebsiteFilter")
	Q_INTERFACES(PluginInterface FeatureProviderInterface AddOnInterface ConfigurationPagePluginInterface)
public:
	enum class Command
	{
		SetRules,
		ClearRules,
		QueryState,
		State,
		CloseBrowsers,
	};
	Q_ENUM(Command)

	enum class Argument
	{
		Rules,
		Active,
		Error,
	};
	Q_ENUM(Argument)

	explicit WebsiteFilterPlugin( QObject* parent = nullptr );
	~WebsiteFilterPlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("3b8e5f1c-a742-4d69-9e0b-c6d2f8a41e73") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral( "WebsiteFilter" );
	}

	QString description() const override
	{
		return tr( "Block websites or allow selected websites only on student computers" );
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
		return tr( "Website Filter" );
	}

	Category addOnCategory() const override
	{
		return Category::Network;
	}

	bool revertSystemChanges( QString* errorString ) override;

	QString addOnIconUrl() const override
	{
		return QStringLiteral(":/websitefilter/website-filter.png");
	}

	QString addOnSummary() const override
	{
		return tr( "Block distracting websites (blacklist) or allow selected websites only (whitelist) on student "
				   "computers, with presets for social media, video, games and AI assistants. Works with Chrome, "
				   "Edge, Brave and Firefox using their enterprise policies; other browsers can be covered with the "
				   "hosts file. Previous policies are restored when the rules are removed." );
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

	bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message ) override;

	bool isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const override;

	ConfigurationPage* createConfigurationPage() override;

	void setRules( const ComputerControlInterfaceList& computerControlInterfaces, const WebsiteRules& rules );
	void clearRules( const ComputerControlInterfaceList& computerControlInterfaces );
	void queryState( const ComputerControlInterfaceList& computerControlInterfaces );

Q_SIGNALS:
	void stateReceived( ComputerControlInterface::Pointer computerControlInterface,
						const WebsiteRules& rules, bool active, const QString& error );

private:
	static constexpr auto StateName = "website-filter";
	static constexpr auto WorkerIdleTimeout = 60 * 1000;

	void initServer();
	bool applyRules( const WebsiteRules& rules, QString* errorString );
	void removeRules();
	void notifyStudent( const QString& message );
	void saveState();

	const Feature m_websiteFilterFeature;
	const Feature m_activeIndicatorFeature;
	const FeatureList m_features;

	WebsiteFilterConfiguration m_configuration;

	// server side
	bool m_serverInitialized{false};
	WebsiteFilterBackend m_backend;
	WebsiteRules m_rules;
	QPointer<VeyonServerInterface> m_server;

	// worker side
	WorkerIdleTimer m_idleTimer{WorkerIdleTimeout};

	// master side
	QPointer<WebsiteFilterDialog> m_dialog;

};
