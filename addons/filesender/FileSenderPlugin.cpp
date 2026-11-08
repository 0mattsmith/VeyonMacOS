/*
 * FileSenderPlugin.cpp - implementation of FileSenderPlugin class
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

#include <QDesktopServices>
#include <QDir>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>

#include "FeatureWorkerManager.h"
#include "FileSenderDialog.h"
#include "FileSenderPlugin.h"
#include "PlatformCoreFunctions.h"
#include "PlatformPluginInterface.h"
#include "PlatformSessionFunctions.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"


FileSenderPlugin::FileSenderPlugin( QObject* parent ) :
	QObject( parent ),
	m_fileSenderFeature( QStringLiteral("FileSender"),
						 Feature::Flag::Action | Feature::Flag::AllComponents,
						 Feature::Uid( "9b2e7d41-5c86-4a1f-b3e9-6d0a4c8f2e75" ),
						 Feature::Uid(),
						 tr( "Send files" ), {},
						 tr( "Send files and folders to the selected student computers" ),
						 QStringLiteral(":/filesender/file-send.png") ),
	m_features( { m_fileSenderFeature } )
{
}



FileSenderPlugin::~FileSenderPlugin()
{
	qDeleteAll( m_receivers );
}



bool FileSenderPlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
									   const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(featureUid)
	Q_UNUSED(operation)
	Q_UNUSED(arguments)
	Q_UNUSED(computerControlInterfaces)

	return false;
}



bool FileSenderPlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
									 const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( feature != m_fileSenderFeature )
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

	m_dialog = new FileSenderDialog( *this, interfaces, master.mainWindow() );
	m_dialog->setAttribute( Qt::WA_DeleteOnClose );
	m_dialog->show();

	return true;
}



bool FileSenderPlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
											 const FeatureMessage& message )
{
	if( message.featureUid() != m_fileSenderFeature.uid() )
	{
		return false;
	}

	Q_EMIT replyReceived( computerControlInterface, message.arguments() );

	return true;
}



bool FileSenderPlugin::handleFeatureMessage( VeyonServerInterface& server,
											 const MessageContext& messageContext,
											 const FeatureMessage& message )
{
	if( message.featureUid() != m_fileSenderFeature.uid() )
	{
		return false;
	}

	if( VeyonCore::platform().sessionFunctions().currentSessionHasUser() == false )
	{
		auto reply = replyMessage( message );
		reply.addArgument( Argument::Error, tr( "No user is logged on at this computer." ) );
		return server.sendFeatureMessageReply( messageContext, reply );
	}

	if( m_pendingRequests.size() > MaximumPendingRequests )
	{
		m_pendingRequests.clear();
	}
	m_pendingRequests[message.argument( Argument::RequestId ).toUuid()] = messageContext;

	// files are written by a worker running as the logged on student so the files belong to them
	server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );

	return true;
}



bool FileSenderPlugin::handleFeatureMessageFromWorker( VeyonServerInterface& server, const FeatureMessage& message )
{
	if( message.featureUid() != m_fileSenderFeature.uid() )
	{
		return false;
	}

	return server.sendFeatureMessageReply( m_pendingRequests.take( message.argument( Argument::RequestId ).toUuid() ), message );
}



bool FileSenderPlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	if( message.featureUid() != m_fileSenderFeature.uid() )
	{
		return false;
	}

	m_idleTimer.restart();

	return worker.sendFeatureMessageReply( handleWorkerRequest( message ) );
}



QUuid FileSenderPlugin::sendRequest( const ComputerControlInterface::Pointer& computerControlInterface, Command command,
									 const QMap<Argument, QVariant>& arguments )
{
	const auto requestId = QUuid::createUuid();

	FeatureMessage message( m_fileSenderFeature.uid(), command );
	message.addArgument( Argument::RequestId, requestId );
	for( auto it = arguments.constBegin(), end = arguments.constEnd(); it != end; ++it )
	{
		message.addArgument( it.key(), it.value() );
	}

	computerControlInterface->sendFeatureMessage( message );

	return requestId;
}



FeatureMessage FileSenderPlugin::handleWorkerRequest( const FeatureMessage& message )
{
	const auto transferId = message.argument( Argument::TransferId ).toUuid();

	auto result = replyMessage( message );
	const auto fail = [&result]( const QString& error ) {
		result.addArgument( Argument::Error, error );
		return FeatureMessage( result );
	};

	switch( message.command<Command>() )
	{
	case Command::Begin:
	{
		const auto directory = destinationDirectory( Destination( message.argument( Argument::Destination ).toInt() ),
													 message.argument( Argument::CustomPath ).toString(),
													 message.argument( Argument::Subfolder ).toString() );
		const auto relativePath = FileReceiver::sanitizeRelativePath( message.argument( Argument::RelativePath ).toString() );
		if( directory.isEmpty() || relativePath.isEmpty() )
		{
			return fail( tr( "Invalid destination." ) );
		}

		auto targetPath = QDir( directory ).filePath( relativePath );
		const auto existingFiles = ExistingFiles( message.argument( Argument::ExistingFiles ).toInt() );
		if( QFileInfo::exists( targetPath ) )
		{
			if( existingFiles == ExistingFiles::Skip )
			{
				result.addArgument( Argument::Skipped, true );
				result.addArgument( Argument::Path, targetPath );
				return FeatureMessage( result );
			}
			if( existingFiles == ExistingFiles::Rename )
			{
				const QFileInfo targetInfo( targetPath );
				targetPath = FileReceiver::uniqueFilePath( targetInfo.absolutePath(), targetInfo.fileName() );
			}
		}

		auto receiver = new FileReceiver;
		QString error;
		if( receiver->begin( targetPath, existingFiles == ExistingFiles::Replace, &error ) == false )
		{
			delete receiver;
			return fail( error );
		}

		delete m_receivers.take( transferId );
		m_receivers[transferId] = receiver;
		result.addArgument( Argument::Path, targetPath );
		return FeatureMessage( result );
	}

	case Command::Chunk:
	{
		auto receiver = m_receivers.value( transferId );
		QString error;
		if( receiver == nullptr )
		{
			return fail( tr( "Invalid transfer." ) );
		}
		if( receiver->write( message.argument( Argument::Data ).toByteArray(), &error ) == false )
		{
			delete m_receivers.take( transferId );
			return fail( error );
		}
		result.addArgument( Argument::Size, receiver->bytesWritten() );
		return FeatureMessage( result );
	}

	case Command::End:
	{
		auto receiver = m_receivers.take( transferId );
		QString error;
		const auto success = receiver && receiver->finish( &error );
		if( receiver )
		{
			result.addArgument( Argument::Path, receiver->targetFilePath() );
		}
		delete receiver;
		return success ? FeatureMessage( result ) : fail( error.isEmpty() ? tr( "Invalid transfer." ) : error );
	}

	case Command::Cancel:
		delete m_receivers.take( transferId );
		return FeatureMessage( result );

	case Command::Complete:
	{
		const auto paths = message.argument( Argument::Paths ).toStringList();
		const auto folder = destinationDirectory( Destination( message.argument( Argument::Destination ).toInt() ),
												  message.argument( Argument::CustomPath ).toString(),
												  message.argument( Argument::Subfolder ).toString() );

		if( message.argument( Argument::OpenFiles ).toBool() )
		{
			for( const auto& path : paths )
			{
				QDesktopServices::openUrl( QUrl::fromLocalFile( path ) );
			}
		}

		if( message.argument( Argument::OpenFolder ).toBool() )
		{
			QDesktopServices::openUrl( QUrl::fromLocalFile( folder ) );
		}

		if( message.argument( Argument::ShowMessage ).toBool() )
		{
			showReceivedMessage( paths, folder, message.argument( Argument::Message ).toString() );
		}

		return FeatureMessage( result );
	}

	default:
		break;
	}

	return fail( tr( "Unsupported request." ) );
}



QString FileSenderPlugin::destinationDirectory( Destination destination, const QString& customPath, const QString& subfolder )
{
	QString directory;

	switch( destination )
	{
	case Destination::Desktop:
		directory = QStandardPaths::writableLocation( QStandardPaths::DesktopLocation );
		break;
	case Destination::Documents:
		directory = QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation );
		break;
	case Destination::Downloads:
		directory = QStandardPaths::writableLocation( QStandardPaths::DownloadLocation );
		break;
	case Destination::FromTeacher:
		directory = QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) + QStringLiteral("/") + tr( "From teacher" );
		break;
	case Destination::Custom:
		if( customPath.isEmpty() )
		{
			return {};
		}
		// relative paths are relative to the home directory of the student
		directory = QDir::isAbsolutePath( customPath ) ? customPath : QDir::home().filePath( customPath );
		break;
	}

	const auto cleanSubfolder = FileReceiver::sanitizeRelativePath( subfolder );
	if( cleanSubfolder.isEmpty() == false )
	{
		directory = QDir( directory ).filePath( cleanSubfolder );
	}

	return QDir::cleanPath( directory );
}



void FileSenderPlugin::showReceivedMessage( const QStringList& paths, const QString& folder, const QString& message )
{
	auto text = message.isEmpty() ? tr( "Your teacher has sent you %n file(s).", "", int(paths.size()) ) : message;
	text += QStringLiteral("\n\n") + tr( "Location: %1" ).arg( QDir::toNativeSeparators( folder ) );

	auto messageBox = new QMessageBox( QMessageBox::Information, tr( "Files received" ), text );
	auto openButton = messageBox->addButton( tr( "Open folder" ), QMessageBox::ActionRole );
	messageBox->addButton( QMessageBox::Close );
	messageBox->setAttribute( Qt::WA_DeleteOnClose );
	connect( openButton, &QPushButton::clicked, this, [folder]() {
		QDesktopServices::openUrl( QUrl::fromLocalFile( folder ) );
	} );
	messageBox->show();
	VeyonCore::platform().coreFunctions().raiseWindow( messageBox, true );
}



FeatureMessage FileSenderPlugin::replyMessage( const FeatureMessage& request )
{
	FeatureMessage reply( request.featureUid(), Command::Result );
	reply.addArgument( Argument::RequestId, request.argument( Argument::RequestId ) );
	reply.addArgument( Argument::TransferId, request.argument( Argument::TransferId ) );
	return FeatureMessage( reply );
}
