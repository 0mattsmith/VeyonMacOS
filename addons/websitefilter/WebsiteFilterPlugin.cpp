/*
 * WebsiteFilterPlugin.cpp - implementation of WebsiteFilterPlugin class
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

#include <QTimer>

#include "AddOnState.h"
#include "AddOnUtils.h"
#include "BuiltinFeatures.h"
#include "FeatureWorkerManager.h"
#include "SimpleConfigurationPage.h"
#include "SystemTrayIcon.h"
#include "VeyonConfiguration.h"
#include "VeyonMasterInterface.h"
#include "VeyonWorkerInterface.h"
#include "WebsiteFilterDialog.h"
#include "WebsiteFilterPlugin.h"


WebsiteFilterPlugin::WebsiteFilterPlugin( QObject* parent ) :
	QObject( parent ),
	m_websiteFilterFeature( QStringLiteral("WebsiteFilter"),
							Feature::Flag::Action | Feature::Flag::AllComponents,
							Feature::Uid( "9c4f2a7e-61d8-4b35-a0e9-5f3b8d2c7a16" ),
							Feature::Uid(),
							tr( "Websites" ), {},
							tr( "Block websites or allow selected websites only" ),
							QStringLiteral("veyon-website-filter") ),
	m_activeIndicatorFeature( QStringLiteral("WebsiteFilterActiveIndicator"),
							  Feature::Flag::Meta | Feature::Flag::AllComponents,
							  Feature::Uid( "e5a9c3d1-2f84-4b67-8d1e-a6c0b7f94e28" ),
							  Feature::Uid(),
							  tr( "Websites filtered" ), {}, {},
							  QStringLiteral("veyon-website-filter") ),
	m_features( { m_websiteFilterFeature, m_activeIndicatorFeature } ),
	m_configuration( &VeyonCore::config() )
{
	if( AddOnUtils::isServer() )
	{
		QTimer::singleShot( 0, this, &WebsiteFilterPlugin::initServer );
	}
}



bool WebsiteFilterPlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
										  const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( featureUid != m_websiteFilterFeature.uid() )
	{
		return false;
	}

	// support for veyon-cli feature start/stop and the WebAPI
	if( operation == Operation::Start )
	{
		setRules( computerControlInterfaces, WebsiteRules::fromVariantMap( arguments ) );
		return true;
	}

	if( operation == Operation::Stop )
	{
		clearRules( computerControlInterfaces );
		return true;
	}

	return false;
}



bool WebsiteFilterPlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
										const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( feature != m_websiteFilterFeature )
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

	m_dialog = new WebsiteFilterDialog( *this, master, interfaces, master.mainWindow() );
	m_dialog->setAttribute( Qt::WA_DeleteOnClose );
	m_dialog->show();

	return true;
}



bool WebsiteFilterPlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
												const FeatureMessage& message )
{
	if( message.featureUid() != m_websiteFilterFeature.uid() || message.command<Command>() != Command::State )
	{
		return false;
	}

	Q_EMIT stateReceived( computerControlInterface,
						  WebsiteRules::fromVariantMap( message.argument( Argument::Rules ).toMap() ),
						  message.argument( Argument::Active ).toBool(),
						  message.argument( Argument::Error ).toString() );

	return true;
}



bool WebsiteFilterPlugin::handleFeatureMessage( VeyonServerInterface& server,
												const MessageContext& messageContext,
												const FeatureMessage& message )
{
	if( message.featureUid() != m_websiteFilterFeature.uid() )
	{
		return false;
	}

	m_server = &server;
	initServer();

	QString error;

	switch( message.command<Command>() )
	{
	case Command::SetRules:
	{
		const auto rules = WebsiteRules::fromVariantMap( message.argument( Argument::Rules ).toMap() );
		if( rules.isActive() == false )
		{
			if( m_rules.isActive() )
			{
				removeRules();
				notifyStudent( tr( "Your teacher has removed the website restrictions." ) );
			}
			break;
		}

		if( applyRules( rules, &error ) )
		{
			notifyStudent( rules.mode == WebsiteRules::Mode::Whitelist ?
							   tr( "Your teacher has restricted web browsing to selected websites." ) :
							   tr( "Your teacher has blocked some websites." ) );

			// browsers have to be closed in the session of the student
			if( rules.closeBrowsers )
			{
				server.featureWorkerManager().sendMessageToUnmanagedSessionWorker(
					FeatureMessage( m_websiteFilterFeature.uid(), Command::CloseBrowsers ) );
			}
		}
		break;
	}

	case Command::ClearRules:
		if( m_rules.isActive() )
		{
			removeRules();
			notifyStudent( tr( "Your teacher has removed the website restrictions." ) );
		}
		break;

	case Command::QueryState:
		break;

	default:
		return false;
	}

	FeatureMessage reply( m_websiteFilterFeature.uid(), Command::State );
	reply.addArgument( Argument::Rules, m_rules.toVariantMap() );
	reply.addArgument( Argument::Active, m_rules.isActive() );
	reply.addArgument( Argument::Error, error );

	return server.sendFeatureMessageReply( messageContext, reply );
}



bool WebsiteFilterPlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	Q_UNUSED(worker)

	if( message.featureUid() != m_websiteFilterFeature.uid() )
	{
		return false;
	}

	m_idleTimer.restart();

	if( message.command<Command>() == Command::CloseBrowsers )
	{
		WebsiteFilterBackend::closeBrowsers();
	}

	return true;
}



bool WebsiteFilterPlugin::isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const
{
	Q_UNUSED(server)

	return featureUid == m_activeIndicatorFeature.uid() && m_rules.isActive();
}



bool WebsiteFilterPlugin::revertSystemChanges( QString* errorString )
{
	Q_UNUSED(errorString)

	// clear() also removes the section managed by Veyon from the hosts file if no state has been saved
	WebsiteFilterBackend backend;
	backend.setState( AddOnState::load( QLatin1String(StateName) )[QStringLiteral("state")].toObject()[QStringLiteral("backend")].toObject() );
	backend.clear();
	AddOnState::remove( QLatin1String(StateName) );

	return true;
}



ConfigurationPage* WebsiteFilterPlugin::createConfigurationPage()
{
	auto page = new SimpleConfigurationPage( tr( "Website Filter" ), QStringLiteral("veyon-website-filter") );
	page->addDescription( tr( "Teachers can block websites or allow selected websites only. The rules are applied as "
							  "browser policies for Google Chrome, Microsoft Edge, Brave, Chromium and Mozilla Firefox. "
							  "Policies configured before (e.g. via Group Policy) are restored when the rules are removed." ) );
	page->addStringList( m_configuration.alwaysAllowedProperty(),
						 tr( "Websites which are always allowed when only selected websites are allowed (one per line):" ),
						 tr( "e.g. your learning platform or your school's website" ) );
	page->addCheckBox( m_configuration.notifyStudentsProperty(),
					   tr( "Notify students when website restrictions are changed" ) );
	page->addStretch();
	return page;
}



void WebsiteFilterPlugin::setRules( const ComputerControlInterfaceList& computerControlInterfaces, const WebsiteRules& rules )
{
	sendFeatureMessage( FeatureMessage( m_websiteFilterFeature.uid(), Command::SetRules )
						.addArgument( Argument::Rules, rules.toVariantMap() ),
						computerControlInterfaces );
}



void WebsiteFilterPlugin::clearRules( const ComputerControlInterfaceList& computerControlInterfaces )
{
	sendFeatureMessage( FeatureMessage( m_websiteFilterFeature.uid(), Command::ClearRules ), computerControlInterfaces );
}



void WebsiteFilterPlugin::queryState( const ComputerControlInterfaceList& computerControlInterfaces )
{
	sendFeatureMessage( FeatureMessage( m_websiteFilterFeature.uid(), Command::QueryState ), computerControlInterfaces );
}



void WebsiteFilterPlugin::initServer()
{
	if( m_serverInitialized )
	{
		return;
	}
	m_serverInitialized = true;

	// the backup of previous browser policies is needed for cleaning up
	const auto rawState = AddOnState::load( QLatin1String(StateName) );
	m_backend.setState( rawState[QStringLiteral("state")].toObject()[QStringLiteral("backend")].toObject() );

	const auto state = AddOnState::loadRuntimeState( QLatin1String(StateName) );
	if( state.isEmpty() == false )
	{
		auto rules = WebsiteRules::fromVariantMap( state[QStringLiteral("rules")].toObject().toVariantMap() );
		rules.closeBrowsers = false;

		QString error;
		if( rules.isActive() && applyRules( rules, &error ) == false )
		{
			vWarning() << "could not restore website rules:" << error;
		}
	}
	else if( rawState.isEmpty() == false )
	{
		// rules of a previous boot which were not meant to be kept - restore the previous browser policies
		m_backend.clear();
		AddOnState::remove( QLatin1String(StateName) );
	}
}



bool WebsiteFilterPlugin::applyRules( const WebsiteRules& rules, QString* errorString )
{
	auto effectiveRules = rules;
	if( rules.mode == WebsiteRules::Mode::Whitelist )
	{
		effectiveRules.domains = WebsiteRules::normalizedDomains( rules.domains + m_configuration.alwaysAllowed() );
	}

	// save the state first so previous policies can be restored even if applying the rules fails partially
	m_rules = rules;
	const auto success = m_backend.apply( effectiveRules, errorString );
	saveState();

	if( success == false )
	{
		vWarning() << "could not apply website rules:" << *errorString;
	}

	return success;
}



void WebsiteFilterPlugin::removeRules()
{
	m_backend.clear();
	m_rules = {};
	AddOnState::remove( QLatin1String(StateName) );
}



void WebsiteFilterPlugin::notifyStudent( const QString& message )
{
	if( m_server && m_configuration.notifyStudents() )
	{
		VeyonCore::builtinFeatures().systemTrayIcon().showMessage( tr( "Websites" ), message,
																   m_server->featureWorkerManager() );
	}
}



void WebsiteFilterPlugin::saveState()
{
	AddOnState::saveRuntimeState( QLatin1String(StateName),
								  QJsonObject{ { QStringLiteral("rules"), QJsonObject::fromVariantMap( m_rules.toVariantMap() ) },
											   { QStringLiteral("backend"), m_backend.state() } },
								  m_rules.keepAfterRestart );
}


IMPLEMENT_CONFIG_PROXY(WebsiteFilterConfiguration)
