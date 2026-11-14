/*
 * MainWindow.cpp - implementation of MainWindow class
 *
 * Copyright (c) 2004-2026 Tobias Junghans <tobydox@veyon.io>
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

#include <algorithm>

#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QHostAddress>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QSplitter>

#include <QDir>
#include <QMenuBar>
#include <QProcess>

#include "AboutDialog.h"
#include "AccessControlProvider.h"
#include "AddOnStoreDialog.h"
#include "Filesystem.h"
#include "PlatformCoreFunctions.h"
#include "MainWindow.h"
#include "BuiltinFeatures.h"
#include "AuthenticationCredentials.h"
#include "ComputerControlListModel.h"
#include "ComputerManager.h"
#include "ComputerSelectPanel.h"
#include "ScreenshotManagementPanel.h"
#include "FeatureManager.h"
#include "MonitoringMode.h"
#include "NetworkObjectDirectory.h"
#include "NetworkObjectDirectoryManager.h"
#include "PlatformUserFunctions.h"
#include "SlideshowPanel.h"
#include "SpotlightPanel.h"
#include "StackedToolButtons.h"
#include "ToolButton.h"
#include "VeyonConfiguration.h"
#include "VeyonMaster.h"
#include "UserConfig.h"

#include "ui_MainWindow.h"


MainWindow::MainWindow( VeyonMaster &masterCore, QWidget* parent ) :
	QMainWindow( parent ),
	ui( new Ui::MainWindow ),
	m_master( masterCore ),
	m_modeGroup( new QButtonGroup( this ) )
{
	ui->setupUi( this );

	restoreState( QByteArray::fromBase64( m_master.userConfig().windowState().toUtf8() ) );
	restoreGeometry( QByteArray::fromBase64( m_master.userConfig().windowGeometry().toUtf8() ) );

	// add widgets to status bar
	ui->statusBar->addWidget( ui->panelButtons );
	ui->statusBar->addWidget( ui->spacerLabel1 );
	ui->statusBar->addWidget( ui->filterLineEdit, 2 );
	ui->statusBar->addWidget( ui->filterPoweredOnComputersButton );
	ui->statusBar->addWidget( ui->filterComputersWithLoggedOnUsersButton );
	ui->statusBar->addWidget( ui->spacerLabel2, 1 );
	ui->statusBar->addWidget( ui->gridSizeSlider, 2 );
	ui->statusBar->addWidget( ui->autoAdjustComputerIconSizeButton );
	ui->statusBar->addWidget( ui->spacerLabel3 );
	ui->statusBar->addWidget( ui->useCustomComputerPositionsButton );
	ui->statusBar->addWidget( ui->alignComputersButton );
	ui->statusBar->addWidget( ui->spacerLabel4 );
	ui->statusBar->addWidget( ui->aboutButton );

	// create all views
	auto mainSplitter = new QSplitter( Qt::Horizontal, ui->centralWidget );
	mainSplitter->setChildrenCollapsible( false );
	mainSplitter->setObjectName( QStringLiteral("MainSplitter") );

	auto monitoringSplitter = new QSplitter( Qt::Vertical, mainSplitter );
	monitoringSplitter->setChildrenCollapsible( false );
	monitoringSplitter->setObjectName( QStringLiteral("MonitoringSplitter") );

	auto slideshowSpotlightSplitter = new QSplitter( Qt::Horizontal, monitoringSplitter );
	slideshowSpotlightSplitter->setChildrenCollapsible( false );
	slideshowSpotlightSplitter->setObjectName( QStringLiteral("SlideshowSpotlightSplitter") );

	auto computerSelectPanel = new ComputerSelectPanel( m_master.computerManager() );
	auto screenshotManagementPanel = new ScreenshotManagementPanel();
	auto slideshowPanel = new SlideshowPanel( m_master.userConfig(), ui->computerMonitoringWidget );
	auto spotlightPanel = new SpotlightPanel( m_master.userConfig(), ui->computerMonitoringWidget );

	slideshowSpotlightSplitter->addWidget( slideshowPanel );
	slideshowSpotlightSplitter->addWidget( spotlightPanel );
	slideshowSpotlightSplitter->setStretchFactor( slideshowSpotlightSplitter->indexOf(slideshowPanel), 1 );
	slideshowSpotlightSplitter->setStretchFactor( slideshowSpotlightSplitter->indexOf(spotlightPanel), 1 );

	monitoringSplitter->addWidget( slideshowSpotlightSplitter );
	monitoringSplitter->addWidget( ui->computerMonitoringWidget );
	monitoringSplitter->setStretchFactor( monitoringSplitter->indexOf(slideshowSpotlightSplitter), 1 );
	monitoringSplitter->setStretchFactor( monitoringSplitter->indexOf(ui->computerMonitoringWidget), 1 );

	mainSplitter->addWidget( computerSelectPanel );
	mainSplitter->addWidget( screenshotManagementPanel );
	mainSplitter->addWidget( monitoringSplitter );

	mainSplitter->setStretchFactor( mainSplitter->indexOf(monitoringSplitter), 1 );


	static const QHash<QWidget *, QAbstractButton *> panelButtons{
		{ computerSelectPanel, ui->computerSelectPanelButton },
		{ screenshotManagementPanel, ui->screenshotManagementPanelButton },
		{ slideshowPanel, ui->slideshowPanelButton },
		{ spotlightPanel, ui->spotlightPanelButton }
	};

	for( auto it = panelButtons.constBegin(), end = panelButtons.constEnd(); it != end; ++it )
	{
		it.key()->hide();
		it.key()->installEventFilter( this );
		connect( *it, &QAbstractButton::toggled, it.key(), &QWidget::setVisible );
	}

	QList<int> splitterSizes;
	for( auto* splitter : { slideshowSpotlightSplitter, monitoringSplitter, mainSplitter } )
	{
		splitter->setHandleWidth( 7 );
		splitter->setStyleSheet( QStringLiteral("QSplitter::handle:hover{background-color:#66a0b3;}") );

		splitter->installEventFilter( this );

		int index = 0;

		const auto splitterStates = m_master.userConfig().splitterStates()[splitter->objectName()].toArray();
		splitterSizes.clear();
		splitterSizes.reserve( splitterStates.size() );

		for( const auto& sizeObject : splitterStates )
		{
			auto size = sizeObject.toInt();
			const auto widget = splitter->widget( index );
			const auto button = panelButtons.value( widget );

			if( widget )
			{
				if( button )
				{
					widget->setVisible( size > 0 );
					button->setChecked( size > 0 );
				}
				size = qAbs( size );
				if( splitter->orientation() == Qt::Horizontal )
				{
					widget->resize( size, widget->height() );
				}
				else
				{
					widget->resize( widget->width(), size );
				}

				widget->setProperty( originalSizePropertyName(), widget->size() );
			}
			splitterSizes.append( size );
			++index;
		}
		splitter->setSizes( splitterSizes );
	}

	const auto SplitterContentBaseSize = 500;

	if( spotlightPanel->property( originalSizePropertyName() ).isNull() ||
		slideshowPanel->property( originalSizePropertyName() ).isNull() )
	{
		slideshowSpotlightSplitter->setSizes( { SplitterContentBaseSize, SplitterContentBaseSize } );
	}

	if( slideshowSpotlightSplitter->property( originalSizePropertyName() ).isNull() ||
		ui->computerMonitoringWidget->property( originalSizePropertyName() ).isNull() )
	{
		monitoringSplitter->setSizes( { SplitterContentBaseSize, SplitterContentBaseSize } );
	}

	ui->centralLayout->addWidget( mainSplitter );

	if( VeyonCore::config().autoOpenComputerSelectPanel() )
	{
		ui->computerSelectPanelButton->setChecked( true );
	}

	// initialize filter controls
	connect( ui->filterLineEdit, &QLineEdit::textChanged,
			 this, [this]( const QString& filter ) { ui->computerMonitoringWidget->setSearchFilter( filter ); } );
	connect( ui->filterPoweredOnComputersButton, &QToolButton::toggled,
			 this, [this]( bool enabled ) { ui->computerMonitoringWidget->setFilterPoweredOnComputers( enabled ); } );
	connect( ui->filterComputersWithLoggedOnUsersButton, &QToolButton::toggled,
			 this, [this]( bool enabled ) { ui->computerMonitoringWidget->setFilterComputersWithLoggedOnUsers( enabled ); } );
	ui->filterPoweredOnComputersButton->setChecked(m_master.userConfig().filterPoweredOnComputers());
	ui->filterComputersWithLoggedOnUsersButton->setChecked(m_master.userConfig().filterComputersWithLoggedOnUsers());

	// initialize monitoring screen size slider
	ui->gridSizeSlider->setMinimum( ComputerMonitoringWidget::MinimumComputerScreenSize );
	ui->gridSizeSlider->setMaximum( ComputerMonitoringWidget::MaximumComputerScreenSize );

	ui->autoAdjustComputerIconSizeButton->setChecked( ui->computerMonitoringWidget->autoAdjustIconSize() );

	connect( ui->gridSizeSlider, &QSlider::valueChanged,
			 this, [this]( int size ) { ui->computerMonitoringWidget->setComputerScreenSize( size ); } );
	connect( ui->computerMonitoringWidget, &ComputerMonitoringWidget::computerScreenSizeAdjusted,
			 ui->gridSizeSlider, &QSlider::setValue );
	connect( ui->autoAdjustComputerIconSizeButton, &QToolButton::toggled,
			 this, [this]( bool enabled ) {
				 ui->computerMonitoringWidget->setAutoAdjustIconSize( enabled );
				 m_master.userConfig().setAutoAdjustMonitoringIconSize( enabled );
			 } );

	int size = ComputerMonitoringWidget::DefaultComputerScreenSize;
	if( m_master.userConfig().monitoringScreenSize() >= ComputerMonitoringWidget::MinimumComputerScreenSize )
	{
		size = m_master.userConfig().monitoringScreenSize();
	}

	ui->gridSizeSlider->setValue( size );
	ui->computerMonitoringWidget->setComputerScreenSize( size );

	// initialize computer placement controls
	auto customComputerPositionsControlMenu = new QMenu(this);
	const auto darkSuffix = VeyonCore::useDarkMode() ? QStringLiteral("-dark") : QString();
	customComputerPositionsControlMenu->addAction(QIcon(QStringLiteral(":/core/document-open%1.png").arg(darkSuffix)),
													tr("Load computer positions"),
													this, &MainWindow::loadComputerPositions);
	customComputerPositionsControlMenu->addAction(QIcon(QStringLiteral(":/core/document-save%1.png").arg(darkSuffix)),
													tr("Save computer positions"),
													this, &MainWindow::saveComputerPositions);
	ui->useCustomComputerPositionsButton->setMenu(customComputerPositionsControlMenu);

	ui->useCustomComputerPositionsButton->setChecked(m_master.userConfig().useCustomComputerPositions());
	connect(ui->useCustomComputerPositionsButton, &QToolButton::toggled,
			ui->computerMonitoringWidget, &ComputerMonitoringWidget::setUseCustomComputerPositions);
	connect(ui->alignComputersButton, &QToolButton::clicked,
			ui->computerMonitoringWidget, &ComputerMonitoringWidget::alignComputers);


	const auto toolButtons = findChildren<QToolButton*>();
	for(auto* btn : toolButtons)
	{
		btn->setIconSize(QSize(20, 20));
	}

	if (VeyonCore::useDarkMode())
	{
		ui->aboutButton->setIcon(QIcon(QStringLiteral(":/core/help-about-dark.png")));
		ui->filterComputersWithLoggedOnUsersButton->setIcon(QIcon(QStringLiteral(":/core/user-group-new-dark.png")));
		ui->autoAdjustComputerIconSizeButton->setIcon(QIcon(QStringLiteral(":/master/zoom-fit-best-dark.png")));
		ui->alignComputersButton->setIcon(QIcon(QStringLiteral(":/master/align-grid-dark.png")));
		ui->useCustomComputerPositionsButton->setIcon(QIcon(QStringLiteral(":/master/exchange-positions-zorder-dark.png")));
		ui->filterPoweredOnComputersButton->setIcon(QIcon(QStringLiteral(":/master/powered-on-dark.png")));
	}

	// create the main toolbar
	ui->toolBar->layout()->setSpacing( 2 );
	ui->toolBar->toggleViewAction()->setEnabled( false );

	addToolBar( Qt::TopToolBarArea, ui->toolBar );

	m_featureGroupingEnabled = m_master.userConfig().groupToolBarFeatures();
	populateToolBar();

#ifdef Q_OS_MACOS
	setupMacOSMenuBar();
#endif
}



MainWindow::~MainWindow()
{
	ui->computerMonitoringWidget->saveConfiguration();

	delete ui;
}



bool MainWindow::initAuthentication()
{
	if( VeyonCore::instance()->initAuthentication() )
	{
		return true;
	}

	if( VeyonCore::config().authenticationMethod() == VeyonCore::AuthenticationMethod::KeyFileAuthentication )
	{
		QMessageBox::information(nullptr,
								 tr("Authentication impossible"),
								 tr("No authentication key files were found or your current ones "
									"are outdated. Please create new key files using Veyon "
									"Configurator. Alternatively set up logon authentication "
									"using Veyon Configurator. Otherwise you won't be "
									"able to access computers using Veyon."));
	}

	return false;
}



bool MainWindow::initAccessControl()
{
	if (VeyonCore::config().accessControlForMasterEnabled())
	{
		const auto accessingUser = VeyonCore::authenticationCredentials().hasCredentials(AuthenticationCredentials::Type::UserLogon) ?
									   VeyonCore::authenticationCredentials().logonUsername() :
									   VeyonCore::platform().userFunctions().queryCurrentUserProperty(PlatformUserFunctions::UserProperty::LoginName);
		const auto accessControlResult = VeyonCore::builtinFeatures().accessControlProvider()
										 .checkAccess(accessingUser,
													  QHostAddress(QHostAddress::LocalHost).toString(),
													  {});
		if( accessControlResult.access == AccessControlProvider::Access::Deny )
		{
			vWarning() << "user" << VeyonCore::authenticationCredentials().logonUsername()
					   << "is not allowed to access computers";
			QMessageBox::critical( nullptr, tr( "Access denied" ),
								   tr( "According to the local configuration you're not allowed "
									   "to access computers in the network. Please log in with a different "
									   "account or let your system administrator check the local configuration." ) );
			return false;
		}
	}

	return true;
}



void MainWindow::reloadSubFeatures()
{
	for( const auto& updateMenu : std::as_const( m_subFeatureMenuUpdaters ) )
	{
		updateMenu();
	}
}



ComputerControlInterfaceList MainWindow::selectedComputerControlInterfaces() const
{
	return ui->computerMonitoringWidget->selectedComputerControlInterfaces();
}



void MainWindow::closeEvent( QCloseEvent* event )
{
	if( m_master.currentMode() != VeyonCore::builtinFeatures().monitoringMode().feature().uid() )
	{
		const Feature& activeFeature = VeyonCore::featureManager().feature( m_master.currentMode() );

		QMessageBox::information(this, tr("Feature active"),
								 tr("The feature \"%1\" is still active. Please stop it before closing Veyon.")
								 .arg(activeFeature.displayName()));
		event->ignore();
		return;
	}

	QJsonObject splitterStates;
	const auto splitters = findChildren<QSplitter *>();
	for( const auto* splitter : splitters )
	{
		const auto sizes = splitter->sizes();
		QJsonArray splitterSizes;
		int i = 0;
		int hiddenSize = 0;
		for( auto size : sizes )
		{
			auto widget = splitter->widget(i);
			const auto originalSize = widget->property( originalSizePropertyName() ).toSize();
			if( widget->size().isEmpty() && originalSize.isEmpty() == false )
			{
				size = splitter->orientation() == Qt::Horizontal ? -originalSize.width() : -originalSize.height();
				hiddenSize += qAbs(size);
			}
			else
			{
				size -= hiddenSize;
			}

			splitterSizes.append( size );
			++i;
		}
		splitterStates[splitter->objectName()] = splitterSizes;
	}

	m_master.userConfig().setSplitterStates( splitterStates );

	m_master.userConfig().setWindowState( QString::fromLatin1( saveState().toBase64() ) );
	m_master.userConfig().setWindowGeometry( QString::fromLatin1( saveGeometry().toBase64() ) );

	QMainWindow::closeEvent( event );
}



bool MainWindow::eventFilter( QObject* object, QEvent* event )
{
	if( event->type() == QEvent::Resize )
	{
		const auto widget = qobject_cast<QWidget *>( object );
		const auto resizeEvent = static_cast<QResizeEvent *>( event );

		if( resizeEvent->oldSize().isEmpty() == false )
		{
			widget->setProperty( originalSizePropertyName(), resizeEvent->oldSize() );
		}
	}

	return QMainWindow::eventFilter( object, event );
}



void MainWindow::keyPressEvent( QKeyEvent* event )
{
	switch( event->key() )
	{
	case Qt::Key_F5:
		VeyonCore::networkObjectDirectoryManager().configuredDirectory()->update();
		m_master.computerControlListModel().reload();
		event->accept();
		break;
	case Qt::Key_F11:
		QWidget::setWindowState( QWidget::windowState() ^ Qt::WindowFullScreen );
		event->accept();
		break;
	default:
		QMainWindow::keyPressEvent( event );
		break;
	}
}



void MainWindow::showAboutDialog()
{
	AboutDialog( this ).exec();
}



void MainWindow::showAddOnStore()
{
	AddOnStoreDialog dialog( this );
	dialog.exec();
}



void MainWindow::openConfigurator()
{
	const auto configuratorFilePath = VeyonCore::filesystem().programFilePath( QStringLiteral("veyon-configurator") );
	if( QProcess::startDetached( configuratorFilePath, {} ) == false )
	{
		QMessageBox::warning( this, tr( "Veyon Configurator" ),
							  tr( "Could not start Veyon Configurator (%1)." ).arg( configuratorFilePath ) );
	}
}



void MainWindow::addAddOnStoreButton()
{
	if( VeyonCore::config().addOnStoreForTeachersEnabled() == false &&
		VeyonCore::platform().coreFunctions().isRunningAsAdmin() == false )
	{
		return;
	}

	// push the button to the right end of the toolbar
	auto spacer = new QWidget( ui->toolBar );
	spacer->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
	m_toolBarActions.append( ui->toolBar->addWidget( spacer ) );

	auto addOnStoreButton = new ToolButton( QIcon( QStringLiteral(":/core/addon-store.png") ),
											tr( "Add-Ons" ), {},
											tr( "Open the Add-On Store to enable, disable, install or create add-ons." ) );
	addOnStoreButton->setObjectName( QStringLiteral("AddOnStoreButton") );
	connect( addOnStoreButton, &QToolButton::clicked, this, &MainWindow::showAddOnStore );
	m_toolBarActions.append( addOnStoreButton->addTo( ui->toolBar ) );
}



#ifdef Q_OS_MACOS
void MainWindow::setupMacOSMenuBar()
{
	// native global menu bar with entries in the application menu
	auto applicationMenu = menuBar()->addMenu( tr( "Veyon" ) );

	auto aboutAction = applicationMenu->addAction( tr( "About Veyon Master" ) );
	aboutAction->setMenuRole( QAction::AboutRole );
	connect( aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog );

	auto configuratorAction = applicationMenu->addAction( tr( "Veyon Configurator…" ) );
	configuratorAction->setMenuRole( QAction::PreferencesRole );
	connect( configuratorAction, &QAction::triggered, this, &MainWindow::openConfigurator );

	auto addOnStoreAction = applicationMenu->addAction( tr( "Add-On Store…" ) );
	addOnStoreAction->setMenuRole( QAction::ApplicationSpecificRole );
	connect( addOnStoreAction, &QAction::triggered, this, &MainWindow::showAddOnStore );

	auto viewMenu = menuBar()->addMenu( tr( "View" ) );

	auto refreshAction = viewMenu->addAction( tr( "Refresh computers" ) );
	refreshAction->setShortcut( QKeySequence::Refresh );
	connect( refreshAction, &QAction::triggered, this, [this]() {
		VeyonCore::networkObjectDirectoryManager().configuredDirectory()->update();
		m_master.computerControlListModel().reload();
	} );

	auto fullScreenAction = viewMenu->addAction( tr( "Enter Full Screen" ) );
	fullScreenAction->setShortcut( QKeySequence::FullScreen );
	connect( fullScreenAction, &QAction::triggered, this, [this]() {
		QWidget::setWindowState( QWidget::windowState() ^ Qt::WindowFullScreen );
	} );
}
#endif



void MainWindow::setFeatureGroupingEnabled( bool enabled )
{
	if( enabled != m_featureGroupingEnabled )
	{
		m_featureGroupingEnabled = enabled;
		populateToolBar();
	}
}



QList<MainWindow::ToolBarGroup> MainWindow::toolBarGroups() const
{
	const auto single = []( const QString& featureName ) {
		return ToolBarGroup{ .name = featureName, .stackedMembers = { { .featureName = featureName } } };
	};

	// toolbar layout if related features are grouped - modes always come first and stay separate,
	// features not listed here (e.g. from other plugins) are appended in their usual order
	return {
		{
			.name = QStringLiteral("RemoteAccessGroup"),
			.label = tr( "Remote" ),
			.stackedMembers = {
				{ .featureName = QStringLiteral("RemoteView"), .label = tr( "View" ) },
				{ .featureName = QStringLiteral("RemoteControl"), .label = tr( "Control" ) }
			}
		},
		{
			// menu only, so that no single click can power down computers or log off users
			.name = QStringLiteral("PowerGroup"),
			.label = tr( "Power" ),
			.description = tr( "Power on, reboot or power down computers, log off or log in users" ),
			.menuMembers = {
				{ .featureName = QStringLiteral("PowerOn") },
				{ .featureName = QStringLiteral("Reboot") },
				{ .featureName = QStringLiteral("PowerDown") },
				{ .featureName = QStringLiteral("UserLogoff"), .separatorBefore = true },
				{ .featureName = QStringLiteral("UserLogin"), .label = tr( "Log in…" ) }
			},
			.iconFeature = QStringLiteral("PowerDown")
		},
		single( QStringLiteral("TextMessage") ),
		single( QStringLiteral("StartApp") ),
		{
			// the DesktopServices plugin looks up the button with the saved websites by this name
			.name = QStringLiteral("OpenWebsite"),
			.stackedMembers = {
				{ .featureName = QStringLiteral("WebPageSender"), .label = tr( "Website" ) }
			},
			.menuMembers = {
				{ .featureName = QStringLiteral("OpenWebsite"), .label = tr( "Open website…" ), .inlineSubFeatures = true }
			}
		},
		single( QStringLiteral("Screenshot") ),
		{
			.name = QStringLiteral("FileTransferGroup"),
			.label = tr( "Files" ),
			.description = tr( "More file functions" ),
			.stackedMembers = {
				{ .featureName = QStringLiteral("DistributeFiles") },
				{ .featureName = QStringLiteral("FileCollect") }
			},
			.menuMembers = {
				{ .featureName = QStringLiteral("FileSender"), .label = tr( "Send files…" ) },
				{ .featureName = QStringLiteral("FileManager"), .label = tr( "File manager…" ) }
			}
		},
		single( QStringLiteral("TaskManager") ),
		single( QStringLiteral("VolumeControl") ),
		single( QStringLiteral("InternetAccess") ),
		single( QStringLiteral("WebsiteFilter") ),
		{
			.name = QStringLiteral("ScriptAddOnsGroup"),
			.label = tr( "Scripts" ),
			.description = tr( "Run a script add-on on the computers" ),
			.menuMemberPrefixes = { QStringLiteral("ScriptAddOn-"), QStringLiteral("ScriptAction-") },
			.iconUrl = QStringLiteral(":/scriptedaddons/scripted-addons.png")
		}
	};
}



void MainWindow::populateToolBar()
{
	clearToolBar();

	if( m_featureGroupingEnabled )
	{
		addGroupedFeaturesToToolBar();
	}
	else
	{
		addFeaturesToToolBar();
	}

	// check the button of the active mode before creating the menus (an active mode has none)
	updateModeButtons();
	reloadSubFeatures();

	addAddOnStoreButton();
}



void MainWindow::clearToolBar()
{
	m_subFeatureMenuUpdaters.clear();

	// the buttons are deleted later, so remove them from the mode group right away
	const auto modeButtons = m_modeGroup->buttons();
	for( auto* button : modeButtons )
	{
		m_modeGroup->removeButton( button );
	}

	// actions with shortcuts from group menus have been added to this window as well
	const auto windowActions = actions();
	for( auto* action : windowActions )
	{
		if( action->property( toolBarShortcutPropertyName() ).toBool() )
		{
			removeAction( action );
		}
	}

	for( auto* action : std::as_const( m_toolBarActions ) )
	{
		ui->toolBar->removeAction( action );
		action->deleteLater();
	}
	m_toolBarActions.clear();
}



void MainWindow::addFeaturesToToolBar()
{
	for( const auto& feature : m_master.features() )
	{
		if( feature.testFlag( Feature::Flag::Meta ) == false )
		{
			addFeatureButton( feature );
		}
	}
}



void MainWindow::addGroupedFeaturesToToolBar()
{
	FeatureList ungroupedFeatures;

	for( const auto& feature : m_master.features() )
	{
		if( feature.testFlag( Feature::Flag::Meta ) )
		{
			continue;
		}

		// modes stay separate so that an active mode can be seen at a glance
		if( feature.testFlag( Feature::Flag::Mode ) )
		{
			addFeatureButton( feature );
		}
		else
		{
			ungroupedFeatures.append( feature );
		}
	}

	const auto takeFeatures = [&ungroupedFeatures]( const std::function<bool(const Feature&)>& filter ) {
		FeatureList features;
		for( auto it = ungroupedFeatures.begin(); it != ungroupedFeatures.end(); )
		{
			if( filter( *it ) )
			{
				features.append( *it );
				it = ungroupedFeatures.erase( it );
			}
			else
			{
				++it;
			}
		}
		return features;
	};

	// members whose features are not available (add-on not installed, feature disabled) are left out
	const auto availableMembers = [&takeFeatures]( const QList<ToolBarGroupMember>& members ) {
		QList<ToolBarGroupMember> available;
		for( const auto& member : members )
		{
			const auto features = takeFeatures( [&member]( const Feature& feature ) {
				return feature.name() == member.featureName;
			} );
			if( features.isEmpty() == false )
			{
				auto availableMember = member;
				availableMember.feature = features.constFirst();
				if( availableMember.label.isEmpty() )
				{
					availableMember.label = availableMember.feature.displayName();
				}
				available.append( availableMember );
			}
		}
		return available;
	};

	const auto groups = toolBarGroups();
	for( const auto& group : groups )
	{
		const auto stackedMembers = availableMembers( group.stackedMembers );
		auto menuMembers = availableMembers( group.menuMembers );

		const auto prefixedFeatures = takeFeatures( [&group]( const Feature& feature ) {
			return std::any_of( group.menuMemberPrefixes.cbegin(), group.menuMemberPrefixes.cend(),
								[&feature]( const QString& prefix ) { return feature.name().startsWith( prefix ); } );
		} );
		for( const auto& feature : prefixedFeatures )
		{
			const ToolBarGroupMember member{ .featureName = feature.name(), .label = feature.displayName(), .feature = feature };
			menuMembers.append( member );
		}

		addFeatureGroup( group, stackedMembers, menuMembers );
	}

	for( const auto& feature : std::as_const( ungroupedFeatures ) )
	{
		addFeatureButton( feature );
	}
}



void MainWindow::addFeatureGroup( const ToolBarGroup& group,
								  const QList<ToolBarGroupMember>& stackedMembers,
								  const QList<ToolBarGroupMember>& menuMembers )
{
	const auto memberCount = stackedMembers.size() + menuMembers.size();
	if( memberCount == 0 )
	{
		return;
	}

	if( memberCount == 1 )
	{
		// nothing left to group with
		addFeatureButton( stackedMembers.isEmpty() ? menuMembers.constFirst().feature : stackedMembers.constFirst().feature );
		return;
	}

	if( stackedMembers.size() >= 2 )
	{
		auto stack = new StackedToolButtons( group.description );
		stack->setObjectName( group.name );

		for( const auto& member : stackedMembers )
		{
			auto button = createFeatureButton( member.feature, member.label );
			button->setToolTipLabel( member.feature.displayName() );
			stack->addButton( button );

			m_subFeatureMenuUpdaters.append( [this, button = QPointer<QToolButton>( button ), feature = member.feature]() {
				if( button )
				{
					addSubFeaturesToToolButton( button, feature );
				}
			} );
		}

		auto action = ui->toolBar->addWidget( stack );
		action->setText( group.label );
		m_toolBarActions.append( action );

		if( menuMembers.isEmpty() == false )
		{
			m_subFeatureMenuUpdaters.append( [this, stack = QPointer<StackedToolButtons>( stack ), group, menuMembers]() {
				if( stack )
				{
					discardMenu( stack->menu() );
					stack->setMenu( createGroupMenu( group, menuMembers, stack ) );
				}
			} );
		}

		return;
	}

	ToolButton* button = nullptr;

	if( stackedMembers.isEmpty() == false )
	{
		// the main part runs the primary feature, the arrow opens the menu
		const auto& primaryMember = stackedMembers.constFirst();
		button = createFeatureButton( primaryMember.feature, primaryMember.label );
		button->setPopupMode( QToolButton::MenuButtonPopup );
	}
	else
	{
		auto iconUrl = group.iconUrl;
		if( iconUrl.isEmpty() || QFile::exists( iconUrl ) == false )
		{
			// reuse the icon of a member
			iconUrl = menuMembers.constFirst().feature.iconUrl();
			for( const auto& member : menuMembers )
			{
				if( member.featureName == group.iconFeature )
				{
					iconUrl = member.feature.iconUrl();
				}
			}
		}

		button = new ToolButton( QIcon( iconUrl ), group.label, {}, group.description );
		button->setPopupMode( QToolButton::InstantPopup );
	}

	button->setObjectName( group.name );
	m_toolBarActions.append( button->addTo( ui->toolBar ) );

	m_subFeatureMenuUpdaters.append( [this, button = QPointer<QToolButton>( button ), group, menuMembers]() {
		if( button )
		{
			discardMenu( button->menu() );
			button->setMenu( createGroupMenu( group, menuMembers, button ) );
		}
	} );
}



ToolButton* MainWindow::createFeatureButton( const Feature& feature, const QString& label )
{
	auto button = new ToolButton( QIcon( feature.iconUrl() ),
								  label,
								  feature.displayNameActive(),
								  feature.description(),
								  feature.shortcut() );
	button->setObjectName( feature.name() );

	connect( button, &QToolButton::clicked, this, [=, this]() { runFeatureFromToolBar( feature ); } );

	if( feature.testFlag( Feature::Flag::Mode ) )
	{
		button->setCheckable( true );
		m_modeGroup->addButton( button, buttonId( feature ) );
	}

	return button;
}



void MainWindow::addFeatureButton( const Feature& feature )
{
	auto button = createFeatureButton( feature, feature.displayName() );
	m_toolBarActions.append( button->addTo( ui->toolBar ) );

	m_subFeatureMenuUpdaters.append( [this, button = QPointer<QToolButton>( button ), feature]() {
		if( button )
		{
			addSubFeaturesToToolButton( button, feature );
		}
	} );
}



QMenu* MainWindow::createGroupMenu( const ToolBarGroup& group, const QList<ToolBarGroupMember>& members, QWidget* parent )
{
	auto menu = new QMenu( parent );
	menu->setObjectName( group.name );
	menu->setToolTipsVisible( true );

	for( const auto& member : members )
	{
		const auto& feature = member.feature;
		const auto subFeatures = m_master.subFeatures( feature.uid() );

		if( member.separatorBefore && menu->isEmpty() == false )
		{
			menu->addSeparator();
		}

		if( member.inlineSubFeatures )
		{
			// e.g. saved websites, followed by the feature itself (which may be listed as sub-feature as well)
			bool hasSubFeatureActions = false;
			for( const auto& subFeature : subFeatures )
			{
				if( subFeature.uid() != feature.uid() )
				{
					addSubFeatureAction( menu, subFeature, feature, nullptr );
					hasSubFeatureActions = true;
				}
			}
			if( hasSubFeatureActions )
			{
				menu->addSeparator();
			}
			addFeatureAction( menu, feature, member.label );
		}
		else if( subFeatures.isEmpty() )
		{
			addFeatureAction( menu, feature, member.label );
		}
		else
		{
			auto subMenu = menu->addMenu( QIcon( feature.iconUrl() ), member.label );
			subMenu->setObjectName( feature.name() );
			subMenu->setToolTipsVisible( true );
			subMenu->menuAction()->setToolTip( feature.description() );

			for( const auto& subFeature : subFeatures )
			{
				addSubFeatureAction( subMenu, subFeature, feature, nullptr );
			}
		}
	}

	return menu;
}



void MainWindow::addFeatureAction( QMenu* menu, const Feature& feature, const QString& label )
{
	auto action = menu->addAction( QIcon( feature.iconUrl() ), label );
	action->setToolTip( feature.description() );
	action->setObjectName( feature.uid().toString() );

	connect( action, &QAction::triggered, this, [=, this]() { runFeatureFromToolBar( feature ); } );

	if( feature.shortcut().isEmpty() == false )
	{
		// also add the action to this window so that its shortcut works while the menu is closed
		action->setShortcut( feature.shortcut() );
		action->setProperty( toolBarShortcutPropertyName(), true );
		addAction( action );
	}
}



void MainWindow::addSubFeatureAction( QMenu* menu, const Feature& subFeature, const Feature& parentFeature,
									  QToolButton* parentButton )
{
	const auto parentFeatureIsMode = parentFeature.testFlag( Feature::Flag::Mode );

	auto action = menu->addAction( QIcon( subFeature.iconUrl() ), subFeature.displayName() );
	action->setShortcut( subFeature.shortcut() );
	action->setToolTip( subFeature.description() );
	action->setObjectName( subFeature.uid().toString() );

	if( subFeature.testFlag( Feature::Flag::Option ) )
	{
		action->setCheckable( true );
		action->setChecked( subFeature.testFlag( Feature::Flag::Checked ) );
	}

	connect( action, &QAction::triggered, this, [=, this]() {
		m_master.runFeature( subFeature );
		if( parentFeatureIsMode )
		{
			if( parentButton && subFeature.testFlag( Feature::Flag::Option ) == false )
			{
				parentButton->setChecked( true );
			}
			reloadSubFeatures();
		}
	} );
}



void MainWindow::addSubFeaturesToToolButton( QToolButton* button, const Feature& parentFeature )
{
	if( button->menu() )
	{
		discardMenu( button->menu() );
		button->setMenu( nullptr );
	}

	const auto parentFeatureIsMode = parentFeature.testFlag( Feature::Flag::Mode );
	const auto subFeatures = m_master.subFeatures( parentFeature.uid() );

	if( subFeatures.isEmpty() ||
		( parentFeatureIsMode && button->isChecked() ) )
	{
		return;
	}

	auto menu = new QMenu( button );
	menu->setObjectName( parentFeature.name() );
	menu->setToolTipsVisible( true );

	for( const auto& subFeature : subFeatures )
	{
		addSubFeatureAction( menu, subFeature, parentFeature, button );
	}

	button->setMenu( menu );
	button->setPopupMode( ToolButton::InstantPopup );
}



void MainWindow::discardMenu( QMenu* menu )
{
	if( menu )
	{
		const auto menuActions = menu->actions();
		for( auto* action : menuActions )
		{
			removeAction( action );
		}

		menu->close();
		menu->deleteLater();
	}
}



void MainWindow::runFeatureFromToolBar( const Feature& feature )
{
	m_master.runFeature( feature );
	updateModeButtonGroup();
	if( feature.testFlag( Feature::Flag::Mode ) )
	{
		reloadSubFeatures();
	}
}



void MainWindow::updateModeButtons()
{
	const auto& currentModeFeature = VeyonCore::featureManager().feature( m_master.currentMode() );

	for( const auto& feature : m_master.features() )
	{
		if( feature.testFlag( Feature::Flag::Mode ) &&
			( feature == currentModeFeature ||
			  m_master.subFeatures( feature.uid() ).contains( currentModeFeature ) ) )
		{
			const auto button = m_modeGroup->button( buttonId( feature ) );
			if( button )
			{
				button->setChecked( true );
			}
		}
	}
}



void MainWindow::updateModeButtonGroup()
{
	const auto& monitoringMode = VeyonCore::builtinFeatures().monitoringMode().feature();

	if( m_master.currentMode() == monitoringMode.uid() )
	{
		m_modeGroup->button( buttonId( monitoringMode ) )->setChecked( true );
	}
}



void MainWindow::loadComputerPositions()
{
	if (const auto fileName = QFileDialog::getOpenFileName(this, tr("Load computer positions"),
														   QDir::homePath(), tr("JSON files (*.json)"));
		!fileName.isEmpty())
	{
		if (QFile file(fileName); file.open(QFile::ReadOnly))
		{
			const auto& computerPositionsProperty = m_master.userConfig().computerPositionsProperty();
			const auto config = QJsonDocument::fromJson(file.readAll()).object();
			const auto uiConfig = config[computerPositionsProperty.parentKey()].toObject();
			const auto computerPositions = uiConfig[computerPositionsProperty.key()].toObject()[QStringLiteral("JsonStoreArray")].toArray();

			ui->computerMonitoringWidget->loadPositions(computerPositions);
			ui->computerMonitoringWidget->setUseCustomComputerPositions(true);
			ui->computerMonitoringWidget->doItemsLayout();
		}
	}
}



void MainWindow::saveComputerPositions()
{
	if (const auto fileName = QFileDialog::getSaveFileName(this, tr("Save computer positions"),
														   QDir::homePath(), tr("JSON files (*.json)"));
		!fileName.isEmpty())
	{
		if (QFile file(fileName); file.open(QFile::WriteOnly | QFile::Truncate))
		{
			const auto& computerPositionsProperty = m_master.userConfig().computerPositionsProperty();

			// create structure identical to UserConfig so file with positions can be used as template for UserConfig
			QJsonObject computerPositions;
			computerPositions[QStringLiteral("JsonStoreArray")] = ui->computerMonitoringWidget->savePositions();
			QJsonObject uiConfig;
			uiConfig[computerPositionsProperty.key()] = computerPositions;
			QJsonObject config;
			config[computerPositionsProperty.parentKey()] = uiConfig;
			file.write(QJsonDocument(config).toJson());
		}
	}
}
