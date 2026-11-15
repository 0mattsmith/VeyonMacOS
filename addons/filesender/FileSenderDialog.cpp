/*
 * FileSenderDialog.cpp - implementation of FileSenderDialog class
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
#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "AddOnUtils.h"
#include "FileSenderDialog.h"
#include "VeyonCore.h"


namespace {

// accepts files dropped from the local file manager (Finder/Explorer)
class DropTreeWidget : public QTreeWidget
{
public:
	std::function<void(const QStringList&)> filesDropped;

	DropTreeWidget()
	{
		setAcceptDrops( true );
		setDragDropMode( QAbstractItemView::DropOnly );
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



FileSenderDialog::FileSenderDialog( FileSenderPlugin& plugin, const ComputerControlInterfaceList& computerControlInterfaces,
									QWidget* parent ) :
	QDialog( parent ),
	m_plugin( plugin )
{
	setWindowTitle( tr( "Send files" ) );
	setWindowIcon( VeyonCore::icon( QStringLiteral("veyon-file-send") ) );
	resize( 820, 720 );

	auto mainLayout = new QVBoxLayout( this );

	// files
	auto filesGroup = new QGroupBox( tr( "Files and folders" ) );
	auto filesLayout = new QVBoxLayout( filesGroup );
	auto fileTree = new DropTreeWidget;
	fileTree->filesDropped = [this]( const QStringList& paths ) { addPaths( paths ); };
	m_fileTree = fileTree;
	m_fileTree->setHeaderLabels( { tr( "Name" ), tr( "Size" ) } );
	m_fileTree->setRootIsDecorated( false );
	m_fileTree->setSelectionMode( QAbstractItemView::ExtendedSelection );
	m_fileTree->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
	m_fileTree->setMinimumHeight( 140 );
	filesLayout->addWidget( m_fileTree );

	auto fileButtonsLayout = new QHBoxLayout;
	m_summaryLabel = new QLabel( tr( "Drag files or folders here or use the buttons." ) );
	fileButtonsLayout->addWidget( m_summaryLabel, 1 );
	auto addFilesButton = new QPushButton( tr( "Add files…" ) );
	connect( addFilesButton, &QPushButton::clicked, this, &FileSenderDialog::addFiles );
	fileButtonsLayout->addWidget( addFilesButton );
	auto addFolderButton = new QPushButton( tr( "Add folder…" ) );
	connect( addFolderButton, &QPushButton::clicked, this, &FileSenderDialog::addFolder );
	fileButtonsLayout->addWidget( addFolderButton );
	auto removeButton = new QPushButton( tr( "Remove" ) );
	connect( removeButton, &QPushButton::clicked, this, &FileSenderDialog::removeSelectedFiles );
	fileButtonsLayout->addWidget( removeButton );
	filesLayout->addLayout( fileButtonsLayout );
	mainLayout->addWidget( filesGroup );

	// destination and options
	auto optionsGroup = new QGroupBox( tr( "Destination on student computers" ) );
	auto optionsLayout = new QFormLayout( optionsGroup );

	m_destinationComboBox = new QComboBox;
	m_destinationComboBox->addItem( tr( "Desktop" ), int(FileSenderPlugin::Destination::Desktop) );
	m_destinationComboBox->addItem( tr( "Documents" ), int(FileSenderPlugin::Destination::Documents) );
	m_destinationComboBox->addItem( tr( "Downloads" ), int(FileSenderPlugin::Destination::Downloads) );
	m_destinationComboBox->addItem( tr( "Documents › From teacher" ), int(FileSenderPlugin::Destination::FromTeacher) );
	m_destinationComboBox->addItem( tr( "Custom folder" ), int(FileSenderPlugin::Destination::Custom) );
	m_destinationComboBox->setCurrentIndex( 3 );
	optionsLayout->addRow( tr( "Folder" ), m_destinationComboBox );

	m_customPathEdit = new QLineEdit;
	m_customPathEdit->setPlaceholderText( tr( "e.g. Documents/Science or C:/Classwork (relative to the student's home folder)" ) );
	m_customPathEdit->setEnabled( false );
	optionsLayout->addRow( tr( "Custom folder" ), m_customPathEdit );
	connect( m_destinationComboBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this, [this]() {
		m_customPathEdit->setEnabled( m_destinationComboBox->currentData().toInt() == int(FileSenderPlugin::Destination::Custom) );
	} );

	m_subfolderEdit = new QLineEdit;
	m_subfolderEdit->setPlaceholderText( tr( "optional, e.g. Lesson 5" ) );
	optionsLayout->addRow( tr( "Subfolder" ), m_subfolderEdit );

	m_existingFilesComboBox = new QComboBox;
	m_existingFilesComboBox->addItem( tr( "Keep both (rename new file)" ), int(FileSenderPlugin::ExistingFiles::Rename) );
	m_existingFilesComboBox->addItem( tr( "Replace" ), int(FileSenderPlugin::ExistingFiles::Replace) );
	m_existingFilesComboBox->addItem( tr( "Skip" ), int(FileSenderPlugin::ExistingFiles::Skip) );
	optionsLayout->addRow( tr( "Existing files" ), m_existingFilesComboBox );

	m_openFilesCheckBox = new QCheckBox( tr( "Open the files after receiving them (up to %1 files)" ).arg( MaximumFilesToOpen ) );
	m_openFolderCheckBox = new QCheckBox( tr( "Open the destination folder" ) );
	m_showMessageCheckBox = new QCheckBox( tr( "Show a message to the students:" ) );
	m_showMessageCheckBox->setChecked( true );
	m_messageEdit = new QLineEdit;
	m_messageEdit->setPlaceholderText( tr( "Default: \"Your teacher has sent you … files.\"" ) );
	connect( m_showMessageCheckBox, &QCheckBox::toggled, m_messageEdit, &QLineEdit::setEnabled );
	optionsLayout->addRow( QString(), m_openFilesCheckBox );
	optionsLayout->addRow( QString(), m_openFolderCheckBox );
	auto messageLayout = new QHBoxLayout;
	messageLayout->addWidget( m_showMessageCheckBox );
	messageLayout->addWidget( m_messageEdit, 1 );
	optionsLayout->addRow( QString(), messageLayout );
	mainLayout->addWidget( optionsGroup );

	// computers
	auto computersGroup = new QGroupBox( tr( "Computers" ) );
	auto computersLayout = new QVBoxLayout( computersGroup );
	m_computerTree = new QTreeWidget;
	m_computerTree->setHeaderLabels( { tr( "Computer" ), tr( "Status" ), tr( "Progress" ) } );
	m_computerTree->setRootIsDecorated( false );
	m_computerTree->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
	m_computerTree->header()->setSectionResizeMode( 1, QHeaderView::Stretch );
	computersLayout->addWidget( m_computerTree );
	mainLayout->addWidget( computersGroup, 1 );

	auto buttonBox = new QDialogButtonBox;
	m_sendButton = buttonBox->addButton( tr( "Send" ), QDialogButtonBox::AcceptRole );
	m_sendButton->setIcon( VeyonCore::icon( QStringLiteral("veyon-file-send") ) );
	m_cancelButton = buttonBox->addButton( tr( "Stop" ), QDialogButtonBox::ActionRole );
	m_cancelButton->setEnabled( false );
	buttonBox->addButton( QDialogButtonBox::Close );
	connect( m_sendButton, &QPushButton::clicked, this, &FileSenderDialog::startSending );
	connect( m_cancelButton, &QPushButton::clicked, this, &FileSenderDialog::cancelSending );
	connect( buttonBox, &QDialogButtonBox::rejected, this, &QDialog::close );
	mainLayout->addWidget( buttonBox );

	connect( &m_plugin, &FileSenderPlugin::replyReceived, this, &FileSenderDialog::handleReply );
	connect( &m_timeoutTimer, &QTimer::timeout, this, &FileSenderDialog::checkTimeouts );
	m_timeoutTimer.start( 5000 );

	setComputerControlInterfaces( computerControlInterfaces );
	updateSendingState();
}



FileSenderDialog::~FileSenderDialog()
{
	qDeleteAll( m_states );
}



void FileSenderDialog::setComputerControlInterfaces( const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( m_sending )
	{
		return;
	}

	m_computerControlInterfaces = computerControlInterfaces;

	qDeleteAll( m_states );
	m_states.clear();
	m_computerTree->clear();

	for( const auto& computer : computerControlInterfaces )
	{
		auto state = new ComputerState;
		state->computer = computer;
		state->item = new QTreeWidgetItem( m_computerTree, { AddOnUtils::computerDisplayName( computer ) } );
		state->progressBar = new QProgressBar;
		state->progressBar->setRange( 0, 1000 );
		state->progressBar->setValue( 0 );
		m_computerTree->setItemWidget( state->item, 2, state->progressBar );
		m_states.append( state );
		updateComputerRow( state, computer->state() == ComputerControlInterface::State::Connected ? tr( "Ready" ) : tr( "Offline" ) );
	}
}



void FileSenderDialog::closeEvent( QCloseEvent* event )
{
	if( m_sending &&
		QMessageBox::question( this, windowTitle(), tr( "Files are still being sent. Stop sending and close?" ) ) != QMessageBox::Yes )
	{
		event->ignore();
		return;
	}

	cancelSending();
	QDialog::closeEvent( event );
}



void FileSenderDialog::addFiles()
{
	addPaths( QFileDialog::getOpenFileNames( this, tr( "Select files to send" ),
											 QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) ) );
}



void FileSenderDialog::addFolder()
{
	const auto folder = QFileDialog::getExistingDirectory( this, tr( "Select folder to send" ),
														   QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) );
	if( folder.isEmpty() == false )
	{
		addPaths( { folder } );
	}
}



void FileSenderDialog::addPaths( const QStringList& paths )
{
	const QFileIconProvider iconProvider;

	for( const auto& path : paths )
	{
		const QFileInfo fileInfo( path );
		if( fileInfo.exists() == false || m_paths.contains( fileInfo.absoluteFilePath() ) )
		{
			continue;
		}

		m_paths.append( fileInfo.absoluteFilePath() );

		qint64 size = fileInfo.size();
		if( fileInfo.isDir() )
		{
			size = 0;
			QDirIterator it( path, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories );
			while( it.hasNext() )
			{
				it.next();
				size += it.fileInfo().size();
			}
		}

		auto item = new QTreeWidgetItem( m_fileTree, { fileInfo.fileName(), AddOnUtils::formatFileSize( size ) } );
		item->setIcon( 0, iconProvider.icon( fileInfo ) );
		item->setToolTip( 0, QDir::toNativeSeparators( fileInfo.absoluteFilePath() ) );
		item->setData( 0, Qt::UserRole, fileInfo.absoluteFilePath() );
		item->setTextAlignment( 1, Qt::AlignRight | Qt::AlignVCenter );
	}

	updateSummary();
}



void FileSenderDialog::removeSelectedFiles()
{
	const auto items = m_fileTree->selectedItems();
	for( auto item : items )
	{
		m_paths.removeAll( item->data( 0, Qt::UserRole ).toString() );
		delete item;
	}

	updateSummary();
}



void FileSenderDialog::updateSummary()
{
	const auto jobs = collectJobs();
	qint64 totalSize = 0;
	for( const auto& job : jobs )
	{
		totalSize += job.size;
	}

	m_summaryLabel->setText( jobs.isEmpty() ? tr( "Drag files or folders here or use the buttons." ) :
											  tr( "%n file(s), %1 in total", "", int(jobs.size()) ).arg( AddOnUtils::formatFileSize( totalSize ) ) );
	updateSendingState();
}



void FileSenderDialog::startSending()
{
	m_jobs = collectJobs();
	if( m_jobs.isEmpty() )
	{
		QMessageBox::information( this, windowTitle(), tr( "Please add the files you want to send." ) );
		return;
	}

	if( m_destinationComboBox->currentData().toInt() == int(FileSenderPlugin::Destination::Custom) &&
		m_customPathEdit->text().trimmed().isEmpty() )
	{
		QMessageBox::information( this, windowTitle(), tr( "Please enter the custom destination folder." ) );
		return;
	}

	m_totalSize = 0;
	for( const auto& job : std::as_const(m_jobs) )
	{
		m_totalSize += job.size;
	}

	int startedCount = 0;
	for( auto state : std::as_const(m_states) )
	{
		state->jobIndex = 0;
		state->bytesSent = 0;
		state->filesToOpen.clear();
		state->errors.clear();
		state->file.close();
		state->progressBar->setValue( 0 );

		if( state->computer->state() != ComputerControlInterface::State::Connected )
		{
			updateComputerRow( state, tr( "Offline - skipped" ) );
			continue;
		}

		state->active = true;
		++startedCount;
		sendNextFile( state );
	}

	if( startedCount == 0 )
	{
		QMessageBox::information( this, windowTitle(), tr( "None of the computers is connected." ) );
	}

	updateSendingState();
}



void FileSenderDialog::cancelSending()
{
	for( auto state : std::as_const(m_states) )
	{
		if( state->active )
		{
			if( state->file.isOpen() )
			{
				state->file.close();
				m_plugin.sendRequest( state->computer, Command::Cancel, { { Argument::TransferId, state->transferId } } );
			}
			state->active = false;
			state->pendingRequest = QUuid();
			updateComputerRow( state, tr( "Stopped" ) );
		}
	}

	updateSendingState();
}



void FileSenderDialog::request( ComputerState* state, Command command, QMap<Argument, QVariant> arguments )
{
	arguments[Argument::TransferId] = state->transferId;
	state->pendingCommand = command;
	state->pendingRequest = m_plugin.sendRequest( state->computer, command, arguments );
	state->lastActivity.restart();
}



void FileSenderDialog::sendNextFile( ComputerState* state )
{
	if( state->jobIndex >= m_jobs.size() )
	{
		finishComputer( state );
		return;
	}

	const auto& job = m_jobs.at( state->jobIndex );

	state->file.setFileName( job.localPath );
	if( state->file.open( QFile::ReadOnly ) == false )
	{
		state->errors.append( QStringLiteral("%1: %2").arg( job.relativePath, state->file.errorString() ) );
		state->bytesSent += job.size;
		++state->jobIndex;
		sendNextFile( state );
		return;
	}

	state->transferId = QUuid::createUuid();

	auto arguments = destinationArguments();
	arguments[Argument::RelativePath] = job.relativePath;
	arguments[Argument::Size] = job.size;
	arguments[Argument::ExistingFiles] = m_existingFilesComboBox->currentData().toInt();
	request( state, Command::Begin, arguments );

	updateComputerRow( state, tr( "Sending %1" ).arg( job.relativePath ) );
}



void FileSenderDialog::sendChunk( ComputerState* state )
{
	if( state->file.atEnd() )
	{
		request( state, Command::End, {} );
		return;
	}

	const auto data = state->file.read( FileSenderPlugin::ChunkSize );
	state->bytesSent += data.size();
	request( state, Command::Chunk, { { Argument::Data, data } } );
	updateComputerRow( state );
}



void FileSenderDialog::finishComputer( ComputerState* state )
{
	auto arguments = destinationArguments();
	// mid() returns QList<QString> with Qt 5, which QVariant doesn't accept
	arguments[Argument::Paths] = m_openFilesCheckBox->isChecked() ? QStringList( state->filesToOpen.mid( 0, MaximumFilesToOpen ) ) : QStringList{};
	arguments[Argument::OpenFiles] = m_openFilesCheckBox->isChecked();
	arguments[Argument::OpenFolder] = m_openFolderCheckBox->isChecked();
	arguments[Argument::ShowMessage] = m_showMessageCheckBox->isChecked();
	arguments[Argument::Message] = m_messageEdit->text().trimmed();
	request( state, Command::Complete, arguments );
}



void FileSenderDialog::handleReply( ComputerControlInterface::Pointer computerControlInterface, const QVariantMap& arguments )
{
	const auto requestId = FileSenderPlugin::argument( arguments, Argument::RequestId ).toUuid();

	ComputerState* state = nullptr;
	for( auto s : std::as_const(m_states) )
	{
		if( s->computer == computerControlInterface && s->active && s->pendingRequest == requestId )
		{
			state = s;
			break;
		}
	}

	if( state == nullptr )
	{
		return;
	}

	state->pendingRequest = QUuid();
	state->lastActivity.restart();

	const auto error = FileSenderPlugin::argument( arguments, Argument::Error ).toString();

	if( state->pendingCommand == Command::Complete )
	{
		state->active = false;
		if( error.isEmpty() == false )
		{
			state->errors.append( error );
		}
		updateComputerRow( state, state->errors.isEmpty() ? tr( "Done" )
														  : tr( "Done with %n error(s)", "", int(state->errors.size()) ) );
		if( state->errors.isEmpty() == false )
		{
			state->item->setToolTip( 1, state->errors.join( QLatin1Char('\n') ) );
		}
		updateSendingState();
		return;
	}

	const auto& job = m_jobs.at( state->jobIndex );

	if( error.isEmpty() == false )
	{
		state->errors.append( QStringLiteral("%1: %2").arg( job.relativePath, error ) );
		if( state->file.isOpen() )
		{
			state->bytesSent += job.size - state->file.pos();
			state->file.close();
			m_plugin.sendRequest( state->computer, Command::Cancel, { { Argument::TransferId, state->transferId } } );
		}
		++state->jobIndex;
		sendNextFile( state );
		return;
	}

	switch( state->pendingCommand )
	{
	case Command::Begin:
		if( FileSenderPlugin::argument( arguments, Argument::Skipped ).toBool() )
		{
			state->file.close();
			state->bytesSent += job.size;
			++state->jobIndex;
			sendNextFile( state );
		}
		else
		{
			sendChunk( state );
		}
		break;

	case Command::Chunk:
		sendChunk( state );
		break;

	case Command::End:
		state->file.close();
		// only open files which have been added individually (not each file of a folder)
		if( job.relativePath.contains( QLatin1Char('/') ) == false )
		{
			state->filesToOpen.append( FileSenderPlugin::argument( arguments, Argument::Path ).toString() );
		}
		++state->jobIndex;
		sendNextFile( state );
		break;

	default:
		break;
	}
}



void FileSenderDialog::checkTimeouts()
{
	for( auto state : std::as_const(m_states) )
	{
		if( state->active && state->pendingRequest.isNull() == false && state->lastActivity.elapsed() > ReplyTimeout )
		{
			state->file.close();
			state->active = false;
			state->pendingRequest = QUuid();
			updateComputerRow( state, tr( "No response from computer" ) );
		}
	}

	updateSendingState();
}



void FileSenderDialog::updateComputerRow( ComputerState* state, const QString& status )
{
	if( status.isEmpty() == false )
	{
		state->item->setText( 1, status );
	}

	if( m_totalSize > 0 )
	{
		state->progressBar->setValue( int( 1000 * qMin( state->bytesSent, m_totalSize ) / m_totalSize ) );
	}
	else if( state->active == false && state->jobIndex > 0 )
	{
		state->progressBar->setValue( 1000 );
	}
}



void FileSenderDialog::updateSendingState()
{
	m_sending = std::any_of( m_states.cbegin(), m_states.cend(), []( const ComputerState* state ) { return state->active; } );

	m_sendButton->setEnabled( m_sending == false && m_paths.isEmpty() == false );
	m_cancelButton->setEnabled( m_sending );
	m_fileTree->setEnabled( m_sending == false );
}



QMap<FileSenderPlugin::Argument, QVariant> FileSenderDialog::destinationArguments() const
{
	return {
		{ Argument::Destination, m_destinationComboBox->currentData().toInt() },
		{ Argument::CustomPath, QDir::fromNativeSeparators( m_customPathEdit->text().trimmed() ) },
		{ Argument::Subfolder, m_subfolderEdit->text().trimmed() },
	};
}



QList<FileSenderDialog::Job> FileSenderDialog::collectJobs() const
{
	QList<Job> jobs;

	for( const auto& path : m_paths )
	{
		const QFileInfo fileInfo( path );
		if( fileInfo.isDir() )
		{
			QDirIterator it( path, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories );
			while( it.hasNext() )
			{
				const auto filePath = it.next();
				jobs.append( { filePath, fileInfo.fileName() + QLatin1Char('/') + QDir( path ).relativeFilePath( filePath ),
							   it.fileInfo().size() } );
			}
		}
		else if( fileInfo.isFile() )
		{
			jobs.append( { path, fileInfo.fileName(), fileInfo.size() } );
		}
	}

	return jobs;
}
