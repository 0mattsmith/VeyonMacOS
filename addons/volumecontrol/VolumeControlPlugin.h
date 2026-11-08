/*
 * VolumeControlPlugin.h - declaration of VolumeControlPlugin class
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
#include "FeatureProviderInterface.h"

class VolumeBackend;
class VolumeManagerDialog;

class VolumeControlPlugin : public QObject, FeatureProviderInterface, PluginInterface, AddOnInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.AddOns.VolumeControl")
	Q_INTERFACES(PluginInterface FeatureProviderInterface AddOnInterface)
public:
	enum class Command
	{
		QueryState,
		State,
		SetLevel,
		SetMuted,
		Lock,
		Unlock,
	};
	Q_ENUM(Command)

	enum class Argument
	{
		Level,
		Muted,
		Locked,
		Persistent,
		Available,
	};
	Q_ENUM(Argument)

	struct VolumeState
	{
		bool available{false};
		int level{0};
		bool muted{false};
		bool locked{false};
	};

	explicit VolumeControlPlugin( QObject* parent = nullptr );
	~VolumeControlPlugin() override;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("b8e4f2a7-5d13-4c69-8e0b-3f7a1c9d2e58") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral( "VolumeControl" );
	}

	QString description() const override
	{
		return tr( "Control, mute and lock the volume of student computers" );
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
		return tr( "Remote Volume Manager" );
	}

	Category addOnCategory() const override
	{
		return Category::Control;
	}

	QString addOnIconUrl() const override
	{
		return QStringLiteral(":/volumecontrol/volume.png");
	}

	QString addOnSummary() const override
	{
		return tr( "Mute or unmute all student computers with one click, set the volume of individual computers "
				   "and lock it so students can't change it." );
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

	// master side API
	void queryState( const ComputerControlInterfaceList& computerControlInterfaces );
	void setLevel( const ComputerControlInterfaceList& computerControlInterfaces, int level );
	void setMuted( const ComputerControlInterfaceList& computerControlInterfaces, bool muted );
	void lock( const ComputerControlInterfaceList& computerControlInterfaces, int level, bool muted, bool persistent );
	void lockCurrent( const ComputerControlInterfaceList& computerControlInterfaces, bool persistent );
	void unlock( const ComputerControlInterfaceList& computerControlInterfaces );

Q_SIGNALS:
	void stateReceived( ComputerControlInterface::Pointer computerControlInterface, const VolumeControlPlugin::VolumeState& state );

private:
	static constexpr auto LockEnforcementInterval = 1000;
	static constexpr auto StateUpdateInterval = 5000;
	static constexpr auto StateName = "volume-lock";

	void initServer();
	void enforceLock();
	void updateState();
	void setLock( bool locked, int level, bool muted, bool persistent );
	FeatureMessage stateMessage();

	const Feature m_volumeFeature;
	const Feature m_volumeManagerFeature;
	const Feature m_muteFeature;
	const Feature m_unmuteFeature;
	const Feature m_lockFeature;
	const Feature m_unlockFeature;
	const Feature m_mutedIndicatorFeature;
	const Feature m_lockedIndicatorFeature;
	const FeatureList m_features;

	// server side state
	VolumeBackend* m_backend{nullptr};
	QTimer m_lockTimer{this};
	QTimer m_stateTimer{this};
	bool m_locked{false};
	int m_lockedLevel{0};
	bool m_lockedMuted{false};
	bool m_lockPersistent{false};
	bool m_currentlyMuted{false};

	QPointer<VolumeManagerDialog> m_dialog;

};

Q_DECLARE_METATYPE(VolumeControlPlugin::VolumeState)
