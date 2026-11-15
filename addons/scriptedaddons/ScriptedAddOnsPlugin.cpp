/*
 * ScriptedAddOnsPlugin.cpp - implementation of ScriptedAddOnsPlugin class
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

#include <QFile>
#include <QInputDialog>
#include <QJsonArray>
#include <QMessageBox>

#include "AddOnManager.h"
#include "AddOnUtils.h"
#include "FeatureWorkerManager.h"
#include "PlatformPluginInterface.h"
#include "PlatformSessionFunctions.h"
#include "ScriptOutputDialog.h"
#include "ScriptRunner.h"
#include "ScriptedAddOnsPlugin.h"
#include "SimpleConfigurationPage.h"
#include "VeyonConfiguration.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"


ScriptedAddOnsPlugin::ScriptedAddOnsPlugin( QObject* parent ) :
	QObject( parent ),
	// carries the scripts to student computers - not shown in Veyon Master
	m_runScriptFeature( QStringLiteral("RunScriptAction"),
						Feature::Flag::Meta | Feature::Flag::Service | Feature::Flag::Worker,
						Feature::Uid( "5e8c1f6a-3b27-4d94-a0e5-9f2b7c4d1e63" ),
						Feature::Uid(),
						tr( "Script add-on" ), {}, {} ),
	m_features( { m_runScriptFeature } ),
	m_configuration( &VeyonCore::config() )
{
	if( AddOnUtils::isMaster() )
	{
		loadScriptAddOns();
	}
}



bool ScriptedAddOnsPlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
										   const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( operation != Operation::Start || m_actions.contains( featureUid ) == false )
	{
		return false;
	}

	runAction( m_actions.value( featureUid ), arguments.value( QStringLiteral("input") ).toString(), computerControlInterfaces );

	return true;
}



bool ScriptedAddOnsPlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
										 const ComputerControlInterfaceList& computerControlInterfaces )
{
	const auto it = m_actions.constFind( feature.uid() );
	if( it == m_actions.constEnd() )
	{
		return false;
	}

	const auto& action = it.value();
	const auto actionName = action.action[QStringLiteral("name")].toString();

	auto interfaces = computerControlInterfaces;
	interfaces.removeLocalHostInterfaces();
	if( interfaces.isEmpty() )
	{
		interfaces = master.filteredComputerControlInterfaces();
		interfaces.removeLocalHostInterfaces();
	}

	QString input;
	const auto inputSpecification = action.action[QStringLiteral("input")].toObject();
	if( inputSpecification[QStringLiteral("enabled")].toBool() )
	{
		bool ok = false;
		auto label = inputSpecification[QStringLiteral("label")].toString();
		if( label.isEmpty() )
		{
			label = tr( "Input" );
		}
		input = QInputDialog::getText( master.mainWindow(), actionName, label, QLineEdit::Normal, {}, &ok );
		if( ok == false )
		{
			return true;
		}
	}

	if( action.action[QStringLiteral("confirm")].toBool() &&
		QMessageBox::question( master.mainWindow(), actionName,
							   tr( "Do you want to run \"%1\" on %n computer(s)?", "", int(interfaces.size()) ).arg( actionName ) )
		!= QMessageBox::Yes )
	{
		return true;
	}

	const auto requestId = runAction( action, input, interfaces );

	if( action.action[QStringLiteral("showOutput")].toBool() )
	{
		auto dialog = new ScriptOutputDialog( *this, requestId, actionName, interfaces, master.mainWindow() );
		dialog->setAttribute( Qt::WA_DeleteOnClose );
		dialog->show();
	}

	return true;
}



bool ScriptedAddOnsPlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
												 const FeatureMessage& message )
{
	if( message.featureUid() != m_runScriptFeature.uid() || message.command<Command>() != Command::Result )
	{
		return false;
	}

	Q_EMIT resultReceived( computerControlInterface,
						   message.argument( Argument::RequestId ).toUuid(),
						   message.argument( Argument::ExitCode ).toInt(),
						   message.argument( Argument::Output ).toString(),
						   message.argument( Argument::Error ).toString() );

	return true;
}



bool ScriptedAddOnsPlugin::handleFeatureMessage( VeyonServerInterface& server,
												 const MessageContext& messageContext,
												 const FeatureMessage& message )
{
	if( message.featureUid() != m_runScriptFeature.uid() || message.command<Command>() != Command::RunScript )
	{
		return false;
	}

	if( m_configuration.allowScripts() == false )
	{
		return server.sendFeatureMessageReply( messageContext,
											   resultMessage( message, tr( "Script add-ons have been disabled on this computer." ) ) );
	}

	const auto script = message.argument( Argument::Scripts ).toMap().value( AddOnManager::currentPlatform() ).toMap();
	if( script.isEmpty() )
	{
		return server.sendFeatureMessageReply( messageContext,
											   resultMessage( message, tr( "This action is not available for this operating system." ) ) );
	}

	if( message.argument( Argument::RunAs ).toString() == QLatin1String("system") )
	{
		if( m_configuration.allowSystemScripts() == false )
		{
			return server.sendFeatureMessageReply( messageContext,
												   resultMessage( message, tr( "Scripts with administrator privileges have not been "
																			   "allowed on this computer by the administrator." ) ) );
		}

		const auto featureUid = message.featureUid();
		const auto requestId = message.argument( Argument::RequestId );

		vInfo() << "running script add-on action" << message.argument( Argument::ActionName ).toString()
				<< "with administrator privileges";

		ScriptRunner::run( script.value( QStringLiteral("interpreter") ).toString(),
						   script.value( QStringLiteral("code") ).toString(),
						   message.argument( Argument::Input ).toString(),
						   message.argument( Argument::AddOnName ).toString(),
						   [&server, messageContext, featureUid, requestId]( const ScriptRunner::Result& result ) {
							   FeatureMessage reply( featureUid, Command::Result );
							   reply.addArgument( Argument::RequestId, requestId );
							   reply.addArgument( Argument::ExitCode, result.exitCode );
							   reply.addArgument( Argument::Output, result.output );
							   reply.addArgument( Argument::Error, result.error );
							   server.sendFeatureMessageReply( messageContext, reply );
						   } );
		return true;
	}

	if( VeyonCore::platform().sessionFunctions().currentSessionHasUser() == false )
	{
		return server.sendFeatureMessageReply( messageContext, resultMessage( message, tr( "No user is logged on at this computer." ) ) );
	}

	// scripts run as the logged on user by default
	m_pendingRequests[message.argument( Argument::RequestId ).toUuid()] = messageContext;
	server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );

	return true;
}



bool ScriptedAddOnsPlugin::handleFeatureMessageFromWorker( VeyonServerInterface& server, const FeatureMessage& message )
{
	if( message.featureUid() != m_runScriptFeature.uid() )
	{
		return false;
	}

	return server.sendFeatureMessageReply( m_pendingRequests.take( message.argument( Argument::RequestId ).toUuid() ), message );
}



bool ScriptedAddOnsPlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	if( message.featureUid() != m_runScriptFeature.uid() || message.command<Command>() != Command::RunScript )
	{
		return false;
	}

	if( m_configuration.allowScripts() == false )
	{
		return worker.sendFeatureMessageReply( resultMessage( message, tr( "Script add-ons have been disabled on this computer." ) ) );
	}

	const auto script = message.argument( Argument::Scripts ).toMap().value( AddOnManager::currentPlatform() ).toMap();
	const auto featureUid = message.featureUid();
	const auto requestId = message.argument( Argument::RequestId );

	// keep the worker running until the script has finished
	m_idleTimer.stop();

	ScriptRunner::run( script.value( QStringLiteral("interpreter") ).toString(),
					   script.value( QStringLiteral("code") ).toString(),
					   message.argument( Argument::Input ).toString(),
					   message.argument( Argument::AddOnName ).toString(),
					   [this, &worker, featureUid, requestId]( const ScriptRunner::Result& result ) {
						   FeatureMessage reply( featureUid, Command::Result );
						   reply.addArgument( Argument::RequestId, requestId );
						   reply.addArgument( Argument::ExitCode, result.exitCode );
						   reply.addArgument( Argument::Output, result.output );
						   reply.addArgument( Argument::Error, result.error );
						   worker.sendFeatureMessageReply( reply );
						   m_idleTimer.restart();
					   } );

	return true;
}



ConfigurationPage* ScriptedAddOnsPlugin::createConfigurationPage()
{
	auto page = new SimpleConfigurationPage( tr( "Script add-ons" ), QStringLiteral("veyon-scripted-addons") );
	page->addDescription( tr( "Script add-ons are created by teachers in the Add-On Store of Veyon Master. Their scripts "
							  "are sent to the student computers and run there when the teacher clicks the add-on's button." ) );
	page->addCheckBox( m_configuration.allowScriptsProperty(),
					   tr( "Allow script add-ons to run scripts as the logged on student on this computer" ) );
	page->addCheckBox( m_configuration.allowSystemScriptsProperty(),
					   tr( "Allow script add-ons to run scripts with administrator privileges on this computer" ),
					   tr( "Only enable this if all teachers who can access this computer are allowed to make "
						   "system-wide changes." ) );
	page->addStretch();
	return page;
}



void ScriptedAddOnsPlugin::loadScriptAddOns()
{
	const auto manifests = AddOnManager::installedManifests( QStringLiteral("script"), true );
	for( const auto& manifest : manifests )
	{
		const Plugin::Uid addOnUid{ manifest[QStringLiteral("uid")].toString() };
		const auto actions = manifest[QStringLiteral("actions")].toArray();
		if( addOnUid.isNull() || actions.isEmpty() )
		{
			continue;
		}

		AddOnManager::markScriptPackageLoaded( addOnUid );

		const auto iconFile = manifest[QStringLiteral("_location")].toString() + QStringLiteral("/icon.png");
		const auto iconUrl = QFile::exists( iconFile ) ? iconFile : QStringLiteral("veyon-scripted-addons");
		const auto addOnName = manifest[QStringLiteral("name")].toString();

		// add-ons with multiple actions get a toolbar button with a menu
		Feature::Uid parentUid;
		if( actions.size() > 1 )
		{
			parentUid = addOnUid;
			m_features.append( Feature( QStringLiteral("ScriptAddOn-%1").arg( manifest[QStringLiteral("id")].toString() ),
										Feature::Flag::Action | Feature::Flag::Master,
										parentUid, Feature::Uid(),
										addOnName, {}, manifest[QStringLiteral("description")].toString(), iconUrl ) );
		}

		for( const auto& actionValue : actions )
		{
			const auto action = actionValue.toObject();
			const Feature::Uid actionUid{ action[QStringLiteral("uid")].toString() };
			if( actionUid.isNull() || m_actions.contains( actionUid ) )
			{
				continue;
			}

			auto description = action[QStringLiteral("description")].toString();
			if( description.isEmpty() )
			{
				description = manifest[QStringLiteral("description")].toString();
			}

			m_features.append( Feature( QStringLiteral("ScriptAction-%1").arg( actionUid.toString( QUuid::WithoutBraces ) ),
										Feature::Flag::Action | Feature::Flag::Master,
										actionUid, parentUid,
										action[QStringLiteral("name")].toString(), {}, description, iconUrl ) );
			m_actions[actionUid] = { manifest, action };
		}
	}
}



QUuid ScriptedAddOnsPlugin::runAction( const Action& action, const QString& input,
									   const ComputerControlInterfaceList& computerControlInterfaces )
{
	const auto requestId = QUuid::createUuid();

	sendFeatureMessage( FeatureMessage( m_runScriptFeature.uid(), Command::RunScript )
						.addArgument( Argument::RequestId, requestId )
						.addArgument( Argument::AddOnName, action.addOn[QStringLiteral("name")].toString() )
						.addArgument( Argument::ActionName, action.action[QStringLiteral("name")].toString() )
						.addArgument( Argument::RunAs, action.action[QStringLiteral("runAs")].toString() )
						.addArgument( Argument::Scripts, action.action[QStringLiteral("scripts")].toObject().toVariantMap() )
						.addArgument( Argument::Input, input ),
						computerControlInterfaces );

	return requestId;
}



FeatureMessage ScriptedAddOnsPlugin::resultMessage( const FeatureMessage& request, const QString& error ) const
{
	FeatureMessage reply( m_runScriptFeature.uid(), Command::Result );
	reply.addArgument( Argument::RequestId, request.argument( Argument::RequestId ) );
	reply.addArgument( Argument::ExitCode, -1 );
	reply.addArgument( Argument::Error, error );
	return FeatureMessage( reply );
}


IMPLEMENT_CONFIG_PROXY(ScriptedAddOnsConfiguration)
