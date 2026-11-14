/*
 * FileManagerDialog.cpp - implementation of FileManagerDialog class
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
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QStandardPaths>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "AddOnUtils.h"
#include "FileManagerDialog.h"
#include "VeyonCore.h"


namespace {

enum Column
{
	ColumnName,
	ColumnSize,
	ColumnType,
	ColumnModified,
	ColumnCount
};

constexpr auto IsDirectoryRole = Qt::UserRole + 1;
constexpr auto SortRole = Qt::UserRole + 2;

// folders are always sorted before files
class FileItem : public QTreeWidgetItem
{
public:
	using QTreeWidgetItem::QTreeWidgetItem;

	bool operator<( const QTreeWidgetItem& other ) const override
	{
		const auto isDirectory = data( ColumnName, IsDirectoryRole ).toBool();
		const auto otherIsDirectory = other.data( ColumnName, IsDirectoryRole ).toBool();
		const auto ascending = treeWidget() == nullptr || treeWidget()->header()->sortIndicatorOrder() == Qt::AscendingOrder;

		if( isDirectory != otherIsDirectory )
		{
			// keep folders on top regardless of the sort order
			return ascending ? isDirectory : otherIsDirectory;
		}

		const auto column = treeWidget() ? treeWidget()->sortColumn() : ColumnName;
		if( column == ColumnSize || column == ColumnModified )
		{
			return data( column, SortRole ).toLongLong() < other.data( column, SortRole ).toLongLong();
		}

		return text( column ).compare( other.text( column ), Qt::CaseInsensitive ) < 0;
	}
};

// accepts files dropped from the local file manager (Finder/Explorer)
class DropTreeWidget : public QTreeWidget
{
public:
	std::function<void(const QStringList&)> filesDropped;

	DropTreeWidget()
	{
		setAcceptDrops( true );
		setDragDropMode( QAbstractItemView::DropOnly );
		setDropIndicatorShown( false );
	}

protected:
	void dragEnterEvent( QDragEnterEvent* event ) override
	{
		if( event->mimeData()->hasUrls() )
		{
			event->acceptProposedAction();
		}
	}

	void dragMoveEvent( QDragMoveEvent* event ) override
	{
		if( event->mimeData()->hasUrls() )
		{
			event->acceptProposedAction();
		}
	}

	void dropEvent( QDropEvent* event ) override
	{
		QStringList paths;
		const auto urls = event->mimeData()->urls();
		for( const auto& url : urls )
		{
			if( url.isLocalFile() )
			{
				paths.append( url.toLocalFile() );
			}
		}

		if( paths.isEmpty() == false && filesDropped )
		{
			filesDropped( paths );
		}

		event->acceptProposedAction();
	}
};

}



FileManagerDialog::FileManagerDialog( FileManagerPlugin& plugin, const ComputerControlInterfaceList& computerControlInterfaces,
									  QWidget* parent ) :
	QDialog( parent ),
	m_plugin( plugin ),
	m_lastDownloadDirectory( QStandardPaths::writableLocation( QStandardPaths::DownloadLocation ) + QStringLiteral("/Veyon") )
{
	setWindowTitle( tr( "Remote File Manager" ) );
	setWindowIcon( VeyonCore::icon( QStringLiteral("veyon-file-manager") ) );
	resize( 1000, 680 );

	auto mainLayout = new QVBoxLayout( this );

	// navigation bar
	auto navigationLayout = new QHBoxLayout;
	m_computerComboBox = new QComboBox;
	m_computerComboBox->setMinimumWidth( 200 );
	connect( m_computerComboBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this, &FileManagerDialog::currentComputerChanged );
	navigationLayout->addWidget( m_computerComboBox );

	const auto addNavigationButton = [&]( const QString& text, const QString& toolTip, const std::function<void()>& handler ) {
		auto button = new QToolButton;
		button->setText( text );
		button->setToolTip( toolTip );
		connect( button, &QToolButton::clicked, this, handler );
		navigationLayout->addWidget( button );
	};
	addNavigationButton( QStringLiteral("◀"), tr( "Back" ), [this]() { navigateBack(); } );
	addNavigationButton( QStringLiteral("▲"), tr( "Up" ), [this]() { navigateUp(); } );
	addNavigationButton( QStringLiteral("⟳"), tr( "Refresh" ), [this]() { refresh(); } );

	m_pathEdit = new QLineEdit;
	connect( m_pathEdit, &QLineEdit::returnPressed, this, [this]() {
		navigateTo( QDir::fromNativeSeparators( m_pathEdit->text().trimmed() ) );
	} );
	navigationLayout->addWidget( m_pathEdit, 1 );
	mainLayout->addLayout( navigationLayout );

	// places and files
	auto splitter = new QSplitter( Qt::Horizontal );
	m_placesList = new QListWidget;
	m_placesList->setIconSize( QSize( 18, 18 ) );
	connect( m_placesList, &QListWidget::itemClicked, this, [this]( QListWidgetItem* item ) {
		navigateTo( item->data( Qt::UserRole ).toString() );
	} );
	splitter->addWidget( m_placesList );

	auto fileTree = new DropTreeWidget;
	fileTree->filesDropped = [this]( const QStringList& paths ) { uploadPaths( paths ); };
	m_fileTree = fileTree;
	m_fileTree->setColumnCount( ColumnCount );
	m_fileTree->setHeaderLabels( { tr( "Name" ), tr( "Size" ), tr( "Type" ), tr( "Modified" ) } );
	m_fileTree->setRootIsDecorated( false );
	m_fileTree->setUniformRowHeights( true );
	m_fileTree->setSelectionMode( QAbstractItemView::ExtendedSelection );
	m_fileTree->setSortingEnabled( true );
	m_fileTree->sortByColumn( ColumnName, Qt::AscendingOrder );
	m_fileTree->header()->setSectionResizeMode( ColumnName, QHeaderView::Stretch );
	m_fileTree->setContextMenuPolicy( Qt::CustomContextMenu );
	connect( m_fileTree, &QTreeWidget::itemDoubleClicked, this, [this]( QTreeWidgetItem* item ) {
		if( item->data( ColumnName, IsDirectoryRole ).toBool() )
		{
			navigateTo( remotePath( item->text( ColumnName ) ) );
		}
		else
		{
			viewSelectedFile();
		}
	} );
	connect( m_fileTree, &QTreeWidget::customContextMenuRequested, this, [this]( const QPoint& pos ) {
		QMenu menu;
		menu.addAction( tr( "View on this computer" ), this, &FileManagerDialog::viewSelectedFile );
		menu.addAction( tr( "Download…" ), this, &FileManagerDialog::downloadSelected );
		menu.addAction( tr( "Open on student computer" ), this, &FileManagerDialog::openSelected );
		if( m_readOnly == false )
		{
			menu.addSeparator();
			menu.addAction( tr( "Rename…" ), this, &FileManagerDialog::renameSelected );
			menu.addAction( tr( "Move to trash" ), this, &FileManagerDialog::deleteSelected );
		}
		menu.exec( m_fileTree->viewport()->mapToGlobal( pos ) );
	} );
	splitter->addWidget( m_fileTree );
	splitter->setStretchFactor( 0, 1 );
	splitter->setStretchFactor( 1, 4 );
	mainLayout->addWidget( splitter, 1 );

	// actions
	auto actionLayout = new QHBoxLayout;
	const auto addActionButton = [&]( const QString& text, const std::function<void()>& handler, bool modifying ) {
		auto button = new QPushButton( text );
		connect( button, &QPushButton::clicked, this, handler );
		actionLayout->addWidget( button );
		if( modifying )
		{
			m_modifyingWidgets.append( button );
		}
	};
	addActionButton( tr( "New folder…" ), [this]() { createFolder(); }, true );
	addActionButton( tr( "Upload files…" ), [this]() { uploadFiles(); }, true );
	addActionButton( tr( "Download…" ), [this]() { downloadSelected(); }, false );
	addActionButton( tr( "Open on student computer" ), [this]() { openSelected(); }, false );
	addActionButton( tr( "Rename…" ), [this]() { renameSelected(); }, true );
	addActionButton( tr( "Move to trash" ), [this]() { deleteSelected(); }, true );
	actionLayout->addStretch( 1 );
	m_showHiddenCheckBox = new QCheckBox( tr( "Show hidden files" ) );
	connect( m_showHiddenCheckBox, &QCheckBox::toggled, this, &FileManagerDialog::renderListing );
	actionLayout->addWidget( m_showHiddenCheckBox );
	mainLayout->addLayout( actionLayout );

	// status and transfers
	auto statusLayout = new QHBoxLayout;
	m_statusLabel = new QLabel;
	m_statusLabel->setWordWrap( true );
	statusLayout->addWidget( m_statusLabel, 1 );
	m_progressBar = new QProgressBar;
	m_progressBar->setMaximumWidth( 250 );
	m_progressBar->hide();
	statusLayout->addWidget( m_progressBar );
	m_cancelTransferButton = new QPushButton( tr( "Cancel" ) );
	m_cancelTransferButton->hide();
	connect( m_cancelTransferButton, &QPushButton::clicked, this, &FileManagerDialog::cancelTransfers );
	statusLayout->addWidget( m_cancelTransferButton );
	mainLayout->addLayout( statusLayout );

	auto hintLabel = new QLabel( tr( "Drag files from your computer into the file list to upload them. "
									 "Only files the logged on student can access are shown." ) );
	hintLabel->setWordWrap( true );
	auto hintFont = hintLabel->font();
	hintFont.setPointSizeF( hintFont.pointSizeF() * 0.9 );
	hintLabel->setFont( hintFont );
	mainLayout->addWidget( hintLabel );

	auto buttonBox = new QDialogButtonBox( QDialogButtonBox::Close );
	connect( buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject );
	mainLayout->addWidget( buttonBox );

	connect( &m_plugin, &FileManagerPlugin::replyReceived, this, &FileManagerDialog::handleReply );

	connect( &m_transferWatchdog, &QTimer::timeout, this, &FileManagerDialog::checkTransferTimeout );
	m_transferWatchdog.start( 5000 );

	setComputerControlInterfaces( computerControlInterfaces );
}



FileManagerDialog::~FileManagerDialog()
{
	m_downloadReceiver.cancel();
}



void FileManagerDialog::setComputerControlInterfaces( const ComputerControlInterfaceList& computerControlInterfaces )
{
	m_computerControlInterfaces = computerControlInterfaces;

	m_computerComboBox->blockSignals( true );
	m_computerComboBox->clear();
	for( const auto& computer : computerControlInterfaces )
	{
		m_computerComboBox->addItem( AddOnUtils::computerDisplayName( computer ), AddOnUtils::computerKey( computer ) );
	}
	m_computerComboBox->blockSignals( false );

	currentComputerChanged();
}



void FileManagerDialog::currentComputerChanged()
{
	m_currentPath.clear();
	m_currentEntries.clear();
	m_history.clear();
	m_placesList->clear();
	m_fileTree->clear();
	m_pathEdit->clear();

	const auto computer = currentComputer();
	if( computer.isNull() )
	{
		m_statusLabel->setText( tr( "No computer selected" ) );
		return;
	}

	if( computer->state() != ComputerControlInterface::State::Connected )
	{
		m_statusLabel->setText( tr( "%1 is not connected." ).arg( computer->computerName() ) );
		return;
	}

	m_statusLabel->setText( tr( "Loading…" ) );
	sendRequest( Command::ListRoots, {}, RequestPurpose::Roots );
}



ComputerControlInterface::Pointer FileManagerDialog::currentComputer() const
{
	return AddOnUtils::findComputer( m_computerControlInterfaces, m_computerComboBox->currentData().toString() );
}



void FileManagerDialog::navigateTo( const QString& path, bool addToHistory )
{
	if( path.isEmpty() )
	{
		return;
	}

	m_pendingNavigationPath = path;
	m_pendingNavigationAddsHistory = addToHistory;
	sendRequest( Command::ListDirectory, { { Argument::Path, path } }, RequestPurpose::Listing );
}



void FileManagerDialog::navigateBack()
{
	if( m_history.isEmpty() == false )
	{
		navigateTo( m_history.takeLast(), false );
	}
}



void FileManagerDialog::navigateUp()
{
	QDir dir( m_currentPath );
	if( m_currentPath.isEmpty() || dir.cdUp() == false )
	{
		return;
	}

	navigateTo( dir.absolutePath() );
}



void FileManagerDialog::refresh()
{
	navigateTo( m_currentPath, false );
}



void FileManagerDialog::createFolder()
{
	bool ok = false;
	const auto name = QInputDialog::getText( this, tr( "New folder" ), tr( "Name of the new folder" ),
											 QLineEdit::Normal, {}, &ok ).trimmed();
	if( ok && name.isEmpty() == false )
	{
		sendRequest( Command::CreateDirectory, { { Argument::Path, m_currentPath }, { Argument::Name, name } },
					 RequestPurpose::Operation );
	}
}



void FileManagerDialog::renameSelected()
{
	const auto items = m_fileTree->selectedItems();
	if( items.size() != 1 )
	{
		return;
	}

	const auto oldName = items.first()->text( ColumnName );
	bool ok = false;
	const auto newName = QInputDialog::getText( this, tr( "Rename" ), tr( "New name" ), QLineEdit::Normal, oldName, &ok ).trimmed();
	if( ok && newName.isEmpty() == false && newName != oldName )
	{
		sendRequest( Command::Rename, { { Argument::Path, remotePath( oldName ) }, { Argument::Name, newName } },
					 RequestPurpose::Operation );
	}
}



void FileManagerDialog::deleteSelected()
{
	const auto paths = selectedRemotePaths();
	if( paths.isEmpty() )
	{
		return;
	}

	if( QMessageBox::question( this, windowTitle(),
							   tr( "Do you really want to move %n item(s) to the trash on %1?", "", int(paths.size()) )
							   .arg( currentComputer()->computerName() ) ) != QMessageBox::Yes )
	{
		return;
	}

	sendRequest( Command::Delete, { { Argument::Paths, paths } }, RequestPurpose::Operation );
}



void FileManagerDialog::openSelected()
{
	const auto items = m_fileTree->selectedItems();
	for( const auto* item : items )
	{
		sendRequest( Command::OpenFile, { { Argument::Path, remotePath( item->text( ColumnName ) ) } }, RequestPurpose::Operation );
	}
}



void FileManagerDialog::uploadFiles()
{
	const auto files = QFileDialog::getOpenFileNames( this, tr( "Select files to upload" ),
													  QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) );
	if( files.isEmpty() == false )
	{
		uploadPaths( files );
	}
}



void FileManagerDialog::uploadPaths( const QStringList& localPaths )
{
	const auto computer = currentComputer();
	if( computer.isNull() || m_currentPath.isEmpty() )
	{
		return;
	}

	if( m_readOnly )
	{
		QMessageBox::information( this, windowTitle(), tr( "Changing files on this computer has been disabled by the administrator." ) );
		return;
	}

	// ask once whether existing files and folders should be replaced
	QStringList existingNames;
	for( const auto& localPath : localPaths )
	{
		const auto name = QFileInfo( localPath ).fileName();
		for( const auto& entry : std::as_const(m_currentEntries) )
		{
			if( entry.toMap().value( QStringLiteral("name") ).toString().compare( name, Qt::CaseInsensitive ) == 0 )
			{
				existingNames.append( name );
			}
		}
	}

	bool overwrite = false;
	if( existingNames.isEmpty() == false )
	{
		const auto answer = QMessageBox::question( this, windowTitle(),
												   tr( "The following items already exist on %1. Replace them?\n\n%2" )
												   .arg( computer->computerName(), existingNames.join( QLatin1Char('\n') ) ),
												   QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel );
		if( answer == QMessageBox::Cancel )
		{
			return;
		}
		overwrite = answer == QMessageBox::Yes;
	}

	for( const auto& localPath : localPaths )
	{
		const QFileInfo fileInfo( localPath );
		if( fileInfo.isDir() )
		{
			QDirIterator it( localPath, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories );
			while( it.hasNext() )
			{
				const auto filePath = it.next();
				Transfer transfer;
				transfer.type = Transfer::Type::Upload;
				transfer.computer = computer;
				transfer.localPath = filePath;
				transfer.remotePath = m_currentPath;
				transfer.name = fileInfo.fileName() + QLatin1Char('/') + QDir( localPath ).relativeFilePath( filePath );
				transfer.overwrite = overwrite;
				enqueueTransfer( transfer );
			}
		}
		else if( fileInfo.isFile() )
		{
			Transfer transfer;
			transfer.type = Transfer::Type::Upload;
			transfer.computer = computer;
			transfer.localPath = localPath;
			transfer.remotePath = m_currentPath;
			transfer.name = fileInfo.fileName();
			transfer.overwrite = overwrite;
			enqueueTransfer( transfer );
		}
	}
}



void FileManagerDialog::downloadSelected()
{
	const auto computer = currentComputer();
	const auto items = m_fileTree->selectedItems();
	if( computer.isNull() || items.isEmpty() )
	{
		return;
	}

	const auto destination = QFileDialog::getExistingDirectory( this, tr( "Download to" ),
																QDir( m_lastDownloadDirectory ).exists() ? m_lastDownloadDirectory
																										 : QStandardPaths::writableLocation( QStandardPaths::DownloadLocation ) );
	if( destination.isEmpty() )
	{
		return;
	}
	m_lastDownloadDirectory = destination;

	for( const auto* item : items )
	{
		const auto name = item->text( ColumnName );

		Transfer transfer;
		transfer.computer = computer;
		transfer.remotePath = remotePath( name );
		transfer.name = name;

		if( item->data( ColumnName, IsDirectoryRole ).toBool() )
		{
			transfer.type = Transfer::Type::ListForDownload;
			transfer.localPath = FileReceiver::uniqueFilePath( destination, name );
		}
		else
		{
			transfer.type = Transfer::Type::Download;
			transfer.localPath = FileReceiver::uniqueFilePath( destination, name );
			transfer.size = item->data( ColumnSize, SortRole ).toLongLong();
		}

		enqueueTransfer( transfer );
	}
}



void FileManagerDialog::viewSelectedFile()
{
	const auto computer = currentComputer();
	const auto items = m_fileTree->selectedItems();
	if( computer.isNull() || items.size() != 1 || items.first()->data( ColumnName, IsDirectoryRole ).toBool() )
	{
		return;
	}

	const auto name = items.first()->text( ColumnName );
	const auto directory = QDir::tempPath() + QStringLiteral("/Veyon File Manager/") +
						   FileReceiver::sanitizeRelativePath( computer->computerName() );
	QDir().mkpath( directory );

	Transfer transfer;
	transfer.type = Transfer::Type::Download;
	transfer.computer = computer;
	transfer.remotePath = remotePath( name );
	transfer.name = name;
	transfer.localPath = FileReceiver::uniqueFilePath( directory, name );
	transfer.size = items.first()->data( ColumnSize, SortRole ).toLongLong();
	transfer.openLocally = true;
	enqueueTransfer( transfer );
}



void FileManagerDialog::renderListing()
{
	m_fileTree->setUpdatesEnabled( false );
	m_fileTree->setSortingEnabled( false );
	m_fileTree->clear();

	const QFileIconProvider iconProvider;
	const auto folderIcon = iconProvider.icon( QFileIconProvider::Folder );
	const auto fileIcon = iconProvider.icon( QFileIconProvider::File );
	const auto showHidden = m_showHiddenCheckBox->isChecked();

	int fileCount = 0;
	int folderCount = 0;

	for( const auto& entryValue : std::as_const(m_currentEntries) )
	{
		const auto entry = entryValue.toMap();
		if( showHidden == false && entry.value( QStringLiteral("hidden") ).toBool() )
		{
			continue;
		}

		const auto name = entry.value( QStringLiteral("name") ).toString();
		const auto isDirectory = entry.value( QStringLiteral("dir") ).toBool();
		const auto size = entry.value( QStringLiteral("size") ).toLongLong();
		const auto modified = entry.value( QStringLiteral("modified") ).toDateTime();

		auto item = new FileItem;
		item->setText( ColumnName, name );
		item->setIcon( ColumnName, isDirectory ? folderIcon : fileIcon );
		item->setData( ColumnName, IsDirectoryRole, isDirectory );
		item->setText( ColumnSize, isDirectory ? QString() : AddOnUtils::formatFileSize( size ) );
		item->setData( ColumnSize, SortRole, size );
		item->setTextAlignment( ColumnSize, Qt::AlignRight | Qt::AlignVCenter );
		const auto suffix = QFileInfo( name ).suffix();
		item->setText( ColumnType, isDirectory ? tr( "Folder" ) :
								   suffix.isEmpty() ? tr( "File" ) : tr( "%1 file" ).arg( suffix.toUpper() ) );
		item->setText( ColumnModified, QLocale().toString( modified, QLocale::ShortFormat ) );
		item->setData( ColumnModified, SortRole, modified.toMSecsSinceEpoch() );
		m_fileTree->addTopLevelItem( item );

		if( isDirectory )
		{
			++folderCount;
		}
		else
		{
			++fileCount;
		}
	}

	m_fileTree->setSortingEnabled( true );
	m_fileTree->setUpdatesEnabled( true );

	if( m_transferActive == false )
	{
		m_statusLabel->setText( tr( "%1 folders, %2 files" ).arg( folderCount ).arg( fileCount ) );
	}
}



QUuid FileManagerDialog::sendRequest( Command command, const QMap<Argument, QVariant>& arguments, RequestPurpose purpose,
									  const ComputerControlInterface::Pointer& computer )
{
	const auto targetComputer = computer.isNull() ? currentComputer() : computer;
	if( targetComputer.isNull() || targetComputer->state() != ComputerControlInterface::State::Connected )
	{
		return {};
	}

	const auto requestId = m_plugin.sendRequest( targetComputer, command, arguments );
	m_pendingRequests[requestId] = purpose;
	return requestId;
}



void FileManagerDialog::handleReply( ComputerControlInterface::Pointer computerControlInterface, int command,
									 const QVariantMap& arguments )
{
	Q_UNUSED(computerControlInterface)

	const auto requestId = FileManagerPlugin::argument( arguments, Argument::RequestId ).toUuid();
	const auto it = m_pendingRequests.find( requestId );
	if( it == m_pendingRequests.end() )
	{
		return;
	}

	const auto purpose = it.value();
	m_pendingRequests.erase( it );

	const auto replyCommand = Command( command );
	const auto error = FileManagerPlugin::argument( arguments, Argument::Error ).toString();

	switch( purpose )
	{
	case RequestPurpose::Roots:
		if( error.isEmpty() == false )
		{
			m_statusLabel->setText( error );
			return;
		}
		m_readOnly = FileManagerPlugin::argument( arguments, Argument::ReadOnly ).toBool();
		for( auto widget : std::as_const(m_modifyingWidgets) )
		{
			widget->setEnabled( m_readOnly == false );
		}
		updatePlaces( FileManagerPlugin::argument( arguments, Argument::Entries ).toList() );
		if( m_currentPath.isEmpty() && m_placesList->count() > 0 )
		{
			navigateTo( m_placesList->item( 0 )->data( Qt::UserRole ).toString(), false );
		}
		break;

	case RequestPurpose::Listing:
		if( error.isEmpty() == false )
		{
			m_statusLabel->setText( error );
			m_pathEdit->setText( QDir::toNativeSeparators( m_currentPath ) );
			return;
		}
		if( m_pendingNavigationAddsHistory && m_currentPath.isEmpty() == false &&
			m_currentPath != FileManagerPlugin::argument( arguments, Argument::Path ).toString() )
		{
			m_history.append( m_currentPath );
		}
		updateListing( FileManagerPlugin::argument( arguments, Argument::Path ).toString(),
					   FileManagerPlugin::argument( arguments, Argument::Entries ).toList() );
		break;

	case RequestPurpose::Operation:
		if( error.isEmpty() == false )
		{
			QMessageBox::warning( this, windowTitle(), error );
		}
		refresh();
		break;

	case RequestPurpose::Transfer:
		if( requestId == m_currentTransferRequest )
		{
			handleTransferReply( replyCommand, arguments );
		}
		break;

	case RequestPurpose::RecursiveListing:
		if( requestId == m_currentTransferRequest )
		{
			handleRecursiveListing( replyCommand, arguments );
		}
		break;

	case RequestPurpose::Ignore:
		break;
	}
}



void FileManagerDialog::handleTransferReply( Command command, const QVariantMap& arguments )
{
	m_lastTransferActivity.restart();

	const auto error = FileManagerPlugin::argument( arguments, Argument::Error ).toString();
	if( error.isEmpty() == false )
	{
		finishCurrentTransfer( error );
		return;
	}

	if( m_currentTransfer.type == Transfer::Type::Upload )
	{
		if( m_uploadFile.isOpen() )
		{
			if( m_uploadFile.atEnd() )
			{
				m_uploadFile.close();
				m_currentTransferRequest = sendRequest( Command::UploadFinish, { { Argument::TransferId, m_currentTransfer.transferId } },
														RequestPurpose::Transfer, m_currentTransfer.computer );
			}
			else
			{
				continueUpload();
			}
		}
		else
		{
			// UploadFinish acknowledged
			finishCurrentTransfer();
		}
		return;
	}

	switch( command )
	{
	case Command::DownloadInfo:
	{
		m_currentTransfer.size = FileManagerPlugin::argument( arguments, Argument::Size ).toLongLong();
		QString receiverError;
		if( m_downloadReceiver.begin( m_currentTransfer.localPath, false, &receiverError ) == false )
		{
			finishCurrentTransfer( receiverError );
			return;
		}
		m_currentTransferRequest = sendRequest( Command::DownloadChunk, { { Argument::TransferId, m_currentTransfer.transferId },
																		  { Argument::Offset, 0 } },
												RequestPurpose::Transfer, m_currentTransfer.computer );
		break;
	}

	case Command::DownloadData:
	{
		const auto data = FileManagerPlugin::argument( arguments, Argument::Data ).toByteArray();
		const auto offset = FileManagerPlugin::argument( arguments, Argument::Offset ).toLongLong();
		QString receiverError;
		if( m_downloadReceiver.write( data, &receiverError ) == false )
		{
			finishCurrentTransfer( receiverError );
			return;
		}
		m_currentTransfer.transferred = offset + data.size();
		updateTransferStatus();

		if( FileManagerPlugin::argument( arguments, Argument::Final ).toBool() || data.isEmpty() )
		{
			sendRequest( Command::DownloadEnd, { { Argument::TransferId, m_currentTransfer.transferId } },
						 RequestPurpose::Ignore, m_currentTransfer.computer );
			finishCurrentTransfer( m_downloadReceiver.finish( &receiverError ) ? QString() : receiverError );
		}
		else
		{
			m_currentTransferRequest = sendRequest( Command::DownloadChunk, { { Argument::TransferId, m_currentTransfer.transferId },
																			  { Argument::Offset, m_currentTransfer.transferred } },
													RequestPurpose::Transfer, m_currentTransfer.computer );
		}
		break;
	}

	default:
		finishCurrentTransfer( tr( "Unexpected reply" ) );
		break;
	}
}



void FileManagerDialog::handleRecursiveListing( Command command, const QVariantMap& arguments )
{
	const auto error = FileManagerPlugin::argument( arguments, Argument::Error ).toString();
	if( error.isEmpty() == false || command != Command::DirectoryListing )
	{
		finishCurrentTransfer( error.isEmpty() ? tr( "Unexpected reply" ) : error );
		return;
	}

	QDir().mkpath( m_currentTransfer.localPath );

	const auto entries = FileManagerPlugin::argument( arguments, Argument::Entries ).toList();
	for( const auto& entryValue : entries )
	{
		const auto entry = entryValue.toMap();
		const auto name = entry.value( QStringLiteral("name") ).toString();

		Transfer transfer;
		transfer.computer = m_currentTransfer.computer;
		transfer.remotePath = m_currentTransfer.remotePath + QLatin1Char('/') + name;
		transfer.localPath = QDir( m_currentTransfer.localPath ).filePath( name );
		transfer.name = name;

		if( entry.value( QStringLiteral("dir") ).toBool() )
		{
			transfer.type = Transfer::Type::ListForDownload;
		}
		else
		{
			transfer.type = Transfer::Type::Download;
			transfer.size = entry.value( QStringLiteral("size") ).toLongLong();
		}

		m_transferQueue.append( transfer );
	}

	finishCurrentTransfer();
}



void FileManagerDialog::updateListing( const QString& path, const QVariantList& entries )
{
	m_currentPath = path;
	m_currentEntries = entries;
	m_pathEdit->setText( QDir::toNativeSeparators( path ) );
	renderListing();
}



void FileManagerDialog::updatePlaces( const QVariantList& roots )
{
	const QFileIconProvider iconProvider;

	m_placesList->clear();
	for( const auto& rootValue : roots )
	{
		const auto root = rootValue.toMap();
		const auto type = root.value( QStringLiteral("type") ).toString();
		auto item = new QListWidgetItem( root.value( QStringLiteral("name") ).toString() );
		item->setData( Qt::UserRole, root.value( QStringLiteral("path") ).toString() );
		item->setToolTip( QDir::toNativeSeparators( root.value( QStringLiteral("path") ).toString() ) );
		item->setIcon( iconProvider.icon( type == QLatin1String("drive") ? QFileIconProvider::Drive :
										  type == QLatin1String("desktop") ? QFileIconProvider::Desktop :
																			 QFileIconProvider::Folder ) );
		m_placesList->addItem( item );
	}
}



void FileManagerDialog::enqueueTransfer( const Transfer& transfer )
{
	m_transferQueue.append( transfer );
	processNextTransfer();
}



void FileManagerDialog::processNextTransfer()
{
	if( m_transferActive )
	{
		return;
	}

	if( m_transferQueue.isEmpty() )
	{
		m_progressBar->hide();
		m_cancelTransferButton->hide();

		if( m_completedTransfers > 0 || m_transferErrors.isEmpty() == false )
		{
			m_statusLabel->setText( tr( "%n file(s) transferred", "", m_completedTransfers ) );
		}

		if( m_transferErrors.isEmpty() == false )
		{
			QMessageBox::warning( this, windowTitle(), tr( "Some files could not be transferred:\n\n%1" )
								  .arg( m_transferErrors.mid( 0, 20 ).join( QLatin1Char('\n') ) ) );
		}

		m_completedTransfers = 0;
		m_transferErrors.clear();
		refresh();
		return;
	}

	m_currentTransfer = m_transferQueue.takeFirst();
	m_transferActive = true;
	m_lastTransferActivity.restart();

	switch( m_currentTransfer.type )
	{
	case Transfer::Type::Upload:
		m_uploadFile.setFileName( m_currentTransfer.localPath );
		if( m_uploadFile.open( QFile::ReadOnly ) == false )
		{
			finishCurrentTransfer( m_uploadFile.errorString() );
			return;
		}
		m_currentTransfer.size = m_uploadFile.size();
		m_currentTransferRequest = sendRequest( Command::UploadStart, {
													{ Argument::TransferId, m_currentTransfer.transferId },
													{ Argument::Path, m_currentTransfer.remotePath },
													{ Argument::Name, m_currentTransfer.name },
													{ Argument::Overwrite, m_currentTransfer.overwrite } },
												RequestPurpose::Transfer, m_currentTransfer.computer );
		break;

	case Transfer::Type::Download:
		m_currentTransferRequest = sendRequest( Command::DownloadStart, {
													{ Argument::TransferId, m_currentTransfer.transferId },
													{ Argument::Path, m_currentTransfer.remotePath } },
												RequestPurpose::Transfer, m_currentTransfer.computer );
		break;

	case Transfer::Type::ListForDownload:
		m_currentTransferRequest = sendRequest( Command::ListDirectory, { { Argument::Path, m_currentTransfer.remotePath } },
												RequestPurpose::RecursiveListing, m_currentTransfer.computer );
		break;
	}

	if( m_currentTransferRequest.isNull() )
	{
		finishCurrentTransfer( tr( "The computer is not connected." ) );
		return;
	}

	updateTransferStatus();
}



void FileManagerDialog::continueUpload()
{
	const auto data = m_uploadFile.read( FileManagerPlugin::ChunkSize );
	m_currentTransfer.transferred += data.size();
	m_currentTransferRequest = sendRequest( Command::UploadChunk, { { Argument::TransferId, m_currentTransfer.transferId },
																	{ Argument::Data, data } },
											RequestPurpose::Transfer, m_currentTransfer.computer );
	updateTransferStatus();
}



void FileManagerDialog::finishCurrentTransfer( const QString& error )
{
	if( m_transferActive == false )
	{
		return;
	}

	if( error.isEmpty() == false )
	{
		m_transferErrors.append( QStringLiteral("%1: %2").arg( m_currentTransfer.name, error ) );

		if( m_currentTransfer.type == Transfer::Type::Download )
		{
			m_downloadReceiver.cancel();
		}
		else if( m_currentTransfer.type == Transfer::Type::Upload )
		{
			sendRequest( Command::UploadCancel, { { Argument::TransferId, m_currentTransfer.transferId } },
						 RequestPurpose::Ignore, m_currentTransfer.computer );
		}
	}
	else if( m_currentTransfer.type != Transfer::Type::ListForDownload )
	{
		++m_completedTransfers;

		if( m_currentTransfer.openLocally )
		{
			QDesktopServices::openUrl( QUrl::fromLocalFile( m_currentTransfer.localPath ) );
		}
	}

	m_uploadFile.close();
	m_transferActive = false;
	m_currentTransferRequest = QUuid();

	QTimer::singleShot( 0, this, &FileManagerDialog::processNextTransfer );
}



void FileManagerDialog::cancelTransfers()
{
	m_transferQueue.clear();
	finishCurrentTransfer( tr( "cancelled" ) );
	m_transferErrors.clear();
}



void FileManagerDialog::checkTransferTimeout()
{
	if( m_transferActive && m_lastTransferActivity.elapsed() > RequestTimeout )
	{
		finishCurrentTransfer( tr( "The computer did not respond." ) );
	}
}



void FileManagerDialog::updateTransferStatus()
{
	if( m_transferActive == false )
	{
		return;
	}

	m_progressBar->show();
	m_cancelTransferButton->show();

	if( m_currentTransfer.size > 0 )
	{
		m_progressBar->setRange( 0, 1000 );
		m_progressBar->setValue( int( 1000 * m_currentTransfer.transferred / m_currentTransfer.size ) );
	}
	else
	{
		m_progressBar->setRange( 0, 0 );
	}

	QString action;
	switch( m_currentTransfer.type )
	{
	case Transfer::Type::Upload: action = tr( "Uploading %1" ); break;
	case Transfer::Type::Download: action = tr( "Downloading %1" ); break;
	case Transfer::Type::ListForDownload: action = tr( "Scanning %1" ); break;
	}

	auto text = action.arg( m_currentTransfer.name );
	if( m_transferQueue.isEmpty() == false )
	{
		text += QLatin1Char(' ') + tr( "(%n more in queue)", "", int(m_transferQueue.size()) );
	}
	m_statusLabel->setText( text );
}



QStringList FileManagerDialog::selectedRemotePaths( bool* containsFolders ) const
{
	QStringList paths;
	bool folders = false;
	const auto items = m_fileTree->selectedItems();
	for( const auto* item : items )
	{
		paths.append( remotePath( item->text( ColumnName ) ) );
		folders |= item->data( ColumnName, IsDirectoryRole ).toBool();
	}

	if( containsFolders )
	{
		*containsFolders = folders;
	}

	return paths;
}



QString FileManagerDialog::remotePath( const QString& name ) const
{
	if( m_currentPath.endsWith( QLatin1Char('/') ) )
	{
		return m_currentPath + name;
	}

	return m_currentPath + QLatin1Char('/') + name;
}
