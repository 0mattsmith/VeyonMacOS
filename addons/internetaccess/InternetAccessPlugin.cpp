/*
 * InternetAccessPlugin.cpp - implementation of InternetAccessPlugin class
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

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include "AddOnState.h"
#include "AddOnUtils.h"
#include "BuiltinFeatures.h"
#include "Configuration/Object.h"
#include "FeatureWorkerManager.h"
#include "InternetAccessPlugin.h"
#include "NetworkRanges.h"
#include "SimpleConfigurationPage.h"
#include "SystemTrayIcon.h"
#include "VeyonConfiguration.h"
#include "VeyonMasterInterface.h"


InternetAccessPlugin::InternetAccessPlugin( QObject* parent ) :
	QObject( parent ),
	m_internetFeature( QStringLiteral("InternetAccess"),
					   Feature::Flag::Action | Feature::Flag::AllComponents,
					   Feature::Uid( "b1e7c3a9-5d42-4f86-9c0e-2a8d6f4b1e57" ),
					   Feature::Uid(),
					   tr( "Internet" ), {},
					   tr( "Pause or resume internet access of student computers" ),
					   QStringLiteral(":/internetaccess/internet-access.png") ),
	m_blockFeature( QStringLiteral("BlockInternet"),
					Feature::Flag::Action | Feature::Flag::Master,
					Feature::Uid( "f3a8d2c6-7e19-4b54-a6d0-8c1e5b9f2a43" ),
					m_internetFeature.uid(),
					tr( "Pause internet access" ), {},
					tr( "Block internet access of the selected computers - the local network keeps working" ),
					QStringLiteral(":/internetaccess/internet-block.png") ),
	m_blockWithExceptionsFeature( QStringLiteral("BlockInternetWithExceptions"),
								  Feature::Flag::Action | Feature::Flag::Master,
								  Feature::Uid( "0c6e9b4d-2a73-4e18-b5f9-7d3a1c8e6b24" ),
								  m_internetFeature.uid(),
								  tr( "Pause internet access except…" ), {},
								  tr( "Block internet access but allow selected websites" ),
								  QStringLiteral(":/internetaccess/internet-block.png") ),
	m_allowFeature( QStringLiteral("AllowInternet"),
					Feature::Flag::Action | Feature::Flag::Master,
					Feature::Uid( "8e2d5a1f-c467-4b93-a0e8-3f6b9d2c7a15" ),
					m_internetFeature.uid(),
					tr( "Resume internet access" ), {},
					tr( "Allow internet access on the selected computers again" ),
					QStringLiteral(":/internetaccess/internet-access.png") ),
	m_blockedIndicatorFeature( QStringLiteral("InternetBlockedIndicator"),
							   Feature::Flag::Meta | Feature::Flag::AllComponents,
							   Feature::Uid( "4d7a1e8c-9b36-4f02-8e5d-c2a6b9f3e174" ),
							   Feature::Uid(),
							   tr( "Internet paused" ), {}, {},
							   QStringLiteral(":/internetaccess/internet-block.png") ),
	m_features( { m_internetFeature, m_blockFeature, m_blockWithExceptionsFeature, m_allowFeature, m_blockedIndicatorFeature } ),
	m_configuration( &VeyonCore::config() )
{
	if( AddOnUtils::isServer() )
	{
		QTimer::singleShot( 0, this, &InternetAccessPlugin::initServer );
	}
}



bool InternetAccessPlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
										   const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( featureUid == m_blockFeature.uid() || ( featureUid == m_internetFeature.uid() && operation == Operation::Start ) )
	{
		block( computerControlInterfaces, arguments.value( QStringLiteral("allowed") ).toStringList(),
			   arguments.value( QStringLiteral("keepAfterRestart") ).toBool() );
		return true;
	}

	if( featureUid == m_allowFeature.uid() || ( featureUid == m_internetFeature.uid() && operation == Operation::Stop ) )
	{
		unblock( computerControlInterfaces );
		return true;
	}

	return false;
}



bool InternetAccessPlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
										 const ComputerControlInterfaceList& computerControlInterfaces )
{
	m_mainWindow = master.mainWindow();

	auto interfaces = computerControlInterfaces;
	interfaces.removeLocalHostInterfaces();
	if( interfaces.isEmpty() )
	{
		interfaces = master.filteredComputerControlInterfaces();
		interfaces.removeLocalHostInterfaces();
	}

	if( feature == m_blockFeature )
	{
		block( interfaces, {}, false );
		return true;
	}

	if( feature == m_allowFeature )
	{
		unblock( interfaces );
		return true;
	}

	if( feature != m_blockWithExceptionsFeature )
	{
		return false;
	}

	const auto configKey = QStringLiteral("LastAllowedWebsites");
	const auto configParentKey = QStringLiteral("InternetAccess");
	auto userConfig = master.userConfigurationObject();

	QDialog dialog( master.mainWindow() );
	dialog.setWindowTitle( m_blockWithExceptionsFeature.displayName() );
	dialog.setWindowIcon( QIcon( m_blockWithExceptionsFeature.iconUrl() ) );
	dialog.resize( 520, 420 );
	auto layout = new QVBoxLayout( &dialog );
	auto label = new QLabel( tr( "Internet access will be paused on %n computer(s). The following websites stay "
								 "available (one per line, e.g. www.example.com):", "", int(interfaces.size()) ) );
	label->setWordWrap( true );
	layout->addWidget( label );
	auto allowedEdit = new QPlainTextEdit( userConfig->value( configKey, configParentKey, {} ).toStringList().join( QLatin1Char('\n') ) );
	layout->addWidget( allowedEdit, 1 );
	auto hintLabel = new QLabel( tr( "Note: websites which load content from other domains (e.g. video platforms) may need "
									 "these domains to be allowed as well." ) );
	hintLabel->setWordWrap( true );
	layout->addWidget( hintLabel );
	auto keepCheckBox = new QCheckBox( tr( "Keep internet access paused after the student computer has been restarted" ) );
	layout->addWidget( keepCheckBox );
	auto buttonBox = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel );
	connect( buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
	connect( buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
	layout->addWidget( buttonBox );

	if( dialog.exec() == QDialog::Accepted )
	{
		const auto allowedHosts = AddOnUtils::splitList( allowedEdit->toPlainText() );
		userConfig->setValue( configKey, allowedHosts, configParentKey );
		block( interfaces, allowedHosts, keepCheckBox->isChecked() );
	}

	return true;
}



bool InternetAccessPlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
												 const FeatureMessage& message )
{
	if( message.featureUid() != m_internetFeature.uid() || message.command<Command>() != Command::State )
	{
		return false;
	}

	const auto error = message.argument( Argument::Error ).toString();
	if( error.isEmpty() == false )
	{
		if( m_collectedErrors.isEmpty() )
		{
			QTimer::singleShot( ErrorCollectionDelay, this, &InternetAccessPlugin::showCollectedErrors );
		}
		m_collectedErrors.append( QStringLiteral("%1: %2").arg( computerControlInterface->computerName(), error ) );
	}

	return true;
}



bool InternetAccessPlugin::handleFeatureMessage( VeyonServerInterface& server,
												 const MessageContext& messageContext,
												 const FeatureMessage& message )
{
	if( message.featureUid() != m_internetFeature.uid() )
	{
		return false;
	}

	m_server = &server;
	initServer();

	QString error;

	switch( message.command<Command>() )
	{
	case Command::Block:
		if( applyBlock( message.argument( Argument::AllowedHosts ).toStringList(),
						message.argument( Argument::KeepAfterRestart ).toBool(), &error ) )
		{
			notifyStudent( tr( "Internet access has been paused by your teacher. The local network is still available." ) );
		}
		break;

	case Command::Unblock:
		if( m_blocked )
		{
			removeBlock();
			notifyStudent( tr( "Internet access is available again." ) );
		}
		break;

	case Command::QueryState:
		break;

	default:
		return false;
	}

	FeatureMessage reply( m_internetFeature.uid(), Command::State );
	reply.addArgument( Argument::Blocked, m_blocked );
	reply.addArgument( Argument::Error, error );

	return server.sendFeatureMessageReply( messageContext, reply );
}



bool InternetAccessPlugin::isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const
{
	Q_UNUSED(server)

	return featureUid == m_blockedIndicatorFeature.uid() && m_blocked;
}



bool InternetAccessPlugin::revertSystemChanges( QString* errorString )
{
	// the state is needed for restoring firewall profiles which have been turned on
	FirewallBackend backend;
	backend.setState( AddOnState::load( QLatin1String(StateName) )[QStringLiteral("state")].toObject()[QStringLiteral("backend")].toObject() );
	const auto success = backend.unblock( errorString );
	AddOnState::remove( QLatin1String(StateName) );

	return success;
}



ConfigurationPage* InternetAccessPlugin::createConfigurationPage()
{
	auto page = new SimpleConfigurationPage( tr( "Internet Access" ), QStringLiteral(":/internetaccess/internet-access.png") );
	page->addDescription( tr( "Teachers can pause the internet access of this computer. The local network (private address "
							  "ranges) always stays available so Veyon keeps working." ) );
	page->addStringList( m_configuration.alwaysAllowedProperty(),
						 tr( "Websites and networks which are always allowed (one per line):" ),
						 tr( "e.g. www.example.com, 203.0.113.0/24 or the address of your proxy server" ) );
	page->addCheckBox( m_configuration.notifyStudentsProperty(),
					   tr( "Notify students when internet access is paused or resumed" ) );
	page->addCheckBox( m_configuration.enableWindowsFirewallProperty(),
					   tr( "Turn on Windows Firewall while internet access is paused if it is turned off (Windows only)" ),
					   tr( "Internet access can only be paused while Windows Firewall is turned on. Veyon turns it off "
						   "again when internet access is resumed." ) );
	page->addStretch();
	return page;
}



void InternetAccessPlugin::block( const ComputerControlInterfaceList& computerControlInterfaces, const QStringList& allowedHosts,
								  bool keepAfterRestart )
{
	sendFeatureMessage( FeatureMessage( m_internetFeature.uid(), Command::Block )
						.addArgument( Argument::AllowedHosts, allowedHosts )
						.addArgument( Argument::KeepAfterRestart, keepAfterRestart ),
						computerControlInterfaces );
}



void InternetAccessPlugin::unblock( const ComputerControlInterfaceList& computerControlInterfaces )
{
	sendFeatureMessage( FeatureMessage( m_internetFeature.uid(), Command::Unblock ), computerControlInterfaces );
}



void InternetAccessPlugin::initServer()
{
	if( m_serverInitialized )
	{
		return;
	}
	m_serverInitialized = true;

	connect( &m_refreshTimer, &QTimer::timeout, this, &InternetAccessPlugin::refreshBlock );

	// the backend state (e.g. firewall profiles which have been turned on) is needed for cleaning up
	const auto rawState = AddOnState::load( QLatin1String(StateName) );
	m_backend.setState( rawState[QStringLiteral("state")].toObject()[QStringLiteral("backend")].toObject() );

	const auto state = AddOnState::loadRuntimeState( QLatin1String(StateName) );
	if( state.isEmpty() == false )
	{
		QString error;
		QStringList allowedHosts;
		const auto hosts = state[QStringLiteral("allowedHosts")].toArray();
		for( const auto& host : hosts )
		{
			allowedHosts.append( host.toString() );
		}

		if( applyBlock( allowedHosts, state[QStringLiteral("keepAfterRestart")].toBool(), &error ) == false )
		{
			vWarning() << "could not restore internet access block:" << error;
		}
	}
	else
	{
		// remove rules which might be left over (e.g. after the computer has been restarted)
		m_backend.unblock( nullptr );
		AddOnState::remove( QLatin1String(StateName) );
	}
}



bool InternetAccessPlugin::applyBlock( const QStringList& allowedHosts, bool keepAfterRestart, QString* errorString )
{
	FirewallBackend::Rules rules;
	rules.allowedIpv4 = NetworkRanges::localIpv4Networks();

	NetworkRanges::resolveHosts( allowedHosts + m_configuration.alwaysAllowed(), &rules.allowedIpv4, &rules.allowedIpv6 );

	// DNS servers have to stay reachable so allowed websites can be resolved
	const auto dnsServers = NetworkRanges::dnsServers();
	for( const auto& dnsServer : dnsServers )
	{
		( dnsServer.contains( QLatin1Char(':') ) ? rules.allowedIpv6 : rules.allowedIpv4 ).append( dnsServer );
	}

	if( m_backend.block( rules, m_configuration.enableWindowsFirewall(), errorString ) == false )
	{
		vWarning() << "could not block internet access:" << *errorString;
		return false;
	}

	m_blocked = true;
	m_allowedHosts = allowedHosts;
	m_keepAfterRestart = keepAfterRestart;
	saveState();

	// IP addresses of websites change from time to time
	m_refreshTimer.start( RefreshInterval );

	return true;
}



void InternetAccessPlugin::removeBlock()
{
	m_backend.unblock( nullptr );
	m_blocked = false;
	m_allowedHosts.clear();
	m_refreshTimer.stop();
	AddOnState::remove( QLatin1String(StateName) );
}



void InternetAccessPlugin::refreshBlock()
{
	if( m_blocked )
	{
		QString error;
		applyBlock( m_allowedHosts, m_keepAfterRestart, &error );
	}
}



void InternetAccessPlugin::notifyStudent( const QString& message )
{
	if( m_server && m_configuration.notifyStudents() )
	{
		VeyonCore::builtinFeatures().systemTrayIcon().showMessage( tr( "Internet access" ), message,
																   m_server->featureWorkerManager() );
	}
}



void InternetAccessPlugin::saveState()
{
	AddOnState::saveRuntimeState( QLatin1String(StateName),
								  QJsonObject{ { QStringLiteral("allowedHosts"), QJsonArray::fromStringList( m_allowedHosts ) },
											   { QStringLiteral("keepAfterRestart"), m_keepAfterRestart },
											   { QStringLiteral("backend"), m_backend.state() } },
								  m_keepAfterRestart );
}



void InternetAccessPlugin::showCollectedErrors()
{
	if( m_collectedErrors.isEmpty() )
	{
		return;
	}

	QMessageBox::warning( m_mainWindow, tr( "Internet Access" ),
						  tr( "Internet access could not be paused on the following computers:\n\n%1" )
						  .arg( m_collectedErrors.join( QLatin1Char('\n') ) ) );
	m_collectedErrors.clear();
}


IMPLEMENT_CONFIG_PROXY(InternetAccessConfiguration)
