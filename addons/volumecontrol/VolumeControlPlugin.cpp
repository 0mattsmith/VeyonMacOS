/*
 * VolumeControlPlugin.cpp - implementation of VolumeControlPlugin class
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

#include <QJsonObject>

#include "AddOnState.h"
#include "AddOnUtils.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "VolumeBackend.h"
#include "VolumeControlPlugin.h"
#include "VolumeManagerDialog.h"


VolumeControlPlugin::VolumeControlPlugin( QObject* parent ) :
	QObject( parent ),
	m_volumeFeature( QStringLiteral("VolumeControl"),
					 Feature::Flag::Action | Feature::Flag::AllComponents,
					 Feature::Uid( "4c9e2b81-73a6-4f0d-b5e8-1d2a6c7f9b03" ),
					 Feature::Uid(),
					 tr( "Volume" ), {},
					 tr( "Mute, unmute or lock the volume of student computers" ),
					 QStringLiteral(":/volumecontrol/volume.png") ),
	m_volumeManagerFeature( QStringLiteral("VolumeManager"),
							Feature::Flag::Action | Feature::Flag::Master,
							Feature::Uid( "8f13d5c6-2a4b-4e97-a0c1-6b5e9d3f7a24" ),
							m_volumeFeature.uid(),
							tr( "Volume Manager…" ), {},
							tr( "Show and change the volume of each student computer" ),
							QStringLiteral(":/volumecontrol/volume.png") ),
	m_muteFeature( QStringLiteral("MuteVolume"),
				   Feature::Flag::Action | Feature::Flag::Master,
				   Feature::Uid( "e2a7c3f9-1b6d-4d58-9e04-7c8b2a5f1d36" ),
				   m_volumeFeature.uid(),
				   tr( "Mute" ), {},
				   tr( "Mute the sound of the selected computers" ),
				   QStringLiteral(":/volumecontrol/volume-mute.png") ),
	m_unmuteFeature( QStringLiteral("UnmuteVolume"),
					 Feature::Flag::Action | Feature::Flag::Master,
					 Feature::Uid( "5b0d9e4a-6c27-4f1b-83d5-2e9a7c1f4b68" ),
					 m_volumeFeature.uid(),
					 tr( "Unmute" ), {},
					 tr( "Unmute the sound of the selected computers" ),
					 QStringLiteral(":/volumecontrol/volume.png") ),
	m_lockFeature( QStringLiteral("LockVolume"),
				   Feature::Flag::Action | Feature::Flag::Master,
				   Feature::Uid( "a6f4b1c8-3e92-4d07-b6a5-9c0e2d8f1a73" ),
				   m_volumeFeature.uid(),
				   tr( "Lock volume" ), {},
				   tr( "Prevent students from changing the current volume" ),
				   QStringLiteral(":/volumecontrol/volume-lock.png") ),
	m_unlockFeature( QStringLiteral("UnlockVolume"),
					 Feature::Flag::Action | Feature::Flag::Master,
					 Feature::Uid( "3d8c6a2f-9b15-4e74-a2c9-5f1b7e0d6c49" ),
					 m_volumeFeature.uid(),
					 tr( "Unlock volume" ), {},
					 tr( "Allow students to change the volume again" ),
					 QStringLiteral(":/volumecontrol/volume.png") ),
	m_mutedIndicatorFeature( QStringLiteral("VolumeMutedIndicator"),
							 Feature::Flag::Meta | Feature::Flag::AllComponents,
							 Feature::Uid( "c1e5a9d2-4f86-4b30-9d7e-8a2c6f0b5e17" ),
							 Feature::Uid(),
							 tr( "Muted" ), {}, {},
							 QStringLiteral(":/volumecontrol/volume-mute.png") ),
	m_lockedIndicatorFeature( QStringLiteral("VolumeLockedIndicator"),
							  Feature::Flag::Meta | Feature::Flag::AllComponents,
							  Feature::Uid( "7a3f0e6b-d2c4-4891-b5f7-1e9c3a6d8b20" ),
							  Feature::Uid(),
							  tr( "Volume locked" ), {}, {},
							  QStringLiteral(":/volumecontrol/volume-lock.png") ),
	m_features( { m_volumeFeature, m_volumeManagerFeature, m_muteFeature, m_unmuteFeature,
				  m_lockFeature, m_unlockFeature, m_mutedIndicatorFeature, m_lockedIndicatorFeature } )
{
	qRegisterMetaType<VolumeControlPlugin::VolumeState>();

	if( AddOnUtils::isServer() )
	{
		QTimer::singleShot( 0, this, &VolumeControlPlugin::initServer );
	}
}



VolumeControlPlugin::~VolumeControlPlugin()
{
	delete m_backend;
}



bool VolumeControlPlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
										  const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( operation != Operation::Start )
	{
		return false;
	}

	if( featureUid == m_muteFeature.uid() )
	{
		setMuted( computerControlInterfaces, true );
	}
	else if( featureUid == m_unmuteFeature.uid() )
	{
		setMuted( computerControlInterfaces, false );
	}
	else if( featureUid == m_lockFeature.uid() )
	{
		if( arguments.contains( QStringLiteral("level") ) )
		{
			lock( computerControlInterfaces, arguments.value( QStringLiteral("level") ).toInt(),
				  arguments.value( QStringLiteral("muted") ).toBool(),
				  arguments.value( QStringLiteral("persistent") ).toBool() );
		}
		else
		{
			lockCurrent( computerControlInterfaces, arguments.value( QStringLiteral("persistent") ).toBool() );
		}
	}
	else if( featureUid == m_unlockFeature.uid() )
	{
		unlock( computerControlInterfaces );
	}
	else if( featureUid == m_volumeFeature.uid() && arguments.contains( QStringLiteral("level") ) )
	{
		setLevel( computerControlInterfaces, arguments.value( QStringLiteral("level") ).toInt() );
	}
	else
	{
		return false;
	}

	return true;
}



bool VolumeControlPlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
										const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( feature == m_volumeManagerFeature || feature == m_volumeFeature )
	{
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
		}
		else
		{
			m_dialog = new VolumeManagerDialog( *this, interfaces, master.mainWindow() );
			m_dialog->setAttribute( Qt::WA_DeleteOnClose );
			m_dialog->show();
		}
		return true;
	}

	return controlFeature( feature.uid(), Operation::Start, {}, computerControlInterfaces );
}



bool VolumeControlPlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
												const FeatureMessage& message )
{
	if( message.featureUid() != m_volumeFeature.uid() || message.command<Command>() != Command::State )
	{
		return false;
	}

	VolumeState state;
	state.available = message.argument( Argument::Available ).toBool();
	state.level = message.argument( Argument::Level ).toInt();
	state.muted = message.argument( Argument::Muted ).toBool();
	state.locked = message.argument( Argument::Locked ).toBool();

	Q_EMIT stateReceived( computerControlInterface, state );

	return true;
}



bool VolumeControlPlugin::handleFeatureMessage( VeyonServerInterface& server,
												const MessageContext& messageContext,
												const FeatureMessage& message )
{
	if( message.featureUid() != m_volumeFeature.uid() )
	{
		return false;
	}

	if( m_backend == nullptr )
	{
		initServer();
	}

	switch( message.command<Command>() )
	{
	case Command::QueryState:
		break;

	case Command::SetLevel:
		m_backend->setLevel( message.argument( Argument::Level ).toInt() );
		if( m_locked )
		{
			setLock( true, message.argument( Argument::Level ).toInt(), m_lockedMuted, m_lockPersistent );
		}
		break;

	case Command::SetMuted:
		m_backend->setMuted( message.argument( Argument::Muted ).toBool() );
		if( m_locked )
		{
			setLock( true, m_lockedLevel, message.argument( Argument::Muted ).toBool(), m_lockPersistent );
		}
		break;

	case Command::Lock:
	{
		const auto current = m_backend->state();
		const auto level = message.hasArgument( Argument::Level ) ? message.argument( Argument::Level ).toInt() : current.level;
		const auto muted = message.hasArgument( Argument::Muted ) ? message.argument( Argument::Muted ).toBool() : current.muted;
		setLock( true, level, muted, message.argument( Argument::Persistent ).toBool() );
		break;
	}

	case Command::Unlock:
		setLock( false, 0, false, false );
		break;

	default:
		return false;
	}

	updateState();

	return server.sendFeatureMessageReply( messageContext, stateMessage() );
}



bool VolumeControlPlugin::isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const
{
	Q_UNUSED(server)

	if( featureUid == m_lockedIndicatorFeature.uid() )
	{
		return m_locked;
	}

	if( featureUid == m_mutedIndicatorFeature.uid() )
	{
		return m_currentlyMuted;
	}

	return false;
}



void VolumeControlPlugin::queryState( const ComputerControlInterfaceList& computerControlInterfaces )
{
	sendFeatureMessage( FeatureMessage( m_volumeFeature.uid(), Command::QueryState ), computerControlInterfaces );
}



void VolumeControlPlugin::setLevel( const ComputerControlInterfaceList& computerControlInterfaces, int level )
{
	sendFeatureMessage( FeatureMessage( m_volumeFeature.uid(), Command::SetLevel )
						.addArgument( Argument::Level, qBound( 0, level, 100 ) ), computerControlInterfaces );
}



void VolumeControlPlugin::setMuted( const ComputerControlInterfaceList& computerControlInterfaces, bool muted )
{
	sendFeatureMessage( FeatureMessage( m_volumeFeature.uid(), Command::SetMuted )
						.addArgument( Argument::Muted, muted ), computerControlInterfaces );
}



void VolumeControlPlugin::lock( const ComputerControlInterfaceList& computerControlInterfaces, int level, bool muted, bool persistent )
{
	sendFeatureMessage( FeatureMessage( m_volumeFeature.uid(), Command::Lock )
						.addArgument( Argument::Level, qBound( 0, level, 100 ) )
						.addArgument( Argument::Muted, muted )
						.addArgument( Argument::Persistent, persistent ), computerControlInterfaces );
}



void VolumeControlPlugin::lockCurrent( const ComputerControlInterfaceList& computerControlInterfaces, bool persistent )
{
	sendFeatureMessage( FeatureMessage( m_volumeFeature.uid(), Command::Lock )
						.addArgument( Argument::Persistent, persistent ), computerControlInterfaces );
}



void VolumeControlPlugin::unlock( const ComputerControlInterfaceList& computerControlInterfaces )
{
	sendFeatureMessage( FeatureMessage( m_volumeFeature.uid(), Command::Unlock ), computerControlInterfaces );
}



void VolumeControlPlugin::initServer()
{
	if( m_backend )
	{
		return;
	}

	m_backend = new VolumeBackend;

	connect( &m_lockTimer, &QTimer::timeout, this, &VolumeControlPlugin::enforceLock );
	connect( &m_stateTimer, &QTimer::timeout, this, &VolumeControlPlugin::updateState );
	m_stateTimer.start( StateUpdateInterval );

	const auto state = AddOnState::loadRuntimeState( QLatin1String(StateName) );
	if( state.isEmpty() == false )
	{
		setLock( true, state[QStringLiteral("level")].toInt(), state[QStringLiteral("muted")].toBool(),
				 state[QStringLiteral("persistent")].toBool() );
	}

	updateState();
}



void VolumeControlPlugin::enforceLock()
{
	if( m_backend == nullptr || m_locked == false )
	{
		return;
	}

	const auto state = m_backend->state();
	if( state.available == false )
	{
		return;
	}

	if( qAbs( state.level - m_lockedLevel ) > 1 )
	{
		m_backend->setLevel( m_lockedLevel );
	}

	if( state.muted != m_lockedMuted )
	{
		m_backend->setMuted( m_lockedMuted );
	}
}



void VolumeControlPlugin::updateState()
{
	if( m_backend )
	{
		m_currentlyMuted = m_backend->state().muted;
	}
}



void VolumeControlPlugin::setLock( bool locked, int level, bool muted, bool persistent )
{
	m_locked = locked;
	m_lockedLevel = qBound( 0, level, 100 );
	m_lockedMuted = muted;
	m_lockPersistent = persistent;

	// the lock survives restarts of the server (logging off and on) and optionally reboots
	if( m_locked )
	{
		AddOnState::saveRuntimeState( QLatin1String(StateName),
									  QJsonObject{ { QStringLiteral("level"), m_lockedLevel },
												   { QStringLiteral("muted"), m_lockedMuted },
												   { QStringLiteral("persistent"), m_lockPersistent } },
									  m_lockPersistent );
	}
	else
	{
		AddOnState::remove( QLatin1String(StateName) );
	}

	if( m_locked )
	{
		m_lockTimer.start( LockEnforcementInterval );
		enforceLock();
	}
	else
	{
		m_lockTimer.stop();
	}
}



FeatureMessage VolumeControlPlugin::stateMessage()
{
	const auto state = m_backend->state();

	FeatureMessage message( m_volumeFeature.uid(), Command::State );
	message.addArgument( Argument::Available, state.available );
	message.addArgument( Argument::Level, state.level );
	message.addArgument( Argument::Muted, state.muted );
	message.addArgument( Argument::Locked, m_locked );

	return FeatureMessage( message );
}
