/*
 * FileSenderPlugin.h - declaration of FileSenderPlugin class
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
#include "FeatureProviderInterface.h"
#include "FileReceiver.h"
#include "WorkerIdleTimer.h"

class FileSenderDialog;

/*!
 * \brief Sends files and folders to many student computers at once. Files are written by a worker
 * running as the logged on student into the chosen folder and can be opened automatically.
 */
class FileSenderPlugin : public QObject, FeatureProviderInterface, PluginInterface, AddOnInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.AddOns.FileSender")
	Q_INTERFACES(PluginInterface FeatureProviderInterface AddOnInterface)
public:
	enum class Command
	{
		Begin,
		Chunk,
		End,
		Cancel,
		Complete,
		Result,
	};
	Q_ENUM(Command)

	enum class Argument
	{
		RequestId,
		TransferId,
		Destination,
		CustomPath,
		Subfolder,
		RelativePath,
		Size,
		ExistingFiles,
		Data,
		Error,
		Skipped,
		Path,
		Paths,
		OpenFiles,
		OpenFolder,
		ShowMessage,
		Message,
	};
	Q_ENUM(Argument)

	enum class Destination
	{
		Desktop,
		Documents,
		Downloads,
		FromTeacher,
		Custom,
	};
	Q_ENUM(Destination)

	enum class ExistingFiles
	{
		Rename,
		Replace,
		Skip,
	};
	Q_ENUM(ExistingFiles)

	static constexpr int ChunkSize = 256 * 1024;

	explicit FileSenderPlugin( QObject* parent = nullptr );
	~FileSenderPlugin() override;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("f4a1c6d9-2e83-4b57-a0d6-9c3e7b5f1a28") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral( "FileSender" );
	}

	QString description() const override
	{
		return tr( "Send files and folders to student computers and open them automatically" );
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
		return tr( "File Sender" );
	}

	Category addOnCategory() const override
	{
		return Category::Files;
	}

	QString addOnIconUrl() const override
	{
		return QStringLiteral(":/filesender/file-send.png");
	}

	QString addOnSummary() const override
	{
		return tr( "Hand out worksheets, documents or whole folders to all selected students at once. Choose the "
				   "destination (Desktop, Documents, Downloads or a custom folder), what happens with existing files "
				   "and whether the files should be opened automatically." );
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

	QUuid sendRequest( const ComputerControlInterface::Pointer& computerControlInterface, Command command,
					   const QMap<Argument, QVariant>& arguments );

	template<typename T>
	static QVariant argument( const QVariantMap& arguments, T argument )
	{
		return arguments.value( QString::number( int(argument) ) );
	}

Q_SIGNALS:
	void replyReceived( ComputerControlInterface::Pointer computerControlInterface, const QVariantMap& arguments );

private:
	static constexpr auto WorkerIdleTimeout = 5 * 60 * 1000;
	static constexpr auto MaximumPendingRequests = 2000;

	FeatureMessage handleWorkerRequest( const FeatureMessage& message );
	static QString destinationDirectory( Destination destination, const QString& customPath, const QString& subfolder );
	void showReceivedMessage( const QStringList& paths, const QString& folder, const QString& message );

	static FeatureMessage replyMessage( const FeatureMessage& request );

	const Feature m_fileSenderFeature;
	const FeatureList m_features;

	// server side
	QHash<QUuid, MessageContext> m_pendingRequests;

	// worker side
	QHash<QUuid, FileReceiver*> m_receivers;
	WorkerIdleTimer m_idleTimer{WorkerIdleTimeout};

	QPointer<FileSenderDialog> m_dialog;

};
