/*
 * TaskManagerDialog.cpp - implementation of TaskManagerDialog class
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

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSplitter>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "AddOnUtils.h"
#include "Configuration/Object.h"
#include "TaskManagerDialog.h"
#include "TaskManagerPlugin.h"
#include "VeyonCore.h"
#include "VeyonMasterInterface.h"


namespace {

// sorts numeric columns by their numeric value instead of their text
class ProcessTreeItem : public QTreeWidgetItem
{
public:
	using QTreeWidgetItem::QTreeWidgetItem;

	bool operator<( const QTreeWidgetItem& other ) const override
	{
		const auto column = treeWidget() ? treeWidget()->sortColumn() : 0;
		const auto a = data( column, Qt::UserRole );
		const auto b = other.data( column, Qt::UserRole );
		if( a.isValid() && b.isValid() )
		{
			return a.toDouble() < b.toDouble();
		}
		return text( column ).compare( other.text( column ), Qt::CaseInsensitive ) < 0;
	}
};

struct BuiltinPreset
{
	const char* name;
	ApplicationRules::Mode mode;
	const char* patterns;
};

static const BuiltinPreset builtinPresets[] = { // clazy:exclude=non-pod-global-static
	{ QT_TRANSLATE_NOOP("TaskManagerDialog", "Block web browsers"), ApplicationRules::Mode::Blacklist,
	  "chrome\nmsedge\nfirefox\nopera\nbrave\nvivaldi\nSafari\nGoogle Chrome\nMicrosoft Edge\nchromium" },
	{ QT_TRANSLATE_NOOP("TaskManagerDialog", "Block games and chat"), ApplicationRules::Mode::Blacklist,
	  "*minecraft*\nsteam*\nepicgameslauncher\nroblox*\ndiscord*\nspotify\nwhatsapp*\ntelegram*\nbattle.net*" },
	{ QT_TRANSLATE_NOOP("TaskManagerDialog", "Office only"), ApplicationRules::Mode::Whitelist,
	  "winword\nexcel\npowerpnt\nonenote\nMicrosoft Word\nMicrosoft Excel\nMicrosoft PowerPoint\nPages\nNumbers\nKeynote\nnotepad\nTextEdit\nsoffice*\nlibreoffice*" },
};

constexpr auto PresetsKey = "Presets";
constexpr auto ConfigParentKey = "TaskManager";

}



TaskManagerDialog::TaskManagerDialog( TaskManagerPlugin& plugin, VeyonMasterInterface& master,
									  const ComputerControlInterfaceList& computerControlInterfaces, QWidget* parent ) :
	QDialog( parent ),
	m_plugin( plugin ),
	m_master( master )
{
	setWindowTitle( tr( "Remote Task Manager" ) );
	setWindowIcon( VeyonCore::icon( QStringLiteral("veyon-task-manager") ) );
	resize( 1100, 700 );

	auto mainLayout = new QVBoxLayout( this );

	auto splitter = new QSplitter( Qt::Horizontal );

	auto computerPanel = new QWidget;
	auto computerLayout = new QVBoxLayout( computerPanel );
	computerLayout->setContentsMargins( 0, 0, 0, 0 );
	computerLayout->addWidget( new QLabel( tr( "Computers" ) ) );
	m_computerList = new QListWidget;
	m_computerList->setSelectionMode( QAbstractItemView::ExtendedSelection );
	m_computerList->setToolTip( tr( "Select one computer to view its programs. "
									"Select multiple computers to apply app rules to all of them." ) );
	computerLayout->addWidget( m_computerList );
	splitter->addWidget( computerPanel );

	m_tabWidget = new QTabWidget;
	m_tabWidget->addTab( createProcessesPage(), VeyonCore::icon( QStringLiteral("veyon-task-manager") ), tr( "Programs" ) );
	m_tabWidget->addTab( createRulesPage(), VeyonCore::icon( QStringLiteral("veyon-application-control") ), tr( "App rules" ) );
	splitter->addWidget( m_tabWidget );
	splitter->setStretchFactor( 0, 1 );
	splitter->setStretchFactor( 1, 4 );
	mainLayout->addWidget( splitter, 1 );

	auto buttonBox = new QDialogButtonBox( QDialogButtonBox::Close );
	connect( buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject );
	mainLayout->addWidget( buttonBox );

	connect( m_computerList, &QListWidget::currentRowChanged, this, [this]() {
		m_processes.clear();
		updateProcessTree();
		refresh();
		if( const auto computer = currentComputer() )
		{
			setRulesInUi( m_rulesStates.value( AddOnUtils::computerKey( computer ) ) );
		}
	} );

	connect( &m_plugin, &TaskManagerPlugin::processListReceived, this, &TaskManagerDialog::handleProcessList );
	connect( &m_plugin, &TaskManagerPlugin::rulesStateReceived, this, &TaskManagerDialog::handleRulesState );
	connect( &m_plugin, &TaskManagerPlugin::terminationResultReceived, this, &TaskManagerDialog::handleTerminationResult );

	connect( &m_refreshTimer, &QTimer::timeout, this, &TaskManagerDialog::refresh );
	m_refreshTimer.start( AutoRefreshInterval );

	loadPresets();
	setComputerControlInterfaces( computerControlInterfaces );
}



void TaskManagerDialog::setComputerControlInterfaces( const ComputerControlInterfaceList& computerControlInterfaces )
{
	m_computerControlInterfaces = computerControlInterfaces;
	updateComputerList();

	if( m_computerList->count() > 0 )
	{
		m_computerList->setCurrentRow( 0 );
	}

	m_plugin.queryRules( AddOnUtils::connectedInterfaces( m_computerControlInterfaces ) );
}



QWidget* TaskManagerDialog::createProcessesPage()
{
	auto page = new QWidget;
	auto layout = new QVBoxLayout( page );

	auto toolLayout = new QHBoxLayout;
	m_filterEdit = new QLineEdit;
	m_filterEdit->setPlaceholderText( tr( "Filter programs" ) );
	m_filterEdit->setClearButtonEnabled( true );
	connect( m_filterEdit, &QLineEdit::textChanged, this, &TaskManagerDialog::updateProcessTree );
	toolLayout->addWidget( m_filterEdit, 1 );

	m_showAllProcessesCheckBox = new QCheckBox( tr( "Show background processes" ) );
	connect( m_showAllProcessesCheckBox, &QCheckBox::toggled, this, &TaskManagerDialog::updateProcessTree );
	toolLayout->addWidget( m_showAllProcessesCheckBox );

	m_autoRefreshCheckBox = new QCheckBox( tr( "Refresh automatically" ) );
	m_autoRefreshCheckBox->setChecked( true );
	toolLayout->addWidget( m_autoRefreshCheckBox );

	auto refreshButton = new QPushButton( tr( "Refresh" ) );
	connect( refreshButton, &QPushButton::clicked, this, &TaskManagerDialog::refresh );
	toolLayout->addWidget( refreshButton );
	layout->addLayout( toolLayout );

	m_processTree = new QTreeWidget;
	m_processTree->setColumnCount( ColumnCount );
	m_processTree->setHeaderLabels( { tr( "Program" ), tr( "PID" ), tr( "User" ), tr( "CPU" ), tr( "Memory" ), tr( "Window" ) } );
	m_processTree->setRootIsDecorated( false );
	m_processTree->setUniformRowHeights( true );
	m_processTree->setSortingEnabled( true );
	m_processTree->sortByColumn( ColumnCpu, Qt::DescendingOrder );
	m_processTree->setSelectionMode( QAbstractItemView::ExtendedSelection );
	m_processTree->setContextMenuPolicy( Qt::CustomContextMenu );
	m_processTree->header()->setSectionResizeMode( ColumnName, QHeaderView::Stretch );
	m_processTree->header()->setSectionResizeMode( ColumnTitle, QHeaderView::Stretch );
	connect( m_processTree, &QTreeWidget::customContextMenuRequested, this, [this]( const QPoint& pos ) {
		QMenu menu;
		menu.addAction( tr( "End program" ), this, [this]() { endSelectedProcesses( false ); } );
		menu.addAction( tr( "Kill program immediately" ), this, [this]() { endSelectedProcesses( true ); } );
		menu.addAction( tr( "End on all computers" ), this, &TaskManagerDialog::endProcessesOnAllComputers );
		menu.addSeparator();
		menu.addAction( tr( "Block this program (add to blacklist)" ), this,
						[this]() { addSelectedProcessesToRules( ApplicationRules::Mode::Blacklist ); } );
		menu.addAction( tr( "Allow this program (add to whitelist)" ), this,
						[this]() { addSelectedProcessesToRules( ApplicationRules::Mode::Whitelist ); } );
		menu.exec( m_processTree->viewport()->mapToGlobal( pos ) );
	} );
	layout->addWidget( m_processTree, 1 );

	auto actionLayout = new QHBoxLayout;
	m_processStatusLabel = new QLabel;
	actionLayout->addWidget( m_processStatusLabel, 1 );

	auto blockButton = new QPushButton( VeyonCore::icon( QStringLiteral("veyon-application-control") ), tr( "Block program…" ) );
	auto blockMenu = new QMenu( blockButton );
	blockMenu->addAction( tr( "Add to blacklist" ), this, [this]() { addSelectedProcessesToRules( ApplicationRules::Mode::Blacklist ); } );
	blockMenu->addAction( tr( "Add to whitelist" ), this, [this]() { addSelectedProcessesToRules( ApplicationRules::Mode::Whitelist ); } );
	blockButton->setMenu( blockMenu );
	actionLayout->addWidget( blockButton );

	auto endAllButton = new QPushButton( tr( "End on all computers" ) );
	endAllButton->setToolTip( tr( "End the selected programs on all computers of this window" ) );
	connect( endAllButton, &QPushButton::clicked, this, &TaskManagerDialog::endProcessesOnAllComputers );
	actionLayout->addWidget( endAllButton );

	auto endButton = new QPushButton( tr( "End program" ) );
	endButton->setToolTip( tr( "Ask the selected programs to quit and end them forcefully if they don't quit within 5 seconds" ) );
	connect( endButton, &QPushButton::clicked, this, [this]() { endSelectedProcesses( false ); } );
	actionLayout->addWidget( endButton );

	layout->addLayout( actionLayout );

	return page;
}



QWidget* TaskManagerDialog::createRulesPage()
{
	auto page = new QWidget;
	auto layout = new QVBoxLayout( page );

	auto presetLayout = new QHBoxLayout;
	presetLayout->addWidget( new QLabel( tr( "Preset" ) ) );
	m_presetComboBox = new QComboBox;
	connect( m_presetComboBox, QOverload<int>::of( &QComboBox::activated ), this, &TaskManagerDialog::loadPreset );
	presetLayout->addWidget( m_presetComboBox, 1 );
	auto savePresetButton = new QPushButton( tr( "Save as preset…" ) );
	connect( savePresetButton, &QPushButton::clicked, this, &TaskManagerDialog::savePreset );
	presetLayout->addWidget( savePresetButton );
	auto deletePresetButton = new QPushButton( tr( "Delete preset" ) );
	connect( deletePresetButton, &QPushButton::clicked, this, &TaskManagerDialog::deletePreset );
	presetLayout->addWidget( deletePresetButton );
	layout->addLayout( presetLayout );

	auto modeGroup = new QGroupBox( tr( "Mode" ) );
	auto modeLayout = new QVBoxLayout( modeGroup );
	m_modeDisabledButton = new QRadioButton( tr( "No restrictions" ) );
	m_modeBlacklistButton = new QRadioButton( tr( "Blacklist: block the programs listed below" ) );
	m_modeWhitelistButton = new QRadioButton( tr( "Whitelist: allow only the programs listed below (system programs always stay allowed)" ) );
	auto buttonGroup = new QButtonGroup( modeGroup );
	for( auto button : { m_modeDisabledButton, m_modeBlacklistButton, m_modeWhitelistButton } )
	{
		buttonGroup->addButton( button );
		modeLayout->addWidget( button );
	}
	m_modeDisabledButton->setChecked( true );
	layout->addWidget( modeGroup );

	auto programsGroup = new QGroupBox( tr( "Programs" ) );
	auto programsLayout = new QVBoxLayout( programsGroup );
	auto hintLabel = new QLabel( tr( "One program per line. Enter program names (e.g. <tt>chrome</tt>, <tt>Minecraft</tt>, "
									 "<tt>Google Chrome</tt>) or file paths. Use <tt>*</tt> as wildcard, e.g. <tt>*game*</tt>." ) );
	hintLabel->setWordWrap( true );
	programsLayout->addWidget( hintLabel );
	m_patternsEdit = new QPlainTextEdit;
	programsLayout->addWidget( m_patternsEdit, 1 );
	layout->addWidget( programsGroup, 1 );

	auto optionsLayout = new QFormLayout;
	m_messageEdit = new QLineEdit;
	m_messageEdit->setPlaceholderText( tr( "Default: \"The application … is not allowed at the moment…\"" ) );
	optionsLayout->addRow( tr( "Message for students" ), m_messageEdit );
	m_persistentCheckBox = new QCheckBox( tr( "Keep rules active after the student computer has been restarted" ) );
	optionsLayout->addRow( QString(), m_persistentCheckBox );
	layout->addLayout( optionsLayout );

	auto actionLayout = new QHBoxLayout;
	m_rulesStatusLabel = new QLabel;
	m_rulesStatusLabel->setWordWrap( true );
	actionLayout->addWidget( m_rulesStatusLabel, 1 );
	auto removeButton = new QPushButton( tr( "Remove rules" ) );
	removeButton->setToolTip( tr( "Remove all app rules from the selected computers" ) );
	connect( removeButton, &QPushButton::clicked, this, &TaskManagerDialog::removeRulesFromComputers );
	actionLayout->addWidget( removeButton );
	auto applyButton = new QPushButton( tr( "Apply to selected computers" ) );
	applyButton->setDefault( true );
	connect( applyButton, &QPushButton::clicked, this, &TaskManagerDialog::applyRulesToComputers );
	actionLayout->addWidget( applyButton );
	layout->addLayout( actionLayout );

	auto blockedGroup = new QGroupBox( tr( "Recently blocked on this computer" ) );
	auto blockedLayout = new QVBoxLayout( blockedGroup );
	m_blockedTree = new QTreeWidget;
	m_blockedTree->setHeaderLabels( { tr( "Time" ), tr( "Program" ), tr( "Path" ) } );
	m_blockedTree->setRootIsDecorated( false );
	m_blockedTree->setMaximumHeight( 150 );
	blockedLayout->addWidget( m_blockedTree );
	layout->addWidget( blockedGroup );

	return page;
}



void TaskManagerDialog::updateComputerList()
{
	const auto currentKey = AddOnUtils::computerKey( currentComputer() );

	m_computerList->clear();
	for( const auto& computer : std::as_const(m_computerControlInterfaces) )
	{
		auto text = AddOnUtils::computerDisplayName( computer );
		const auto rules = m_rulesStates.value( AddOnUtils::computerKey( computer ) );
		if( rules.isActive() )
		{
			text += rules.mode == ApplicationRules::Mode::Blacklist ? tr( " – blacklist active" ) : tr( " – whitelist active" );
		}

		auto item = new QListWidgetItem( text );
		item->setData( Qt::UserRole, AddOnUtils::computerKey( computer ) );
		if( computer->state() != ComputerControlInterface::State::Connected )
		{
			item->setForeground( palette().color( QPalette::Disabled, QPalette::Text ) );
			item->setToolTip( tr( "This computer is not connected." ) );
		}
		if( rules.isActive() )
		{
			item->setIcon( VeyonCore::icon( QStringLiteral("veyon-application-control") ) );
		}
		m_computerList->addItem( item );

		if( item->data( Qt::UserRole ).toString() == currentKey )
		{
			m_computerList->blockSignals( true );
			m_computerList->setCurrentItem( item );
			m_computerList->blockSignals( false );
		}
	}
}



ComputerControlInterface::Pointer TaskManagerDialog::currentComputer() const
{
	const auto item = m_computerList->currentItem();
	if( item == nullptr )
	{
		return {};
	}

	return AddOnUtils::findComputer( m_computerControlInterfaces, item->data( Qt::UserRole ).toString() );
}



ComputerControlInterfaceList TaskManagerDialog::selectedComputers() const
{
	ComputerControlInterfaceList computers;
	const auto items = m_computerList->selectedItems();
	for( const auto* item : items )
	{
		const auto computer = AddOnUtils::findComputer( m_computerControlInterfaces, item->data( Qt::UserRole ).toString() );
		if( computer )
		{
			computers.append( computer );
		}
	}

	if( computers.isEmpty() && currentComputer() )
	{
		computers.append( currentComputer() );
	}

	return computers;
}



void TaskManagerDialog::refresh()
{
	if( sender() == &m_refreshTimer && ( m_autoRefreshCheckBox->isChecked() == false || isVisible() == false ) )
	{
		return;
	}

	const auto computer = currentComputer();
	if( computer.isNull() )
	{
		m_processStatusLabel->setText( tr( "No computer selected" ) );
		return;
	}

	if( computer->state() != ComputerControlInterface::State::Connected )
	{
		m_processStatusLabel->setText( tr( "%1 is not connected" ).arg( computer->computerName() ) );
		return;
	}

	m_plugin.queryProcesses( computer );
}



void TaskManagerDialog::updateProcessTree()
{
	QList<qint64> selectedPids;
	const auto selectedItems = m_processTree->selectedItems();
	for( const auto* item : selectedItems )
	{
		selectedPids.append( item->data( ColumnPid, Qt::UserRole ).toLongLong() );
	}

	const auto filter = m_filterEdit->text().trimmed();
	const auto showAll = m_showAllProcessesCheckBox->isChecked();

	m_processTree->setUpdatesEnabled( false );
	m_processTree->setSortingEnabled( false );
	m_processTree->clear();

	int shownCount = 0;
	double totalCpu = 0;
	for( const auto& process : std::as_const(m_processes) )
	{
		totalCpu += process.cpu;

		if( showAll == false && process.isApplication == false )
		{
			continue;
		}

		if( filter.isEmpty() == false &&
			process.displayName().contains( filter, Qt::CaseInsensitive ) == false &&
			process.name.contains( filter, Qt::CaseInsensitive ) == false &&
			process.windowTitle.contains( filter, Qt::CaseInsensitive ) == false )
		{
			continue;
		}

		auto item = new ProcessTreeItem;
		item->setText( ColumnName, process.displayName() );
		item->setToolTip( ColumnName, process.path.isEmpty() ? process.name : process.path );
		item->setText( ColumnPid, QString::number( process.pid ) );
		item->setData( ColumnPid, Qt::UserRole, process.pid );
		item->setText( ColumnUser, process.user );
		item->setText( ColumnCpu, QStringLiteral("%1 %").arg( process.cpu, 0, 'f', 1 ) );
		item->setData( ColumnCpu, Qt::UserRole, process.cpu );
		item->setText( ColumnMemory, AddOnUtils::formatFileSize( process.memory ) );
		item->setData( ColumnMemory, Qt::UserRole, process.memory );
		item->setText( ColumnTitle, process.windowTitle );
		item->setTextAlignment( ColumnPid, Qt::AlignRight | Qt::AlignVCenter );
		item->setTextAlignment( ColumnCpu, Qt::AlignRight | Qt::AlignVCenter );
		item->setTextAlignment( ColumnMemory, Qt::AlignRight | Qt::AlignVCenter );
		m_processTree->addTopLevelItem( item );

		if( selectedPids.contains( process.pid ) )
		{
			item->setSelected( true );
		}

		++shownCount;
	}

	m_processTree->setSortingEnabled( true );
	m_processTree->setUpdatesEnabled( true );

	if( const auto computer = currentComputer() )
	{
		m_processStatusLabel->setText( tr( "%1: %2 programs shown, %3 processes in total, CPU usage %4 %" )
									   .arg( computer->computerName() ).arg( shownCount ).arg( m_processes.size() )
									   .arg( qMin( 100.0, totalCpu ), 0, 'f', 0 ) );
	}
}



void TaskManagerDialog::endSelectedProcesses( bool force )
{
	const auto computer = currentComputer();
	const auto processes = selectedProcesses();
	if( computer.isNull() || processes.isEmpty() )
	{
		return;
	}

	QStringList names;
	QList<qint64> pids;
	for( const auto& process : processes )
	{
		names.append( process.displayName() );
		pids.append( process.pid );
	}

	if( force &&
		QMessageBox::question( this, windowTitle(),
							   tr( "Do you really want to end the following programs immediately? Unsaved work will be lost.\n\n%1" )
							   .arg( names.join( QLatin1Char('\n') ) ) ) != QMessageBox::Yes )
	{
		return;
	}

	m_plugin.terminateProcesses( computer, pids, force );
	QTimer::singleShot( 1000, this, &TaskManagerDialog::refresh );
}



void TaskManagerDialog::endProcessesOnAllComputers()
{
	const auto processes = selectedProcesses();

	QStringList names;
	for( const auto& process : processes )
	{
		if( names.contains( process.name ) == false )
		{
			names.append( process.name );
		}
	}

	bool ok = false;
	const auto text = QInputDialog::getMultiLineText( this, windowTitle(),
													  tr( "The following programs will be ended on all %1 computers of this window "
														  "(one per line, wildcards allowed):" ).arg( m_computerControlInterfaces.size() ),
													  names.join( QLatin1Char('\n') ), &ok );
	const auto patterns = AddOnUtils::splitList( text );
	if( ok == false || patterns.isEmpty() )
	{
		return;
	}

	m_plugin.terminateByName( AddOnUtils::connectedInterfaces( m_computerControlInterfaces ), patterns );
	QTimer::singleShot( 1000, this, &TaskManagerDialog::refresh );
}



void TaskManagerDialog::addSelectedProcessesToRules( ApplicationRules::Mode mode )
{
	const auto processes = selectedProcesses();
	if( processes.isEmpty() )
	{
		return;
	}

	auto patterns = AddOnUtils::splitList( m_patternsEdit->toPlainText() );
	for( const auto& process : processes )
	{
		auto name = process.displayName();
		if( name.endsWith( QLatin1String(".exe"), Qt::CaseInsensitive ) )
		{
			name.chop( 4 );
		}
		if( patterns.contains( name, Qt::CaseInsensitive ) == false )
		{
			patterns.append( name );
		}
	}

	m_patternsEdit->setPlainText( patterns.join( QLatin1Char('\n') ) );
	( mode == ApplicationRules::Mode::Blacklist ? m_modeBlacklistButton : m_modeWhitelistButton )->setChecked( true );
	m_tabWidget->setCurrentIndex( 1 );
}



QList<ProcessInfo> TaskManagerDialog::selectedProcesses() const
{
	QList<ProcessInfo> processes;
	const auto items = m_processTree->selectedItems();
	for( const auto* item : items )
	{
		const auto pid = item->data( ColumnPid, Qt::UserRole ).toLongLong();
		const auto it = std::find_if( m_processes.cbegin(), m_processes.cend(),
									  [pid]( const ProcessInfo& p ) { return p.pid == pid; } );
		if( it != m_processes.cend() )
		{
			processes.append( *it );
		}
	}
	return processes;
}



ApplicationRules TaskManagerDialog::rulesFromUi() const
{
	ApplicationRules rules;
	rules.mode = m_modeBlacklistButton->isChecked() ? ApplicationRules::Mode::Blacklist :
				 m_modeWhitelistButton->isChecked() ? ApplicationRules::Mode::Whitelist :
													  ApplicationRules::Mode::Disabled;
	rules.patterns = AddOnUtils::splitList( m_patternsEdit->toPlainText() );
	rules.message = m_messageEdit->text().trimmed();
	rules.persistent = m_persistentCheckBox->isChecked();
	return rules;
}



void TaskManagerDialog::setRulesInUi( const ApplicationRules& rules )
{
	switch( rules.mode )
	{
	case ApplicationRules::Mode::Blacklist: m_modeBlacklistButton->setChecked( true ); break;
	case ApplicationRules::Mode::Whitelist: m_modeWhitelistButton->setChecked( true ); break;
	case ApplicationRules::Mode::Disabled: m_modeDisabledButton->setChecked( true ); break;
	}

	if( rules.mode != ApplicationRules::Mode::Disabled || rules.patterns.isEmpty() == false )
	{
		m_patternsEdit->setPlainText( rules.patterns.join( QLatin1Char('\n') ) );
		m_messageEdit->setText( rules.message );
		m_persistentCheckBox->setChecked( rules.persistent );
	}

	m_blockedTree->clear();
	if( const auto computer = currentComputer() )
	{
		const auto blockedApplications = m_blockedApplications.value( AddOnUtils::computerKey( computer ) );
		for( const auto& entry : blockedApplications )
		{
			const auto map = entry.toMap();
			auto item = new QTreeWidgetItem( { QLocale().toString( map.value( QStringLiteral("time") ).toDateTime().time(), QLocale::ShortFormat ),
											   map.value( QStringLiteral("name") ).toString(),
											   map.value( QStringLiteral("path") ).toString() } );
			m_blockedTree->addTopLevelItem( item );
		}

		m_rulesStatusLabel->setText( rules.isActive() ?
										 tr( "Rules active on %1 (%2 programs listed)" ).arg( computer->computerName() ).arg( rules.patterns.size() ) :
										 tr( "No rules active on %1" ).arg( computer->computerName() ) );
	}
}



void TaskManagerDialog::applyRulesToComputers()
{
	const auto rules = rulesFromUi();
	const auto computers = AddOnUtils::connectedInterfaces( selectedComputers() );

	if( computers.isEmpty() )
	{
		QMessageBox::information( this, windowTitle(), tr( "Please select at least one connected computer." ) );
		return;
	}

	if( rules.mode == ApplicationRules::Mode::Disabled )
	{
		removeRulesFromComputers();
		return;
	}

	if( rules.mode == ApplicationRules::Mode::Whitelist &&
		QMessageBox::question( this, windowTitle(),
							   tr( "In whitelist mode all programs which are not listed are closed immediately on %n computer(s). "
								   "Unsaved work in these programs will be lost. Continue?", "", int(computers.size()) ) ) != QMessageBox::Yes )
	{
		return;
	}

	if( rules.mode == ApplicationRules::Mode::Blacklist && rules.patterns.isEmpty() )
	{
		QMessageBox::information( this, windowTitle(), tr( "Please enter at least one program to block." ) );
		return;
	}

	m_plugin.setRules( computers, rules );
}



void TaskManagerDialog::removeRulesFromComputers()
{
	m_plugin.clearRules( AddOnUtils::connectedInterfaces( selectedComputers() ) );
}



void TaskManagerDialog::loadPresets()
{
	m_presetComboBox->clear();
	m_presetComboBox->addItem( tr( "(choose a preset)" ) );

	for( const auto& preset : builtinPresets )
	{
		m_presetComboBox->addItem( tr( preset.name ), QVariantMap{
									   { QStringLiteral("mode"), int(preset.mode) },
									   { QStringLiteral("patterns"), QString::fromUtf8( preset.patterns ).split( QLatin1Char('\n') ) },
									   { QStringLiteral("builtin"), true } } );
	}

	const auto presets = userPresets();
	for( const auto& preset : presets )
	{
		const auto object = preset.toObject();
		m_presetComboBox->addItem( object[QStringLiteral("name")].toString(), object.toVariantMap() );
	}
}



void TaskManagerDialog::savePreset()
{
	bool ok = false;
	const auto name = QInputDialog::getText( this, tr( "Save preset" ), tr( "Name of the preset" ),
											 QLineEdit::Normal, {}, &ok ).trimmed();
	if( ok == false || name.isEmpty() )
	{
		return;
	}

	auto preset = rulesFromUi().toJson();
	preset[QStringLiteral("name")] = name;

	auto presets = userPresets();
	for( int i = 0; i < presets.size(); ++i )
	{
		if( presets[i].toObject()[QStringLiteral("name")].toString() == name )
		{
			presets.removeAt( i );
			break;
		}
	}
	presets.append( preset );
	setUserPresets( presets );

	loadPresets();
	m_presetComboBox->setCurrentIndex( m_presetComboBox->findText( name ) );
}



void TaskManagerDialog::deletePreset()
{
	const auto data = m_presetComboBox->currentData().toMap();
	if( data.isEmpty() || data.value( QStringLiteral("builtin") ).toBool() )
	{
		QMessageBox::information( this, windowTitle(), tr( "Built-in presets can't be deleted." ) );
		return;
	}

	const auto name = m_presetComboBox->currentText();
	auto presets = userPresets();
	for( int i = 0; i < presets.size(); ++i )
	{
		if( presets[i].toObject()[QStringLiteral("name")].toString() == name )
		{
			presets.removeAt( i );
			break;
		}
	}
	setUserPresets( presets );
	loadPresets();
}



void TaskManagerDialog::loadPreset( int index )
{
	const auto data = m_presetComboBox->itemData( index ).toMap();
	if( data.isEmpty() == false )
	{
		auto rules = ApplicationRules::fromVariantMap( data );
		if( data.value( QStringLiteral("builtin") ).toBool() )
		{
			rules.message = m_messageEdit->text();
			rules.persistent = m_persistentCheckBox->isChecked();
		}
		switch( rules.mode )
		{
		case ApplicationRules::Mode::Blacklist: m_modeBlacklistButton->setChecked( true ); break;
		case ApplicationRules::Mode::Whitelist: m_modeWhitelistButton->setChecked( true ); break;
		case ApplicationRules::Mode::Disabled: m_modeDisabledButton->setChecked( true ); break;
		}
		m_patternsEdit->setPlainText( rules.patterns.join( QLatin1Char('\n') ) );
		m_messageEdit->setText( rules.message );
		m_persistentCheckBox->setChecked( rules.persistent );
	}
}



QJsonArray TaskManagerDialog::userPresets() const
{
	return m_master.userConfigurationObject()->value( QLatin1String(PresetsKey), QLatin1String(ConfigParentKey), QJsonArray{} ).toJsonArray();
}



void TaskManagerDialog::setUserPresets( const QJsonArray& presets )
{
	m_master.userConfigurationObject()->setValue( QLatin1String(PresetsKey), presets, QLatin1String(ConfigParentKey) );
}



void TaskManagerDialog::handleProcessList( ComputerControlInterface::Pointer computerControlInterface,
										   const QString& platform, const ProcessInfoList& processes )
{
	if( computerControlInterface != currentComputer() )
	{
		return;
	}

	m_platform = platform;
	m_processes = processes;
	m_processTree->setColumnHidden( ColumnTitle, platform == QLatin1String("macos") );
	updateProcessTree();
}



void TaskManagerDialog::handleRulesState( ComputerControlInterface::Pointer computerControlInterface,
										  const ApplicationRules& rules, const QVariantList& blockedApplications )
{
	const auto key = AddOnUtils::computerKey( computerControlInterface );
	if( AddOnUtils::findComputer( m_computerControlInterfaces, key ).isNull() )
	{
		return;
	}

	const auto changed = m_rulesStates.value( key ) != rules;
	m_rulesStates[key] = rules;
	m_blockedApplications[key] = blockedApplications;

	if( changed )
	{
		updateComputerList();
	}

	if( computerControlInterface == currentComputer() && ( changed || m_tabWidget->currentIndex() == 1 ) )
	{
		setRulesInUi( rules );
	}
}



void TaskManagerDialog::handleTerminationResult( ComputerControlInterface::Pointer computerControlInterface,
												 int terminatedCount, const QStringList& failedNames )
{
	if( AddOnUtils::findComputer( m_computerControlInterfaces, AddOnUtils::computerKey( computerControlInterface ) ).isNull() )
	{
		return;
	}

	if( failedNames.isEmpty() == false )
	{
		QMessageBox::warning( this, windowTitle(),
							  tr( "The following programs could not be ended on %1 (system programs are protected):\n\n%2" )
							  .arg( computerControlInterface->computerName(), failedNames.join( QLatin1Char('\n') ) ) );
	}
	else if( computerControlInterface == currentComputer() )
	{
		m_processStatusLabel->setText( tr( "%n program(s) ended on %1", "", terminatedCount ).arg( computerControlInterface->computerName() ) );
	}
}
