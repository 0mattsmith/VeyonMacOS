/*
 * FileManagerPlugin.h - declaration of FileManagerPlugin class
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

#include "AddOnInterface.h"
#include "ConfigurationPagePluginInterface.h"
#include "FeatureProviderInterface.h"
#include "FileManagerConfiguration.h"
#include "FileReceiver.h"
#include "WorkerIdleTimer.h"

class FileManagerDialog;

/*!
 * \brief Lets teachers browse the files of the student logged on at a computer, download (collect) and upload
 * (hand out) files. All file operations are performed by a worker process running as the logged on student,
 * i.e. only files which the student can access are accessible.
 */
class FileManagerPlugin : public QObject, FeatureProviderInterface, PluginInterface, AddOnInterface,
		ConfigurationPagePluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.AddOns.FileManager")
	Q_INTERFACES(PluginInterface FeatureProviderInterface AddOnInterface ConfigurationPagePluginInterface)
public:
	enum class Command
	{
		ListRoots,
		Roots,
		ListDirectory,
		DirectoryListing,
		CreateDirectory,
		Rename,
		Delete,
		OpenFile,
		Result,
		DownloadStart,
		DownloadInfo,
		DownloadChunk,
		DownloadData,
		DownloadEnd,
		UploadStart,
		UploadChunk,
		UploadFinish,
		UploadCancel,
	};
	Q_ENUM(Command)

	enum class Argument
	{
		RequestId,
		Path,
		Name,
		Paths,
		Entries,
		Error,
		TransferId,
		Size,
		Offset,
		Data,
		Overwrite,
		Final,
		ReadOnly,
	};
	Q_ENUM(Argument)

	static constexpr int ChunkSize = 256 * 1024;

	explicit FileManagerPlugin( QObject* parent = nullptr );
	~FileManagerPlugin() override;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("c7d2e8f1-4a6b-4c39-9e05-8b1f3d7a2c64") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral( "FileManager" );
	}

	QString description() const override
	{
		return tr( "Browse the files of students and transfer files to and from student computers" );
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
		return tr( "Remote File Manager" );
	}

	Category addOnCategory() const override
	{
		return Category::Files;
	}

	QString addOnIconUrl() const override
	{
		return QStringLiteral(":/filemanager/filemanager.png");
	}

	QString addOnSummary() const override
	{
		return tr( "Browse the files of the student logged on at a computer to retrieve work or hand out files. "
				   "Only files the student has access to are shown and the student is notified when a teacher "
				   "accesses their files (can be configured)." );
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

	ConfigurationPage* createConfigurationPage() override;

	//! \brief Sends a request to a computer and returns its ID (master side)
	QUuid sendRequest( const ComputerControlInterface::Pointer& computerControlInterface, Command command,
					   const QMap<Argument, QVariant>& arguments = {} );

	template<typename T>
	static QVariant argument( const QVariantMap& arguments, T argument )
	{
		return arguments.value( QString::number( int(argument) ) );
	}

Q_SIGNALS:
	void replyReceived( ComputerControlInterface::Pointer computerControlInterface, int command, const QVariantMap& arguments );

private:
	static constexpr auto WorkerIdleTimeout = 10 * 60 * 1000;
	static constexpr auto NotificationInterval = 5 * 60;
	static constexpr auto MaximumListingSize = 10000;
	static constexpr auto MaximumPendingRequests = 2000;

	// worker side
	FeatureMessage handleWorkerRequest( const FeatureMessage& message );
	QVariantList roots() const;
	FeatureMessage listDirectory( const FeatureMessage& request ) const;
	QString deletePaths( const QStringList& paths ) const;

	static FeatureMessage replyMessage( const FeatureMessage& request, Command command );

	const Feature m_fileManagerFeature;
	const FeatureList m_features;

	FileManagerConfiguration m_configuration;

	// server side
	QHash<QUuid, MessageContext> m_pendingRequests;
	QDateTime m_lastNotification;

	// worker side
	QHash<QUuid, FileReceiver*> m_uploads;
	QHash<QUuid, QFile*> m_downloads;
	WorkerIdleTimer m_idleTimer{WorkerIdleTimeout};

	QPointer<FileManagerDialog> m_dialog;

};
