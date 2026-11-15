/*
 * FileManagerPlugin.cpp - implementation of FileManagerPlugin class
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
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>

#include "BuiltinFeatures.h"
#include "FeatureWorkerManager.h"
#include "FileManagerDialog.h"
#include "FileManagerPlugin.h"
#include "PlatformPluginInterface.h"
#include "PlatformSessionFunctions.h"
#include "SimpleConfigurationPage.h"
#include "SystemTrayIcon.h"
#include "VeyonConfiguration.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"


FileManagerPlugin::FileManagerPlugin( QObject* parent ) :
	QObject( parent ),
	m_fileManagerFeature( QStringLiteral("FileManager"),
						  Feature::Flag::Action | Feature::Flag::AllComponents,
						  Feature::Uid( "0e8b3f5a-7c21-4d96-a4b8-2f6c9e1d5a37" ),
						  Feature::Uid(),
						  tr( "Files" ), {},
						  tr( "Browse the files of the logged on student to collect work or hand out files" ),
						  QStringLiteral("veyon-file-manager") ),
	m_features( { m_fileManagerFeature } ),
	m_configuration( &VeyonCore::config() )
{
}



FileManagerPlugin::~FileManagerPlugin()
{
	qDeleteAll( m_uploads );
	qDeleteAll( m_downloads );
}



bool FileManagerPlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
										const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(featureUid)
	Q_UNUSED(operation)
	Q_UNUSED(arguments)
	Q_UNUSED(computerControlInterfaces)

	return false;
}



bool FileManagerPlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
									  const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( feature != m_fileManagerFeature )
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

	m_dialog = new FileManagerDialog( *this, interfaces, master.mainWindow() );
	m_dialog->setAttribute( Qt::WA_DeleteOnClose );
	m_dialog->show();

	return true;
}



bool FileManagerPlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
											  const FeatureMessage& message )
{
	if( message.featureUid() != m_fileManagerFeature.uid() )
	{
		return false;
	}

	Q_EMIT replyReceived( computerControlInterface, message.command<int>(), message.arguments() );

	return true;
}



bool FileManagerPlugin::handleFeatureMessage( VeyonServerInterface& server,
											  const MessageContext& messageContext,
											  const FeatureMessage& message )
{
	if( message.featureUid() != m_fileManagerFeature.uid() )
	{
		return false;
	}

	const auto command = message.command<Command>();

	const auto replyError = [&]( const QString& error ) {
		auto reply = replyMessage( message, Command::Result );
		reply.addArgument( Argument::Error, error );
		return server.sendFeatureMessageReply( messageContext, reply );
	};

	if( VeyonCore::platform().sessionFunctions().currentSessionHasUser() == false )
	{
		return replyError( tr( "No user is logged on at this computer." ) );
	}

	const auto modifyingCommand = command == Command::CreateDirectory ||
								  command == Command::Rename ||
								  command == Command::Delete ||
								  command == Command::UploadStart ||
								  command == Command::UploadChunk ||
								  command == Command::UploadFinish;
	if( modifyingCommand && m_configuration.allowModifications() == false )
	{
		return replyError( tr( "Changing files on this computer has been disabled by the administrator." ) );
	}

	// let the student know that a teacher is accessing their files
	if( m_configuration.notifyStudents() &&
		( command == Command::ListRoots || command == Command::DownloadStart || command == Command::UploadStart ) )
	{
		const auto now = QDateTime::currentDateTime();
		if( m_lastNotification.isValid() == false || m_lastNotification.secsTo( now ) > NotificationInterval )
		{
			m_lastNotification = now;
			VeyonCore::builtinFeatures().systemTrayIcon().showMessage(
				tr( "File access" ),
				tr( "Your teacher is accessing your files using Veyon." ),
				server.featureWorkerManager() );
		}
	}

	if( m_pendingRequests.size() > MaximumPendingRequests )
	{
		m_pendingRequests.clear();
	}
	m_pendingRequests[message.argument( Argument::RequestId ).toUuid()] = messageContext;

	// file operations are performed by a worker running as the logged on user
	server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );

	return true;
}



bool FileManagerPlugin::handleFeatureMessageFromWorker( VeyonServerInterface& server, const FeatureMessage& message )
{
	if( message.featureUid() != m_fileManagerFeature.uid() )
	{
		return false;
	}

	// forward reply to the master which sent the request
	return server.sendFeatureMessageReply( m_pendingRequests.take( message.argument( Argument::RequestId ).toUuid() ), message );
}



bool FileManagerPlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	if( message.featureUid() != m_fileManagerFeature.uid() )
	{
		return false;
	}

	m_idleTimer.restart();

	return worker.sendFeatureMessageReply( handleWorkerRequest( message ) );
}



ConfigurationPage* FileManagerPlugin::createConfigurationPage()
{
	auto page = new SimpleConfigurationPage( tr( "File Manager" ), QStringLiteral("veyon-file-manager") );
	page->addDescription( tr( "The Remote File Manager add-on lets teachers browse the files of the student logged on at a "
							  "computer. All operations run with the permissions of the student." ) );
	page->addCheckBox( m_configuration.notifyStudentsProperty(),
					   tr( "Notify students when a teacher accesses their files" ) );
	page->addCheckBox( m_configuration.allowModificationsProperty(),
					   tr( "Allow teachers to upload, rename and delete files" ),
					   tr( "If disabled, teachers can only browse and download files." ) );
	page->addStretch();
	return page;
}



QUuid FileManagerPlugin::sendRequest( const ComputerControlInterface::Pointer& computerControlInterface, Command command,
									  const QMap<Argument, QVariant>& arguments )
{
	const auto requestId = QUuid::createUuid();

	FeatureMessage message( m_fileManagerFeature.uid(), command );
	message.addArgument( Argument::RequestId, requestId );
	for( auto it = arguments.constBegin(), end = arguments.constEnd(); it != end; ++it )
	{
		message.addArgument( it.key(), it.value() );
	}

	computerControlInterface->sendFeatureMessage( message );

	return requestId;
}



FeatureMessage FileManagerPlugin::handleWorkerRequest( const FeatureMessage& message )
{
	const auto command = message.command<Command>();
	const auto path = message.argument( Argument::Path ).toString();
	const auto transferId = message.argument( Argument::TransferId ).toUuid();

	auto result = replyMessage( message, Command::Result );
	const auto fail = [&result]( const QString& error ) {
		result.addArgument( Argument::Error, error );
		return FeatureMessage( result );
	};

	const auto modifyingCommand = command == Command::CreateDirectory || command == Command::Rename ||
								  command == Command::Delete || command == Command::UploadStart;
	if( modifyingCommand && m_configuration.allowModifications() == false )
	{
		return fail( tr( "Changing files on this computer has been disabled by the administrator." ) );
	}

	switch( command )
	{
	case Command::ListRoots:
	{
		auto reply = replyMessage( message, Command::Roots );
		reply.addArgument( Argument::Entries, roots() );
		reply.addArgument( Argument::ReadOnly, m_configuration.allowModifications() == false );
		return FeatureMessage( reply );
	}

	case Command::ListDirectory:
		return listDirectory( message );

	case Command::CreateDirectory:
	{
		const auto name = message.argument( Argument::Name ).toString();
		if( name.isEmpty() || name.contains( QLatin1Char('/') ) || name.contains( QLatin1Char('\\') ) ||
			QDir( path ).mkdir( name ) == false )
		{
			return fail( tr( "Could not create folder \"%1\"." ).arg( name ) );
		}
		return FeatureMessage( result );
	}

	case Command::Rename:
	{
		const QFileInfo fileInfo( path );
		const auto newName = message.argument( Argument::Name ).toString();
		if( newName.isEmpty() || newName.contains( QLatin1Char('/') ) || newName.contains( QLatin1Char('\\') ) ||
			QDir( fileInfo.absolutePath() ).rename( fileInfo.fileName(), newName ) == false )
		{
			return fail( tr( "Could not rename \"%1\" to \"%2\"." ).arg( fileInfo.fileName(), newName ) );
		}
		return FeatureMessage( result );
	}

	case Command::Delete:
	{
		const auto error = deletePaths( message.argument( Argument::Paths ).toStringList() );
		return error.isEmpty() ? FeatureMessage( result ) : fail( error );
	}

	case Command::OpenFile:
		if( QDesktopServices::openUrl( QUrl::fromLocalFile( path ) ) == false )
		{
			return fail( tr( "Could not open \"%1\"." ).arg( QFileInfo( path ).fileName() ) );
		}
		return FeatureMessage( result );

	case Command::DownloadStart:
	{
		auto file = new QFile( path );
		if( QFileInfo( path ).isFile() == false || file->open( QFile::ReadOnly ) == false )
		{
			const auto error = file->errorString();
			delete file;
			return fail( tr( "Could not read \"%1\": %2" ).arg( QFileInfo( path ).fileName(), error ) );
		}
		delete m_downloads.take( transferId );
		m_downloads[transferId] = file;

		auto reply = replyMessage( message, Command::DownloadInfo );
		reply.addArgument( Argument::Size, file->size() );
		return FeatureMessage( reply );
	}

	case Command::DownloadChunk:
	{
		auto file = m_downloads.value( transferId );
		if( file == nullptr || file->seek( message.argument( Argument::Offset ).toLongLong() ) == false )
		{
			return fail( tr( "Invalid download request." ) );
		}

		auto reply = replyMessage( message, Command::DownloadData );
		reply.addArgument( Argument::Offset, file->pos() );
		reply.addArgument( Argument::Data, file->read( ChunkSize ) );
		reply.addArgument( Argument::Final, file->atEnd() );
		return FeatureMessage( reply );
	}

	case Command::DownloadEnd:
		delete m_downloads.take( transferId );
		return FeatureMessage( result );

	case Command::UploadStart:
	{
		auto receiver = new FileReceiver;
		const auto fileName = FileReceiver::sanitizeRelativePath( message.argument( Argument::Name ).toString() );
		QString error;
		if( fileName.isEmpty() ||
			receiver->begin( QDir( path ).filePath( fileName ), message.argument( Argument::Overwrite ).toBool(), &error ) == false )
		{
			delete receiver;
			return fail( error.isEmpty() ? tr( "Invalid file name." ) : error );
		}
		delete m_uploads.take( transferId );
		m_uploads[transferId] = receiver;
		return FeatureMessage( result );
	}

	case Command::UploadChunk:
	{
		auto receiver = m_uploads.value( transferId );
		QString error;
		if( receiver == nullptr )
		{
			return fail( tr( "Invalid upload request." ) );
		}
		if( receiver->write( message.argument( Argument::Data ).toByteArray(), &error ) == false )
		{
			delete m_uploads.take( transferId );
			return fail( error );
		}
		result.addArgument( Argument::Size, receiver->bytesWritten() );
		return FeatureMessage( result );
	}

	case Command::UploadFinish:
	{
		auto receiver = m_uploads.take( transferId );
		QString error;
		const auto success = receiver && receiver->finish( &error );
		delete receiver;
		return success ? FeatureMessage( result ) : fail( error.isEmpty() ? tr( "Invalid upload request." ) : error );
	}

	case Command::UploadCancel:
		delete m_uploads.take( transferId );
		return FeatureMessage( result );

	default:
		break;
	}

	return fail( tr( "Unsupported request." ) );
}



QVariantList FileManagerPlugin::roots() const
{
	QVariantList roots;
	QStringList paths;

	const auto addRoot = [&]( const QString& name, const QString& path, const QString& type ) {
		const auto normalizedPath = QDir::fromNativeSeparators( path );
		if( normalizedPath.isEmpty() || paths.contains( normalizedPath ) || QDir( normalizedPath ).exists() == false )
		{
			return;
		}
		paths.append( normalizedPath );
		roots.append( QVariantMap{ { QStringLiteral("name"), name },
								   { QStringLiteral("path"), normalizedPath },
								   { QStringLiteral("type"), type } } );
	};

	addRoot( tr( "Home" ), QDir::homePath(), QStringLiteral("home") );
	addRoot( tr( "Desktop" ), QStandardPaths::writableLocation( QStandardPaths::DesktopLocation ), QStringLiteral("desktop") );
	addRoot( tr( "Documents" ), QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ), QStringLiteral("documents") );
	addRoot( tr( "Downloads" ), QStandardPaths::writableLocation( QStandardPaths::DownloadLocation ), QStringLiteral("downloads") );
	addRoot( tr( "Pictures" ), QStandardPaths::writableLocation( QStandardPaths::PicturesLocation ), QStringLiteral("pictures") );

	const auto volumes = QStorageInfo::mountedVolumes();
	for( const auto& volume : volumes )
	{
		if( volume.isValid() == false || volume.isReady() == false )
		{
			continue;
		}

		const auto rootPath = volume.rootPath();
#if defined(Q_OS_MACOS)
		if( rootPath != QLatin1String("/") && rootPath.startsWith( QLatin1String("/Volumes/") ) == false )
		{
			continue;
		}
#elif defined(Q_OS_LINUX)
		if( rootPath != QLatin1String("/") &&
			rootPath.startsWith( QLatin1String("/media/") ) == false &&
			rootPath.startsWith( QLatin1String("/run/media/") ) == false &&
			rootPath.startsWith( QLatin1String("/mnt/") ) == false )
		{
			continue;
		}
#endif

		auto name = volume.displayName();
		if( name.isEmpty() || name == rootPath )
		{
			name = QDir::toNativeSeparators( rootPath );
		}
#if defined(Q_OS_WIN)
		else
		{
			name = QStringLiteral("%1 (%2)").arg( name, QDir::toNativeSeparators( rootPath ).remove( QLatin1Char('\\') ) );
		}
#endif

		addRoot( name, rootPath, QStringLiteral("drive") );
	}

	return roots;
}



FeatureMessage FileManagerPlugin::listDirectory( const FeatureMessage& request ) const
{
	const auto path = request.argument( Argument::Path ).toString();
	const QDir dir( path );

	if( dir.exists() == false || dir.isReadable() == false )
	{
		auto reply = replyMessage( request, Command::Result );
		reply.addArgument( Argument::Error, dir.exists() ? tr( "Access to \"%1\" has been denied." ).arg( QDir::toNativeSeparators( path ) )
														 : tr( "The folder \"%1\" does not exist." ).arg( QDir::toNativeSeparators( path ) ) );
		return FeatureMessage( reply );
	}

	QVariantList entries;
	const auto fileInfos = dir.entryInfoList( QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
											  QDir::DirsFirst | QDir::Name | QDir::IgnoreCase );
	entries.reserve( qMin<int>( int(fileInfos.size()), MaximumListingSize ) );
	for( const auto& fileInfo : fileInfos )
	{
		if( entries.size() >= MaximumListingSize )
		{
			break;
		}

		entries.append( QVariantMap{
			{ QStringLiteral("name"), fileInfo.fileName() },
			{ QStringLiteral("dir"), fileInfo.isDir() },
			{ QStringLiteral("size"), fileInfo.isDir() ? qint64(0) : fileInfo.size() },
			{ QStringLiteral("modified"), fileInfo.lastModified() },
			{ QStringLiteral("hidden"), fileInfo.isHidden() },
		} );
	}

	auto reply = replyMessage( request, Command::DirectoryListing );
	reply.addArgument( Argument::Path, QDir::fromNativeSeparators( dir.absolutePath() ) );
	reply.addArgument( Argument::Entries, entries );
	return FeatureMessage( reply );
}



QString FileManagerPlugin::deletePaths( const QStringList& paths ) const
{
	QStringList failed;

	for( const auto& path : paths )
	{
		// move to the trash/recycle bin so accidentally deleted files can be restored by the student
		if( QFile::moveToTrash( path ) == false )
		{
			failed.append( QFileInfo( path ).fileName() );
		}
	}

	if( failed.isEmpty() )
	{
		return {};
	}

	return tr( "The following items could not be moved to the trash: %1" ).arg( failed.join( QStringLiteral(", ") ) );
}



FeatureMessage FileManagerPlugin::replyMessage( const FeatureMessage& request, Command command )
{
	FeatureMessage reply( request.featureUid(), command );
	reply.addArgument( Argument::RequestId, request.argument( Argument::RequestId ) );
	if( request.hasArgument( Argument::TransferId ) )
	{
		reply.addArgument( Argument::TransferId, request.argument( Argument::TransferId ) );
	}
	return FeatureMessage( reply );
}


IMPLEMENT_CONFIG_PROXY(FileManagerConfiguration)
