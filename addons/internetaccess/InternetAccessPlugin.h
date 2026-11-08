/*
 * InternetAccessPlugin.h - declaration of InternetAccessPlugin class
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
#include <QTimer>

#include "AddOnInterface.h"
#include "ConfigurationPagePluginInterface.h"
#include "FeatureProviderInterface.h"
#include "FirewallBackend.h"
#include "InternetAccessConfiguration.h"
#include "VeyonServerInterface.h"

/*!
 * \brief Pauses internet access of student computers while the local network (and thus Veyon) keeps working.
 * Websites can be excluded by the teacher (per lesson) and by the administrator (always allowed).
 */
class InternetAccessPlugin : public QObject, FeatureProviderInterface, PluginInterface, AddOnInterface,
		ConfigurationPagePluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.AddOns.InternetAccess")
	Q_INTERFACES(PluginInterface FeatureProviderInterface AddOnInterface ConfigurationPagePluginInterface)
public:
	enum class Command
	{
		Block,
		Unblock,
		QueryState,
		State,
	};
	Q_ENUM(Command)

	enum class Argument
	{
		AllowedHosts,
		KeepAfterRestart,
		Blocked,
		Error,
	};
	Q_ENUM(Argument)

	explicit InternetAccessPlugin( QObject* parent = nullptr );
	~InternetAccessPlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("6a9d4e2b-1c78-4f35-8b0a-d3e5f7c19a42") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral( "InternetAccess" );
	}

	QString description() const override
	{
		return tr( "Pause and resume internet access of student computers" );
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
		return tr( "Internet Access" );
	}

	Category addOnCategory() const override
	{
		return Category::Network;
	}

	bool revertSystemChanges( QString* errorString ) override;

	QString addOnIconUrl() const override
	{
		return QStringLiteral(":/internetaccess/internet-access.png");
	}

	QString addOnSummary() const override
	{
		return tr( "Pause the internet access of student computers while the local network keeps working, e.g. during "
				   "tests. Allow selected websites for a lesson; administrators can configure websites and networks "
				   "which are always allowed. Uses the firewall of the operating system." );
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

	bool isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const override;

	ConfigurationPage* createConfigurationPage() override;

	void block( const ComputerControlInterfaceList& computerControlInterfaces, const QStringList& allowedHosts,
				bool keepAfterRestart );
	void unblock( const ComputerControlInterfaceList& computerControlInterfaces );

private:
	static constexpr auto RefreshInterval = 10 * 60 * 1000;
	static constexpr auto ErrorCollectionDelay = 1500;
	static constexpr auto StateName = "internet-access";

	void initServer();
	bool applyBlock( const QStringList& allowedHosts, bool keepAfterRestart, QString* errorString );
	void removeBlock();
	void refreshBlock();
	void notifyStudent( const QString& message );
	void saveState();
	void showCollectedErrors();

	const Feature m_internetFeature;
	const Feature m_blockFeature;
	const Feature m_blockWithExceptionsFeature;
	const Feature m_allowFeature;
	const Feature m_blockedIndicatorFeature;
	const FeatureList m_features;

	InternetAccessConfiguration m_configuration;

	// server side
	bool m_serverInitialized{false};
	FirewallBackend m_backend;
	bool m_blocked{false};
	QStringList m_allowedHosts;
	bool m_keepAfterRestart{false};
	QTimer m_refreshTimer{this};
	QPointer<VeyonServerInterface> m_server;

	// master side
	QStringList m_collectedErrors;
	QPointer<QWidget> m_mainWindow;

};
