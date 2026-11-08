/*
 * TaskManagerPlugin.h - declaration of TaskManagerPlugin class
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

#include <QDateTime>
#include <QPointer>
#include <QTimer>

#include "AddOnInterface.h"
#include "ApplicationRules.h"
#include "FeatureProviderInterface.h"
#include "VeyonServerInterface.h"

class ProcessBackend;
class TaskManagerDialog;

class TaskManagerPlugin : public QObject, FeatureProviderInterface, PluginInterface, AddOnInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.AddOns.TaskManager")
	Q_INTERFACES(PluginInterface FeatureProviderInterface AddOnInterface)
public:
	enum class Command
	{
		QueryProcesses,
		ProcessList,
		TerminateProcesses,
		TerminateByName,
		TerminationResult,
		SetRules,
		ClearRules,
		QueryRules,
		RulesState,
	};
	Q_ENUM(Command)

	enum class Argument
	{
		Processes,
		Platform,
		Pids,
		Force,
		Patterns,
		Rules,
		BlockedApplications,
		TerminatedCount,
		FailedNames,
		UserName,
	};
	Q_ENUM(Argument)

	explicit TaskManagerPlugin( QObject* parent = nullptr );
	~TaskManagerPlugin() override;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("a3c5d1e2-6f47-4b8a-9d21-5e0c7b3f8a14") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral( "TaskManager" );
	}

	QString description() const override
	{
		return tr( "View and end running programs on student computers and allow or block applications" );
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
		return tr( "Remote Task Manager" );
	}

	Category addOnCategory() const override
	{
		return Category::Control;
	}

	QString addOnIconUrl() const override
	{
		return QStringLiteral(":/taskmanager/taskmanager.png");
	}

	QString addOnSummary() const override
	{
		return tr( "Shows the programs running on student computers including CPU and memory usage and lets you "
				   "end them. App rules block listed applications (blacklist) or allow only listed applications "
				   "(whitelist) and are enforced continuously, even after a restart if desired." );
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

	void sendAsyncFeatureMessages( VeyonServerInterface& server, const MessageContext& messageContext ) override;

	bool isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const override;

	// master side API used by TaskManagerDialog
	void queryProcesses( const ComputerControlInterface::Pointer& computerControlInterface );
	void terminateProcesses( const ComputerControlInterface::Pointer& computerControlInterface,
							 const QList<qint64>& pids, bool force );
	void terminateByName( const ComputerControlInterfaceList& computerControlInterfaces, const QStringList& patterns );
	void setRules( const ComputerControlInterfaceList& computerControlInterfaces, const ApplicationRules& rules );
	void clearRules( const ComputerControlInterfaceList& computerControlInterfaces );
	void queryRules( const ComputerControlInterfaceList& computerControlInterfaces );

Q_SIGNALS:
	void processListReceived( ComputerControlInterface::Pointer computerControlInterface,
							  const QString& platform, const ProcessInfoList& processes );
	void rulesStateReceived( ComputerControlInterface::Pointer computerControlInterface,
							 const ApplicationRules& rules, const QVariantList& blockedApplications );
	void terminationResultReceived( ComputerControlInterface::Pointer computerControlInterface,
									int terminatedCount, const QStringList& failedNames );

private:
	static constexpr auto EnforcementInterval = 1000;
	static constexpr auto GracefulTerminationTimeout = 5000;
	static constexpr auto MaximumBlockedApplications = 50;
	static constexpr auto NotificationThrottleInterval = 30;
	static constexpr auto StateName = "taskmanager-rules";

	void initServer();
	void enforceRules();
	void applyRules( const ApplicationRules& rules );
	void notifyBlockedApplication( const ProcessInfo& process );
	FeatureMessage rulesStateMessage() const;
	int terminateProcesses( const QList<qint64>& pids, bool force, QStringList* failedNames );

	const Feature m_taskManagerFeature;
	const Feature m_applicationRulesFeature;
	const FeatureList m_features;

	// server side state
	ProcessBackend* m_backend{nullptr};
	ApplicationRules m_rules;
	QTimer m_enforcementTimer{this};
	QVariantList m_blockedApplications;
	QHash<QString, QDateTime> m_lastNotifications;
	QPointer<VeyonServerInterface> m_server;
	int m_rulesVersion{0};

	QPointer<TaskManagerDialog> m_dialog;

};
