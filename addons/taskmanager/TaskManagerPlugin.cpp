/*
 * TaskManagerPlugin.cpp - implementation of TaskManagerPlugin class
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

#include "AddOnState.h"
#include "AddOnUtils.h"
#include "BuiltinFeatures.h"
#include "FeatureWorkerManager.h"
#include "ProcessBackend.h"
#include "SystemTrayIcon.h"
#include "TaskManagerDialog.h"
#include "TaskManagerPlugin.h"
#include "VeyonMasterInterface.h"


TaskManagerPlugin::TaskManagerPlugin( QObject* parent ) :
	QObject( parent ),
	m_taskManagerFeature( QStringLiteral("TaskManager"),
						  Feature::Flag::Action | Feature::Flag::AllComponents,
						  Feature::Uid( "d2f7a6b1-0c3e-4e59-8a47-1b9f6c2d7e30" ),
						  Feature::Uid(),
						  tr( "Tasks" ), {},
						  tr( "Open the remote task manager to view and end programs on student computers "
							  "or to allow and block applications." ),
						  QStringLiteral(":/taskmanager/taskmanager.png") ),
	// only used to signal active application rules on computer tiles
	m_applicationRulesFeature( QStringLiteral("ApplicationRules"),
							   Feature::Flag::Meta | Feature::Flag::AllComponents,
							   Feature::Uid( "6e1b9c47-28d5-4f3a-b0e2-9a7c4d5f1b86" ),
							   Feature::Uid(),
							   tr( "App rules" ), {},
							   tr( "Application rules are active" ),
							   QStringLiteral(":/taskmanager/application-control.png") ),
	m_features( { m_taskManagerFeature, m_applicationRulesFeature } )
{
	if( AddOnUtils::isServer() )
	{
		// initialize after VeyonCore has been set up completely
		QTimer::singleShot( 0, this, &TaskManagerPlugin::initServer );
	}
}



TaskManagerPlugin::~TaskManagerPlugin()
{
	delete m_backend;
}



bool TaskManagerPlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
										const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( featureUid != m_taskManagerFeature.uid() )
	{
		return false;
	}

	// support for veyon-cli feature start/stop and the WebAPI
	if( operation == Operation::Start )
	{
		const auto patterns = arguments.value( QStringLiteral("terminate") ).toStringList();
		if( patterns.isEmpty() == false )
		{
			terminateByName( computerControlInterfaces, patterns );
		}

		if( arguments.contains( QStringLiteral("rules") ) )
		{
			setRules( computerControlInterfaces, ApplicationRules::fromVariantMap( arguments.value( QStringLiteral("rules") ).toMap() ) );
		}
		return true;
	}

	if( operation == Operation::Stop )
	{
		clearRules( computerControlInterfaces );
		return true;
	}

	return false;
}



bool TaskManagerPlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
									  const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( feature != m_taskManagerFeature )
	{
		return false;
	}

	auto interfaces = computerControlInterfaces;
	interfaces.removeLocalHostInterfaces();
	if( interfaces.isEmpty() )
	{
		interfaces = master.filteredComputerControlInterfaces();
		interfaces.removeLocalHostInterfaces();
	}

	if( m_dialog )
	{
		m_dialog->setComputerControlInterfaces( interfaces );
		m_dialog->raise();
		m_dialog->activateWindow();
		return true;
	}

	m_dialog = new TaskManagerDialog( *this, master, interfaces, master.mainWindow() );
	m_dialog->setAttribute( Qt::WA_DeleteOnClose );
	m_dialog->show();

	return true;
}



bool TaskManagerPlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
											  const FeatureMessage& message )
{
	if( message.featureUid() != m_taskManagerFeature.uid() )
	{
		return false;
	}

	switch( message.command<Command>() )
	{
	case Command::ProcessList:
	{
		ProcessInfoList processes;
		const auto processList = message.argument( Argument::Processes ).toList();
		processes.reserve( processList.size() );
		for( const auto& process : processList )
		{
			processes.append( ProcessInfo::fromVariantMap( process.toMap() ) );
		}
		Q_EMIT processListReceived( computerControlInterface, message.argument( Argument::Platform ).toString(), processes );
		Q_EMIT rulesStateReceived( computerControlInterface,
								   ApplicationRules::fromVariantMap( message.argument( Argument::Rules ).toMap() ),
								   message.argument( Argument::BlockedApplications ).toList() );
		return true;
	}

	case Command::RulesState:
		Q_EMIT rulesStateReceived( computerControlInterface,
								   ApplicationRules::fromVariantMap( message.argument( Argument::Rules ).toMap() ),
								   message.argument( Argument::BlockedApplications ).toList() );
		return true;

	case Command::TerminationResult:
		Q_EMIT terminationResultReceived( computerControlInterface,
										  message.argument( Argument::TerminatedCount ).toInt(),
										  message.argument( Argument::FailedNames ).toStringList() );
		return true;

	default:
		break;
	}

	return false;
}



bool TaskManagerPlugin::handleFeatureMessage( VeyonServerInterface& server,
											  const MessageContext& messageContext,
											  const FeatureMessage& message )
{
	if( message.featureUid() != m_taskManagerFeature.uid() )
	{
		return false;
	}

	m_server = &server;

	if( m_backend == nullptr )
	{
		initServer();
	}

	switch( message.command<Command>() )
	{
	case Command::QueryProcesses:
	{
		QVariantList processes;
		const auto processList = m_backend->processes( ProcessBackend::Detail::Full );
		processes.reserve( processList.size() );
		for( const auto& process : processList )
		{
			processes.append( process.toVariantMap() );
		}

		auto reply = rulesStateMessage();
		reply.setCommand( Command::ProcessList );
		reply.addArgument( Argument::Processes, processes );
		reply.addArgument( Argument::Platform, ProcessBackend::platform() );
		return server.sendFeatureMessageReply( messageContext, reply );
	}

	case Command::TerminateProcesses:
	{
		QList<qint64> pids;
		const auto pidList = message.argument( Argument::Pids ).toList();
		for( const auto& pid : pidList )
		{
			pids.append( pid.toLongLong() );
		}

		QStringList failedNames;
		const auto count = terminateProcesses( pids, message.argument( Argument::Force ).toBool(), &failedNames );

		return server.sendFeatureMessageReply( messageContext,
											   FeatureMessage( m_taskManagerFeature.uid(), Command::TerminationResult )
											   .addArgument( Argument::TerminatedCount, count )
											   .addArgument( Argument::FailedNames, failedNames ) );
	}

	case Command::TerminateByName:
	{
		const auto patterns = message.argument( Argument::Patterns ).toStringList();
		QList<qint64> pids;
		const auto processes = m_backend->processes( ProcessBackend::Detail::Basic );
		for( const auto& process : processes )
		{
			ApplicationRules matcher;
			matcher.patterns = patterns;
			if( process.isUserProcess && m_backend->isCriticalProcess( process ) == false && matcher.matches( process ) )
			{
				pids.append( process.pid );
			}
		}

		QStringList failedNames;
		const auto count = terminateProcesses( pids, true, &failedNames );

		return server.sendFeatureMessageReply( messageContext,
											   FeatureMessage( m_taskManagerFeature.uid(), Command::TerminationResult )
											   .addArgument( Argument::TerminatedCount, count )
											   .addArgument( Argument::FailedNames, failedNames ) );
	}

	case Command::SetRules:
		applyRules( ApplicationRules::fromVariantMap( message.argument( Argument::Rules ).toMap() ) );
		return server.sendFeatureMessageReply( messageContext, rulesStateMessage() );

	case Command::ClearRules:
		applyRules( {} );
		return server.sendFeatureMessageReply( messageContext, rulesStateMessage() );

	case Command::QueryRules:
		return server.sendFeatureMessageReply( messageContext, rulesStateMessage() );

	default:
		break;
	}

	return false;
}



void TaskManagerPlugin::sendAsyncFeatureMessages( VeyonServerInterface& server, const MessageContext& messageContext )
{
	Q_UNUSED(messageContext)

	// remember server instance so students can be notified about blocked applications
	m_server = &server;
}



bool TaskManagerPlugin::isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const
{
	Q_UNUSED(server)

	return featureUid == m_applicationRulesFeature.uid() && m_rules.isActive();
}



void TaskManagerPlugin::queryProcesses( const ComputerControlInterface::Pointer& computerControlInterface )
{
	computerControlInterface->sendFeatureMessage( FeatureMessage( m_taskManagerFeature.uid(), Command::QueryProcesses ) );
}



void TaskManagerPlugin::terminateProcesses( const ComputerControlInterface::Pointer& computerControlInterface,
											const QList<qint64>& pids, bool force )
{
	QVariantList pidList;
	for( const auto pid : pids )
	{
		pidList.append( pid );
	}

	computerControlInterface->sendFeatureMessage( FeatureMessage( m_taskManagerFeature.uid(), Command::TerminateProcesses )
												  .addArgument( Argument::Pids, pidList )
												  .addArgument( Argument::Force, force ) );
}



void TaskManagerPlugin::terminateByName( const ComputerControlInterfaceList& computerControlInterfaces, const QStringList& patterns )
{
	sendFeatureMessage( FeatureMessage( m_taskManagerFeature.uid(), Command::TerminateByName )
						.addArgument( Argument::Patterns, patterns ), computerControlInterfaces );
}



void TaskManagerPlugin::setRules( const ComputerControlInterfaceList& computerControlInterfaces, const ApplicationRules& rules )
{
	sendFeatureMessage( FeatureMessage( m_taskManagerFeature.uid(), Command::SetRules )
						.addArgument( Argument::Rules, rules.toVariantMap() ), computerControlInterfaces );
}



void TaskManagerPlugin::clearRules( const ComputerControlInterfaceList& computerControlInterfaces )
{
	sendFeatureMessage( FeatureMessage( m_taskManagerFeature.uid(), Command::ClearRules ), computerControlInterfaces );
}



void TaskManagerPlugin::queryRules( const ComputerControlInterfaceList& computerControlInterfaces )
{
	sendFeatureMessage( FeatureMessage( m_taskManagerFeature.uid(), Command::QueryRules ), computerControlInterfaces );
}



void TaskManagerPlugin::initServer()
{
	if( m_backend )
	{
		return;
	}

	m_backend = new ProcessBackend;

	connect( &m_enforcementTimer, &QTimer::timeout, this, &TaskManagerPlugin::enforceRules );

	// restore rules which should survive restarts
	const auto state = AddOnState::loadRuntimeState( QLatin1String(StateName) );
	if( state.isEmpty() == false )
	{
		applyRules( ApplicationRules::fromJson( state ) );
	}
}



void TaskManagerPlugin::enforceRules()
{
	if( m_backend == nullptr || m_rules.isActive() == false )
	{
		return;
	}

	const auto processes = m_backend->processes( ProcessBackend::Detail::Basic );
	for( const auto& process : processes )
	{
		if( process.isUserProcess == false ||
			m_backend->isSystemProcess( process ) ||
			m_rules.isAllowed( process ) )
		{
			continue;
		}

		if( m_backend->terminate( process.pid, true ) )
		{
			vDebug() << "terminated blocked application" << process.name << process.pid;

			m_blockedApplications.prepend( QVariantMap{
				{ QStringLiteral("time"), QDateTime::currentDateTime() },
				{ QStringLiteral("name"), process.displayName() },
				{ QStringLiteral("path"), process.path },
			} );
			while( m_blockedApplications.size() > MaximumBlockedApplications )
			{
				m_blockedApplications.removeLast();
			}

			notifyBlockedApplication( process );
		}
	}
}



void TaskManagerPlugin::applyRules( const ApplicationRules& rules )
{
	m_rules = rules;
	++m_rulesVersion;

	// rules survive restarts of the server (logging off and on) and optionally reboots
	if( m_rules.isActive() )
	{
		AddOnState::saveRuntimeState( QLatin1String(StateName), m_rules.toJson(), m_rules.persistent );
	}
	else
	{
		AddOnState::remove( QLatin1String(StateName) );
	}

	if( m_rules.isActive() )
	{
		m_enforcementTimer.start( EnforcementInterval );
		enforceRules();
	}
	else
	{
		m_enforcementTimer.stop();
		m_blockedApplications.clear();
	}
}



void TaskManagerPlugin::notifyBlockedApplication( const ProcessInfo& process )
{
	if( m_server.isNull() )
	{
		return;
	}

	const auto name = process.displayName();
	const auto now = QDateTime::currentDateTime();
	const auto lastNotification = m_lastNotifications.value( name );
	if( lastNotification.isValid() && lastNotification.secsTo( now ) < NotificationThrottleInterval )
	{
		return;
	}
	m_lastNotifications[name] = now;

	const auto message = m_rules.message.isEmpty() ?
							 tr( "The application \"%1\" is not allowed at the moment and has been closed by your teacher." ).arg( name )
						   : m_rules.message;

	VeyonCore::builtinFeatures().systemTrayIcon().showMessage( tr( "Application blocked" ), message,
															  m_server->featureWorkerManager() );
}



FeatureMessage TaskManagerPlugin::rulesStateMessage() const
{
	FeatureMessage message( m_taskManagerFeature.uid(), Command::RulesState );
	message.addArgument( Argument::Rules, m_rules.toVariantMap() );
	message.addArgument( Argument::BlockedApplications, m_blockedApplications );

	// FeatureMessage has an explicit copy constructor
	return FeatureMessage( message );
}



int TaskManagerPlugin::terminateProcesses( const QList<qint64>& pids, bool force, QStringList* failedNames )
{
	int count = 0;

	const auto processes = m_backend->processes( ProcessBackend::Detail::Basic );

	for( const auto pid : pids )
	{
		const auto it = std::find_if( processes.cbegin(), processes.cend(),
									  [pid]( const ProcessInfo& p ) { return p.pid == pid; } );
		if( it == processes.cend() )
		{
			continue;
		}

		if( m_backend->isCriticalProcess( *it ) )
		{
			failedNames->append( it->name );
			continue;
		}

		if( m_backend->terminate( pid, force ) == false )
		{
			failedNames->append( it->name );
			continue;
		}

		++count;

		if( force == false )
		{
			// kill the process if it did not quit within the timeout (e.g. because of a save dialog)
			QTimer::singleShot( GracefulTerminationTimeout, this, [this, pid]() {
				if( m_backend && m_backend->isRunning( pid ) )
				{
					m_backend->terminate( pid, true );
				}
			} );
		}
	}

	return count;
}
