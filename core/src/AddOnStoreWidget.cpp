/*
 * AddOnStoreWidget.cpp - implementation of AddOnStoreWidget class
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

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QVersionNumber>

#include "AddOnStoreWidget.h"
#include "Filesystem.h"
#include "PlatformCoreFunctions.h"
#include "ScriptAddOnEditorDialog.h"
#include "VeyonConfiguration.h"


namespace {

// a small iOS/macOS like switch button
class ToggleSwitch : public QAbstractButton
{
public:
	explicit ToggleSwitch( QWidget* parent = nullptr ) :
		QAbstractButton( parent )
	{
		setCheckable( true );
		setCursor( Qt::PointingHandCursor );
		setFocusPolicy( Qt::StrongFocus );
		setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
	}

	QSize sizeHint() const override
	{
		return { 44, 24 };
	}

protected:
	void paintEvent( QPaintEvent* event ) override
	{
		Q_UNUSED(event)

		QPainter painter( this );
		painter.setRenderHint( QPainter::Antialiasing );

		const auto r = QRectF( rect() ).adjusted( 1, 1, -1, -1 );

		QColor trackColor = isChecked() ? QColor( 0x19, 0x8c, 0xb3 ) : palette().color( QPalette::Mid );
		if( isEnabled() == false )
		{
			trackColor.setAlpha( 90 );
		}

		painter.setPen( hasFocus() ? QPen( palette().color( QPalette::Highlight ), 1.5 ) : Qt::NoPen );
		painter.setBrush( trackColor );
		painter.drawRoundedRect( r, r.height() / 2, r.height() / 2 );

		const auto knobDiameter = r.height() - 4;
		const auto x = isChecked() ? r.right() - knobDiameter - 2 : r.left() + 2;
		painter.setPen( Qt::NoPen );
		painter.setBrush( isEnabled() ? QColor( Qt::white ) : QColor( 240, 240, 240 ) );
		painter.drawEllipse( QRectF( x, r.top() + 2, knobDiameter, knobDiameter ) );
	}
};



QLabel* createBadge( const QString& text, const QColor& color )
{
	auto label = new QLabel( text );
	label->setStyleSheet( QStringLiteral("QLabel { color: %1; background-color: rgba(%2,%3,%4,38); "
										 "border-radius: 7px; padding: 1px 7px; font-weight: bold; }")
						  .arg( color.name() ).arg( color.red() ).arg( color.green() ).arg( color.blue() ) );
	auto font = label->font();
	font.setPointSizeF( font.pointSizeF() * 0.85 );
	label->setFont( font );
	return label;
}



QLabel* createSecondaryLabel( const QString& text, qreal scale = 0.92 )
{
	auto label = new QLabel( text );
	label->setWordWrap( true );
	label->setTextInteractionFlags( Qt::TextSelectableByMouse );
	auto font = label->font();
	font.setPointSizeF( font.pointSizeF() * scale );
	label->setFont( font );
	auto palette = label->palette();
	palette.setColor( QPalette::WindowText, palette.color( QPalette::PlaceholderText ) );
	label->setPalette( palette );
	return label;
}



QFrame* createCardFrame()
{
	auto frame = new QFrame;
	frame->setObjectName( QStringLiteral("addOnCard") );
	frame->setStyleSheet( QStringLiteral("QFrame#addOnCard { border: 1px solid palette(mid); border-radius: 10px; "
										 "background-color: palette(base); }") );
	frame->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Maximum );
	return frame;
}



QString platformDisplayName( const QString& platform )
{
	if( platform.startsWith( QLatin1String("windows") ) )
	{
		return QStringLiteral("Windows");
	}
	if( platform.startsWith( QLatin1String("macos") ) )
	{
		return QStringLiteral("macOS");
	}
	if( platform.startsWith( QLatin1String("linux") ) )
	{
		return QStringLiteral("Linux");
	}
	return platform;
}

}



AddOnStoreWidget::AddOnStoreWidget( Mode mode, QWidget* parent ) :
	QWidget( parent ),
	m_mode( mode ),
	m_networkAccessManager( new QNetworkAccessManager( this ) )
{
	auto mainLayout = new QVBoxLayout( this );
	mainLayout->setContentsMargins( mode == Mode::Master ? 12 : 0, mode == Mode::Master ? 12 : 0,
									mode == Mode::Master ? 12 : 0, mode == Mode::Master ? 12 : 0 );
	mainLayout->setSpacing( 10 );

	// header
	auto headerLayout = new QHBoxLayout;
	auto headerIcon = new QLabel;
	headerIcon->setPixmap( QIcon( QStringLiteral(":/core/addon-store.png") ).pixmap( 48, 48 ) );
	headerLayout->addWidget( headerIcon );

	auto headerTextLayout = new QVBoxLayout;
	headerTextLayout->setSpacing( 2 );
	auto titleLabel = new QLabel( tr( "Add-On Store" ) );
	auto titleFont = titleLabel->font();
	titleFont.setPointSizeF( titleFont.pointSizeF() * 1.6 );
	titleFont.setBold( true );
	titleLabel->setFont( titleFont );
	headerTextLayout->addWidget( titleLabel );
	headerTextLayout->addWidget( createSecondaryLabel(
		tr( "Extend Veyon with additional features. Enable or disable add-ons, install add-ons from files or "
			"repositories and create your own add-ons from scripts." ) ) );
	headerLayout->addLayout( headerTextLayout, 1 );
	mainLayout->addLayout( headerLayout );

	// tool row
	auto toolLayout = new QHBoxLayout;
	m_searchEdit = new QLineEdit;
	m_searchEdit->setPlaceholderText( tr( "Search add-ons" ) );
	m_searchEdit->setClearButtonEnabled( true );
	connect( m_searchEdit, &QLineEdit::textChanged, this, &AddOnStoreWidget::applyFilter );
	toolLayout->addWidget( m_searchEdit, 2 );

	m_categoryComboBox = new QComboBox;
	m_categoryComboBox->addItem( tr( "All categories" ), -1 );
	for( const auto category : { AddOnInterface::Category::Monitoring,
								 AddOnInterface::Category::Control,
								 AddOnInterface::Category::Files,
								 AddOnInterface::Category::Network,
								 AddOnInterface::Category::Communication,
								 AddOnInterface::Category::Scripting,
								 AddOnInterface::Category::Other } )
	{
		m_categoryComboBox->addItem( AddOnManager::categoryDisplayName( category ), int(category) );
	}
	connect( m_categoryComboBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this, &AddOnStoreWidget::applyFilter );
	toolLayout->addWidget( m_categoryComboBox, 1 );

	auto installButton = new QPushButton( tr( "Install from file…" ) );
	connect( installButton, &QPushButton::clicked, this, &AddOnStoreWidget::installFromFile );
	toolLayout->addWidget( installButton );

	auto createButton = new QPushButton( QIcon( QStringLiteral(":/core/addon-script.png") ), tr( "Create add-on…" ) );
	createButton->setToolTip( tr( "Create your own add-on which runs scripts on student computers" ) );
	connect( createButton, &QPushButton::clicked, this, &AddOnStoreWidget::createScriptAddOn );
	toolLayout->addWidget( createButton );

	mainLayout->addLayout( toolLayout );

	// tabs
	m_tabWidget = new QTabWidget;
	mainLayout->addWidget( m_tabWidget, 1 );

	const auto createScrollPage = []( QVBoxLayout*& contentLayout ) {
		auto scrollArea = new QScrollArea;
		scrollArea->setWidgetResizable( true );
		scrollArea->setFrameShape( QFrame::NoFrame );
		scrollArea->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
		auto content = new QWidget;
		contentLayout = new QVBoxLayout( content );
		contentLayout->setSpacing( 8 );
		contentLayout->setContentsMargins( 6, 6, 6, 6 );
		scrollArea->setWidget( content );
		return scrollArea;
	};

	auto installedPage = new QWidget;
	auto installedPageLayout = new QVBoxLayout( installedPage );
	installedPageLayout->setContentsMargins( 0, 6, 0, 0 );
	m_showSystemComponentsCheckBox = new QCheckBox( tr( "Show system components" ) );
	connect( m_showSystemComponentsCheckBox, &QCheckBox::toggled, this, &AddOnStoreWidget::applyFilter );
	installedPageLayout->addWidget( m_showSystemComponentsCheckBox, 0, Qt::AlignRight );
	installedPageLayout->addWidget( createScrollPage( m_installedLayout ), 1 );
	m_tabWidget->addTab( installedPage, tr( "Installed" ) );

	auto catalogPage = new QWidget;
	auto catalogPageLayout = new QVBoxLayout( catalogPage );
	catalogPageLayout->setContentsMargins( 0, 6, 0, 0 );
	auto catalogToolLayout = new QHBoxLayout;
	m_catalogStatusLabel = createSecondaryLabel( {} );
	catalogToolLayout->addWidget( m_catalogStatusLabel, 1 );
	auto refreshCatalogButton = new QPushButton( tr( "Refresh" ) );
	connect( refreshCatalogButton, &QPushButton::clicked, this, &AddOnStoreWidget::fetchCatalog );
	catalogToolLayout->addWidget( refreshCatalogButton );
	auto repositoriesButton = new QPushButton( tr( "Repositories…" ) );
	connect( repositoriesButton, &QPushButton::clicked, this, &AddOnStoreWidget::manageRepositories );
	catalogToolLayout->addWidget( repositoriesButton );
	catalogPageLayout->addLayout( catalogToolLayout );
	catalogPageLayout->addWidget( createScrollPage( m_catalogLayout ), 1 );
	m_tabWidget->addTab( catalogPage, tr( "Discover" ) );

	connect( m_tabWidget, &QTabWidget::currentChanged, this, [this]( int index ) {
		if( index == 1 && m_catalogEntries.isEmpty() && m_pendingCatalogRequests == 0 )
		{
			fetchCatalog();
		}
	} );

	// footer
	auto footerLayout = new QHBoxLayout;
	m_footerLabel = new QLabel;
	m_footerLabel->setWordWrap( true );
	footerLayout->addWidget( m_footerLabel, 1 );
	if( m_mode == Mode::Master )
	{
		m_restartButton = new QPushButton( tr( "Restart Veyon Master" ) );
		connect( m_restartButton, &QPushButton::clicked, this, &AddOnStoreWidget::restartMaster );
		footerLayout->addWidget( m_restartButton );
	}
	mainLayout->addLayout( footerLayout );

	connect( &m_manager, &AddOnManager::addOnsChanged, this, [this]() { m_changesPending = true; } );

	refresh();
}



void AddOnStoreWidget::refresh()
{
	rebuildInstalledList();
	rebuildCatalogList();
}



QIcon AddOnStoreWidget::addOnIcon( const AddOnManager::AddOn& addOn )
{
	if( addOn.iconData.isEmpty() == false )
	{
		QPixmap pixmap;
		if( pixmap.loadFromData( addOn.iconData ) )
		{
			return QIcon( pixmap );
		}
	}

	if( addOn.iconUrl.isEmpty() == false )
	{
		const QIcon icon( addOn.iconUrl );
		if( icon.isNull() == false && icon.availableSizes().isEmpty() == false )
		{
			return icon;
		}
	}

	if( addOn.packageType == QLatin1String("script") )
	{
		return QIcon( QStringLiteral(":/core/addon-script.png") );
	}

	return QIcon( QStringLiteral(":/core/addon-generic.png") );
}



void AddOnStoreWidget::rebuildInstalledList()
{
	while( auto item = m_installedLayout->takeAt( 0 ) )
	{
		if( item->widget() )
		{
			item->widget()->hide();
			item->widget()->deleteLater();
		}
		delete item;
	}
	m_cards.clear();
	m_sectionHeaders.clear();

	m_addOns = m_manager.addOns();

	const QList<std::tuple<AddOnManager::Kind, QString, QString>> sections{
		{ AddOnManager::Kind::AddOn, tr( "Add-ons" ),
		  tr( "Add-ons bundled with this Veyon edition, installed from packages or created by you" ) },
		{ AddOnManager::Kind::BuiltinFeature, tr( "Built-in features" ),
		  tr( "Standard Veyon features which can be disabled if not needed" ) },
		{ AddOnManager::Kind::SystemComponent, tr( "System components" ),
		  tr( "Components required for Veyon to work - they can't be disabled" ) },
	};

	for( const auto& [kind, title, subtitle] : sections )
	{
		auto header = createSectionHeader( title, subtitle );
		header->setProperty( "kind", int(kind) );
		m_installedLayout->addWidget( header );
		m_sectionHeaders.append( header );

		for( const auto& addOn : std::as_const(m_addOns) )
		{
			if( addOn.kind != kind )
			{
				continue;
			}

			auto card = createCard( addOn );
			m_installedLayout->addWidget( card );

			CardInfo cardInfo;
			cardInfo.widget = card;
			cardInfo.searchText = QStringList{ addOn.name, addOn.description, addOn.summary, addOn.vendor, addOn.id }.
								  join( QLatin1Char(' ') ).toLower();
			cardInfo.category = addOn.category;
			cardInfo.kind = addOn.kind;
			m_cards.append( cardInfo );
		}
	}

	m_installedLayout->addStretch( 1 );

	applyFilter();
	updateFooter();
}



void AddOnStoreWidget::rebuildCatalogList()
{
	while( auto item = m_catalogLayout->takeAt( 0 ) )
	{
		if( item->widget() )
		{
			item->widget()->hide();
			item->widget()->deleteLater();
		}
		delete item;
	}
	m_catalogCards.clear();

	const auto repositories = VeyonCore::config().addOnRepositories();

	if( repositories.isEmpty() )
	{
		m_catalogStatusLabel->setText( tr( "No add-on repositories have been configured yet." ) );
		auto hint = createSecondaryLabel(
			tr( "An add-on repository is a web server or network share providing an index.json file which lists "
				"available add-on packages. Click \"Repositories…\" to add one. Alternatively install add-on "
				"packages (*.veyonaddon) directly using \"Install from file…\"." ), 1.0 );
		hint->setAlignment( Qt::AlignCenter );
		hint->setContentsMargins( 30, 30, 30, 30 );
		m_catalogLayout->addWidget( hint );
	}
	else if( m_pendingCatalogRequests > 0 )
	{
		m_catalogStatusLabel->setText( tr( "Loading add-on catalog…" ) );
	}
	else
	{
		m_catalogStatusLabel->setText( tr( "%n add-on(s) available from %1 repository(ies)", "", int(m_catalogEntries.size()) ).
									   arg( repositories.size() ) );

		for( const auto& [entry, indexUrl] : std::as_const(m_catalogEntries) )
		{
			auto card = createCatalogCard( entry, indexUrl );
			m_catalogLayout->addWidget( card );

			CardInfo cardInfo;
			cardInfo.widget = card;
			cardInfo.searchText = QStringList{ entry[QStringLiteral("name")].toString(),
											   entry[QStringLiteral("description")].toString(),
											   entry[QStringLiteral("vendor")].toString() }.join( QLatin1Char(' ') ).toLower();
			cardInfo.category = AddOnManager::categoryFromString( entry[QStringLiteral("category")].toString() );
			m_catalogCards.append( cardInfo );
		}
	}

	m_catalogLayout->addStretch( 1 );

	applyFilter();
}



void AddOnStoreWidget::applyFilter()
{
	const auto searchText = m_searchEdit->text().trimmed().toLower();
	const auto category = m_categoryComboBox->currentData().toInt();
	const auto showSystemComponents = m_showSystemComponentsCheckBox->isChecked();

	const auto matches = [&]( const CardInfo& card ) {
		return ( searchText.isEmpty() || card.searchText.contains( searchText ) ) &&
				( category < 0 || ( card.kind == AddOnManager::Kind::AddOn && int(card.category) == category ) ) &&
				( card.kind != AddOnManager::Kind::SystemComponent || showSystemComponents );
	};

	QMap<int, int> visibleCardsPerKind;
	for( const auto& card : std::as_const(m_cards) )
	{
		const auto visible = matches( card );
		card.widget->setVisible( visible );
		if( visible )
		{
			visibleCardsPerKind[int(card.kind)]++;
		}
	}

	for( auto header : std::as_const(m_sectionHeaders) )
	{
		header->setVisible( visibleCardsPerKind.value( header->property( "kind" ).toInt() ) > 0 );
	}

	for( const auto& card : std::as_const(m_catalogCards) )
	{
		card.widget->setVisible( matches( card ) );
	}
}



void AddOnStoreWidget::updateFooter()
{
	const auto restartRequired = m_changesPending ||
								 std::any_of( m_addOns.cbegin(), m_addOns.cend(),
											  []( const AddOnManager::AddOn& addOn ) { return addOn.restartRequired(); } );

	QString text;
	if( restartRequired )
	{
		text = m_mode == Mode::Master ?
				   tr( "<b>Restart required:</b> changes take effect after Veyon Master has been restarted." ) :
				   tr( "<b>Restart required:</b> changes take effect after applying the configuration and "
					   "restarting the Veyon Service and Veyon Master." );
	}
	text += QStringLiteral("<br>") +
			tr( "Add-ons which control student computers have to be installed and enabled on the student "
				"computers as well (e.g. by distributing the Veyon configuration)." );

	m_footerLabel->setText( text );

	if( m_restartButton )
	{
		m_restartButton->setVisible( restartRequired );
	}
}



QWidget* AddOnStoreWidget::createCard( const AddOnManager::AddOn& addOn )
{
	auto card = createCardFrame();
	auto layout = new QHBoxLayout( card );
	layout->setContentsMargins( 12, 10, 12, 10 );
	layout->setSpacing( 12 );

	auto iconLabel = new QLabel;
	iconLabel->setPixmap( addOnIcon( addOn ).pixmap( 52, 52 ) );
	iconLabel->setAlignment( Qt::AlignTop );
	layout->addWidget( iconLabel, 0, Qt::AlignTop );

	auto textLayout = new QVBoxLayout;
	textLayout->setSpacing( 3 );

	auto titleLayout = new QHBoxLayout;
	titleLayout->setSpacing( 6 );
	auto nameLabel = new QLabel( addOn.name );
	auto nameFont = nameLabel->font();
	nameFont.setBold( true );
	nameFont.setPointSizeF( nameFont.pointSizeF() * 1.15 );
	nameLabel->setFont( nameFont );
	titleLayout->addWidget( nameLabel );

	switch( addOn.origin )
	{
	case AddOnManager::Origin::Bundled:
		if( addOn.kind == AddOnManager::Kind::AddOn )
		{
			titleLayout->addWidget( createBadge( tr( "Included" ), QColor( 0x19, 0x8c, 0xb3 ) ) );
		}
		break;
	case AddOnManager::Origin::Installed:
		titleLayout->addWidget( createBadge( tr( "Installed" ), QColor( 0x3a, 0x9a, 0x4a ) ) );
		break;
	case AddOnManager::Origin::UserInstalled:
		titleLayout->addWidget( createBadge( tr( "My add-on" ), QColor( 0x7a, 0x50, 0xc8 ) ) );
		break;
	}

	if( addOn.packageType == QLatin1String("script") )
	{
		titleLayout->addWidget( createBadge( tr( "Script" ), QColor( 0x7a, 0x50, 0xc8 ) ) );
	}

	if( addOn.restartRequired() )
	{
		titleLayout->addWidget( createBadge( tr( "Restart required" ), QColor( 0xd0, 0x70, 0x10 ) ) );
	}

	titleLayout->addStretch( 1 );
	textLayout->addLayout( titleLayout );

	QStringList metaParts;
	if( addOn.vendor.isEmpty() == false )
	{
		metaParts.append( addOn.vendor );
	}
	if( addOn.version.isEmpty() == false )
	{
		metaParts.append( tr( "Version %1" ).arg( addOn.version ) );
	}
	if( addOn.kind == AddOnManager::Kind::AddOn )
	{
		metaParts.append( AddOnManager::categoryDisplayName( addOn.category ) );
	}
	textLayout->addWidget( createSecondaryLabel( metaParts.join( QStringLiteral(" · ") ), 0.88 ) );

	if( addOn.description.isEmpty() == false )
	{
		auto descriptionLabel = new QLabel( addOn.description );
		descriptionLabel->setWordWrap( true );
		textLayout->addWidget( descriptionLabel );
	}

	if( addOn.summary.isEmpty() == false )
	{
		textLayout->addWidget( createSecondaryLabel( addOn.summary ) );
	}

	if( addOn.kind == AddOnManager::Kind::AddOn && addOn.clientPlatforms.isEmpty() == false )
	{
		QStringList platforms;
		for( const auto& platform : addOn.clientPlatforms )
		{
			platforms.append( platformDisplayName( platform ) );
		}
		platforms.removeDuplicates();
		textLayout->addWidget( createSecondaryLabel( tr( "Student computers: %1" ).arg( platforms.join( QStringLiteral(", ") ) ), 0.85 ) );
	}

	layout->addLayout( textLayout, 1 );

	// controls
	auto controlsLayout = new QVBoxLayout;
	controlsLayout->setSpacing( 6 );

	auto toggleLayout = new QHBoxLayout;
	toggleLayout->setSpacing( 6 );
	auto stateLabel = createSecondaryLabel( addOn.enabled ? tr( "Enabled" ) : tr( "Disabled" ), 0.88 );
	stateLabel->setWordWrap( false );
	toggleLayout->addWidget( stateLabel );
	auto toggle = new ToggleSwitch;
	toggle->setChecked( addOn.enabled );
	toggle->setEnabled( addOn.canBeDisabled );
	toggle->setToolTip( addOn.canBeDisabled ? ( addOn.enabled ? tr( "Click to disable this add-on" ) : tr( "Click to enable this add-on" ) )
											: tr( "This component is required and can't be disabled" ) );
	connect( toggle, &QAbstractButton::toggled, this, [this, addOn]( bool checked ) {
		// defer as the card (and thus the toggle) gets recreated
		QTimer::singleShot( 0, this, [this, addOn, checked]() { toggleAddOn( addOn, checked ); } );
	} );
	toggleLayout->addWidget( toggle );
	controlsLayout->addLayout( toggleLayout );

	const auto addButton = [&]( const QString& text, const std::function<void()>& handler ) {
		auto button = new QToolButton;
		button->setText( text );
		button->setToolButtonStyle( Qt::ToolButtonTextOnly );
		button->setAutoRaise( false );
		connect( button, &QToolButton::clicked, this, [this, handler]() { QTimer::singleShot( 0, this, handler ); } );
		controlsLayout->addWidget( button, 0, Qt::AlignRight );
	};

	if( addOn.packageType == QLatin1String("script") && addOn.removable )
	{
		addButton( tr( "Edit…" ), [this, addOn]() { editScriptAddOn( addOn ); } );
		addButton( tr( "Export…" ), [this, addOn]() { exportAddOn( addOn ); } );
	}

	if( addOn.removable )
	{
		addButton( tr( "Remove" ), [this, addOn]() { removeAddOn( addOn ); } );
	}

	controlsLayout->addStretch( 1 );
	layout->addLayout( controlsLayout );

	return card;
}



QWidget* AddOnStoreWidget::createCatalogCard( const QJsonObject& entry, const QUrl& indexUrl )
{
	auto card = createCardFrame();
	auto layout = new QHBoxLayout( card );
	layout->setContentsMargins( 12, 10, 12, 10 );
	layout->setSpacing( 12 );

	AddOnManager::AddOn pseudoAddOn;
	pseudoAddOn.packageType = entry[QStringLiteral("type")].toString();
	auto iconString = entry[QStringLiteral("icon")].toString();
	if( iconString.startsWith( QLatin1String("data:") ) )
	{
		iconString = iconString.mid( iconString.indexOf( QLatin1Char(',') ) + 1 );
	}
	pseudoAddOn.iconData = QByteArray::fromBase64( iconString.toLatin1() );

	auto iconLabel = new QLabel;
	iconLabel->setPixmap( addOnIcon( pseudoAddOn ).pixmap( 52, 52 ) );
	layout->addWidget( iconLabel, 0, Qt::AlignTop );

	auto textLayout = new QVBoxLayout;
	textLayout->setSpacing( 3 );

	auto nameLabel = new QLabel( entry[QStringLiteral("name")].toString() );
	auto nameFont = nameLabel->font();
	nameFont.setBold( true );
	nameFont.setPointSizeF( nameFont.pointSizeF() * 1.15 );
	nameLabel->setFont( nameFont );
	textLayout->addWidget( nameLabel );

	const auto version = entry[QStringLiteral("version")].toString();
	QStringList metaParts{ entry[QStringLiteral("vendor")].toString(), tr( "Version %1" ).arg( version ),
						   AddOnManager::categoryDisplayName( AddOnManager::categoryFromString( entry[QStringLiteral("category")].toString() ) ),
						   indexUrl.host().isEmpty() ? indexUrl.toDisplayString() : indexUrl.host() };
	metaParts.removeAll( QString() );
	textLayout->addWidget( createSecondaryLabel( metaParts.join( QStringLiteral(" · ") ), 0.88 ) );

	auto descriptionLabel = new QLabel( entry[QStringLiteral("description")].toString() );
	descriptionLabel->setWordWrap( true );
	textLayout->addWidget( descriptionLabel );
	layout->addLayout( textLayout, 1 );

	// determine installation state
	const Plugin::Uid uid{ entry[QStringLiteral("uid")].toString() };
	const auto installed = std::find_if( m_addOns.cbegin(), m_addOns.cend(),
										 [&]( const AddOnManager::AddOn& a ) {
		return a.uid == uid || ( a.id.isEmpty() == false && a.id == entry[QStringLiteral("id")].toString() );
	} );

	bool availableForPlatform = true;
	if( pseudoAddOn.packageType == QLatin1String("plugin") )
	{
		availableForPlatform = false;
		const auto platforms = entry[QStringLiteral("platforms")].toArray();
		for( const auto& key : AddOnManager::currentPlatformKeys() )
		{
			availableForPlatform |= platforms.contains( key );
		}
	}

	auto button = new QPushButton;
	if( availableForPlatform == false )
	{
		button->setText( tr( "Not available" ) );
		button->setEnabled( false );
		button->setToolTip( tr( "This add-on is not available for this platform." ) );
	}
	else if( installed != m_addOns.cend() )
	{
		const auto updateAvailable = QVersionNumber::fromString( version ) > QVersionNumber::fromString( installed->version );
		button->setText( updateAvailable ? tr( "Update" ) : tr( "Installed" ) );
		button->setEnabled( updateAvailable && installed->origin != AddOnManager::Origin::Bundled );
	}
	else
	{
		button->setText( tr( "Install" ) );
	}

	connect( button, &QPushButton::clicked, this, [this, entry, indexUrl]() { installFromCatalog( entry, indexUrl ); } );
	layout->addWidget( button, 0, Qt::AlignTop );

	return card;
}



QWidget* AddOnStoreWidget::createSectionHeader( const QString& title, const QString& subtitle )
{
	auto header = new QWidget;
	auto layout = new QVBoxLayout( header );
	layout->setContentsMargins( 2, 10, 2, 2 );
	layout->setSpacing( 0 );

	auto titleLabel = new QLabel( title );
	auto font = titleLabel->font();
	font.setBold( true );
	font.setPointSizeF( font.pointSizeF() * 1.1 );
	titleLabel->setFont( font );
	layout->addWidget( titleLabel );
	layout->addWidget( createSecondaryLabel( subtitle, 0.88 ) );

	return header;
}



void AddOnStoreWidget::toggleAddOn( const AddOnManager::AddOn& addOn, bool enabled )
{
	QString errorString;
	if( m_manager.setEnabled( addOn, enabled, m_mode == Mode::Master, &errorString ) == false )
	{
		QMessageBox::warning( this, tr( "Add-On Store" ), errorString.isEmpty() ? tr( "The operation failed." ) : errorString );
	}
	else if( m_mode == Mode::Configurator )
	{
		Q_EMIT configurationChanged();
	}

	rebuildInstalledList();
}



void AddOnStoreWidget::removeAddOn( const AddOnManager::AddOn& addOn )
{
	if( QMessageBox::question( this, tr( "Remove add-on" ),
							   tr( "Do you really want to remove the add-on \"%1\"?" ).arg( addOn.name ) ) != QMessageBox::Yes )
	{
		return;
	}

	QString errorString;
	if( m_manager.uninstall( addOn, &errorString ) == false )
	{
		QMessageBox::warning( this, tr( "Add-On Store" ), errorString );
	}

	refresh();
}



void AddOnStoreWidget::exportAddOn( const AddOnManager::AddOn& addOn )
{
	auto manifest = AddOnManager::readManifest( addOn.location );
	if( manifest.isEmpty() )
	{
		return;
	}

	if( addOn.iconData.isEmpty() == false )
	{
		manifest[QStringLiteral("icon")] = QString::fromLatin1( addOn.iconData.toBase64() );
	}

	auto fileName = addOn.name;
	fileName.replace( QRegularExpression( QStringLiteral("[^\\w\\-. ]") ), QStringLiteral("_") );

	const auto filePath = QFileDialog::getSaveFileName( this, tr( "Export add-on" ),
														VeyonCore::filesystem().userStandardLocation( QStandardPaths::DocumentsLocation ) +
														QDir::separator() + fileName + QLatin1Char('.') + AddOnManager::packageFileSuffix(),
														tr( "Veyon add-ons (*.%1)" ).arg( AddOnManager::packageFileSuffix() ) );
	if( filePath.isEmpty() )
	{
		return;
	}

	QString errorString;
	if( AddOnManager::writePackage( filePath, manifest, &errorString ) == false )
	{
		QMessageBox::warning( this, tr( "Add-On Store" ), errorString );
		return;
	}

	VeyonCore::filesystem().transferOwnershipToUser( filePath );
}



void AddOnStoreWidget::editScriptAddOn( const AddOnManager::AddOn& addOn )
{
	auto manifest = AddOnManager::readManifest( addOn.location );
	if( manifest.isEmpty() )
	{
		return;
	}

	if( addOn.iconData.isEmpty() == false )
	{
		manifest[QStringLiteral("icon")] = QString::fromLatin1( addOn.iconData.toBase64() );
	}

	ScriptAddOnEditorDialog dialog( manifest, this );
	if( dialog.exec() == QDialog::Accepted )
	{
		installScriptManifest( dialog.manifest(), addOn.origin == AddOnManager::Origin::UserInstalled ?
								   AddOnManager::InstallScope::CurrentUser : AddOnManager::InstallScope::System );
	}
}



void AddOnStoreWidget::createScriptAddOn()
{
	ScriptAddOnEditorDialog dialog( ScriptAddOnEditorDialog::createDefaultManifest(), this );
	if( dialog.exec() != QDialog::Accepted )
	{
		return;
	}

	auto scope = AddOnManager::InstallScope::CurrentUser;
	if( canInstallSystemWide() )
	{
		QMessageBox scopeQuestion( QMessageBox::Question, tr( "Install add-on" ),
								   tr( "Who should be able to use the add-on \"%1\" on this computer?" ).
								   arg( dialog.manifest()[QStringLiteral("name")].toString() ) );
		auto meButton = scopeQuestion.addButton( tr( "Only me" ), QMessageBox::AcceptRole );
		scopeQuestion.addButton( tr( "All users" ), QMessageBox::AcceptRole );
		scopeQuestion.addButton( QMessageBox::Cancel );
		if( VeyonCore::config().userAddOnsEnabled() == false )
		{
			meButton->setEnabled( false );
		}
		scopeQuestion.exec();
		if( scopeQuestion.clickedButton() == scopeQuestion.button( QMessageBox::Cancel ) )
		{
			return;
		}
		scope = scopeQuestion.clickedButton() == meButton ? AddOnManager::InstallScope::CurrentUser
														  : AddOnManager::InstallScope::System;
	}

	installScriptManifest( dialog.manifest(), scope );
}



void AddOnStoreWidget::installFromFile()
{
	const auto filePath = QFileDialog::getOpenFileName( this, tr( "Install add-on" ),
														VeyonCore::filesystem().userStandardLocation( QStandardPaths::DownloadLocation ),
														tr( "Veyon add-ons (*.%1);;All files (*)" ).arg( AddOnManager::packageFileSuffix() ) );
	if( filePath.isEmpty() == false )
	{
		installPackageFile( filePath, true );
	}
}



bool AddOnStoreWidget::installPackageFile( const QString& filePath, bool askForConfirmation )
{
	const auto package = AddOnManager::readPackage( filePath );
	if( package.isValid() == false )
	{
		QMessageBox::warning( this, tr( "Install add-on" ), package.errorString );
		return false;
	}

	const auto& manifest = package.manifest;
	const auto isScript = manifest[QStringLiteral("type")].toString() == QLatin1String("script");

	auto scope = AddOnManager::InstallScope::System;

	if( askForConfirmation )
	{
		const auto details = tr( "<p><b>%1</b> %2<br>by %3</p><p>%4</p>" ).
							 arg( manifest[QStringLiteral("name")].toString().toHtmlEscaped(),
								  manifest[QStringLiteral("version")].toString().toHtmlEscaped(),
								  manifest[QStringLiteral("vendor")].toString().toHtmlEscaped(),
								  manifest[QStringLiteral("description")].toString().toHtmlEscaped() );
		const auto warning = isScript ?
								 tr( "This add-on runs scripts on student computers. Only install add-ons from sources you trust." ) :
								 tr( "This add-on contains program code which runs with full privileges on this computer. "
									 "Only install add-ons from sources you trust." );

		QMessageBox confirmation( QMessageBox::Question, tr( "Install add-on" ),
								  details + QStringLiteral("<p><i>%1</i></p>").arg( warning ) );
		confirmation.setTextFormat( Qt::RichText );
		QAbstractButton* meButton = nullptr;
		QAbstractButton* allButton = nullptr;
		if( isScript )
		{
			meButton = confirmation.addButton( tr( "Install for me" ), QMessageBox::AcceptRole );
			meButton->setEnabled( VeyonCore::config().userAddOnsEnabled() );
			if( canInstallSystemWide() )
			{
				allButton = confirmation.addButton( tr( "Install for all users" ), QMessageBox::AcceptRole );
			}
		}
		else
		{
			allButton = confirmation.addButton( tr( "Install" ), QMessageBox::AcceptRole );
			allButton->setEnabled( canInstallSystemWide() );
		}
		confirmation.addButton( QMessageBox::Cancel );
		confirmation.exec();

		if( confirmation.clickedButton() == nullptr ||
			( confirmation.clickedButton() != meButton && confirmation.clickedButton() != allButton ) )
		{
			return false;
		}
		scope = confirmation.clickedButton() == meButton ? AddOnManager::InstallScope::CurrentUser
														 : AddOnManager::InstallScope::System;
	}

	QString errorString;
	QApplication::setOverrideCursor( Qt::WaitCursor );
	const auto success = m_manager.installPackage( filePath, scope, &errorString );
	QApplication::restoreOverrideCursor();

	if( success == false )
	{
		QMessageBox::warning( this, tr( "Install add-on" ), errorString.isEmpty() ? tr( "The operation failed." ) : errorString );
	}

	m_changesPending |= success;
	refresh();

	return success;
}



bool AddOnStoreWidget::installScriptManifest( const QJsonObject& manifest, AddOnManager::InstallScope scope )
{
	const auto tempFilePath = QDir::toNativeSeparators( QDir::tempPath() + QDir::separator() +
														QStringLiteral("veyon-addon-%1.%2").
														arg( QUuid::createUuid().toString( QUuid::WithoutBraces ),
															 AddOnManager::packageFileSuffix() ) );

	QString errorString;
	bool success = AddOnManager::writePackage( tempFilePath, manifest, &errorString ) &&
				   m_manager.installPackage( tempFilePath, scope, &errorString );

	QFile::remove( tempFilePath );

	if( success == false )
	{
		QMessageBox::warning( this, tr( "Add-On Store" ), errorString.isEmpty() ? tr( "The operation failed." ) : errorString );
	}

	m_changesPending |= success;
	refresh();

	return success;
}



void AddOnStoreWidget::fetchCatalog()
{
	if( m_pendingCatalogRequests > 0 )
	{
		return;
	}

	m_catalogEntries.clear();

	const auto repositories = VeyonCore::config().addOnRepositories();
	for( const auto& repository : repositories )
	{
		auto url = QUrl::fromUserInput( repository.trimmed() );
		if( url.isValid() == false )
		{
			continue;
		}
		if( url.path().endsWith( QLatin1String(".json") ) == false )
		{
			url.setPath( url.path() + ( url.path().endsWith( QLatin1Char('/') ) ? QString() : QStringLiteral("/") ) +
						 QStringLiteral("index.json") );
		}

		++m_pendingCatalogRequests;

		auto reply = m_networkAccessManager->get( QNetworkRequest( url ) );
		connect( reply, &QNetworkReply::finished, this, [this, reply]() {
			reply->deleteLater();
			--m_pendingCatalogRequests;

			if( reply->error() == QNetworkReply::NoError )
			{
				const auto index = QJsonDocument::fromJson( reply->readAll() ).object();
				for( const auto& entry : index[QStringLiteral("addOns")].toArray() )
				{
					const auto entryObject = entry.toObject();
					if( AddOnManager::isValidPackageId( entryObject[QStringLiteral("id")].toString() ) &&
						entryObject[QStringLiteral("package")].toString().isEmpty() == false )
					{
						m_catalogEntries.append( { entryObject, reply->url() } );
					}
				}
			}
			else
			{
				vWarning() << "failed to fetch add-on repository index" << reply->url() << reply->errorString();
			}

			if( m_pendingCatalogRequests == 0 )
			{
				rebuildCatalogList();
			}
		} );
	}

	rebuildCatalogList();
}



void AddOnStoreWidget::manageRepositories()
{
	bool ok = false;
	const auto text = QInputDialog::getMultiLineText( this, tr( "Add-on repositories" ),
													  tr( "Please enter the URLs of add-on repositories (one per line). "
														  "Both web servers (https://…) and network shares (file://…) are supported." ),
													  VeyonCore::config().addOnRepositories().join( QLatin1Char('\n') ), &ok );
	if( ok == false )
	{
		return;
	}

	QStringList repositories;
	for( const auto& line : text.split( QLatin1Char('\n') ) )
	{
		if( line.trimmed().isEmpty() == false )
		{
			repositories.append( line.trimmed() );
		}
	}

	if( m_mode == Mode::Configurator || VeyonCore::config().isStoreWritable() )
	{
		VeyonCore::config().setAddOnRepositories( repositories );
		if( m_mode == Mode::Master )
		{
			VeyonCore::config().flushStore();
		}
		else
		{
			Q_EMIT configurationChanged();
		}
	}
	else
	{
		QString errorString;
		if( AddOnManager::runCliElevated( QStringList{ QStringLiteral("addons"), QStringLiteral("repositories"), QStringLiteral("set") } + repositories,
										  &errorString ) == false )
		{
			QMessageBox::warning( this, tr( "Add-On Store" ), errorString );
		}
		VeyonCore::config().reloadFromStore();
	}

	fetchCatalog();
}



void AddOnStoreWidget::installFromCatalog( const QJsonObject& entry, const QUrl& indexUrl )
{
	const auto packageUrl = indexUrl.resolved( QUrl( entry[QStringLiteral("package")].toString() ) );

	QApplication::setOverrideCursor( Qt::WaitCursor );

	auto reply = m_networkAccessManager->get( QNetworkRequest( packageUrl ) );
	connect( reply, &QNetworkReply::finished, this, [this, reply, entry]() {
		reply->deleteLater();
		QApplication::restoreOverrideCursor();

		if( reply->error() != QNetworkReply::NoError )
		{
			QMessageBox::warning( this, tr( "Install add-on" ),
								  tr( "Could not download add-on: %1" ).arg( reply->errorString() ) );
			return;
		}

		const auto data = reply->readAll();
		const auto expectedChecksum = entry[QStringLiteral("sha256")].toString().toLower();
		if( expectedChecksum.isEmpty() == false &&
			QString::fromLatin1( QCryptographicHash::hash( data, QCryptographicHash::Sha256 ).toHex() ) != expectedChecksum )
		{
			QMessageBox::warning( this, tr( "Install add-on" ),
								  tr( "The downloaded add-on package is damaged (checksum mismatch)." ) );
			return;
		}

		const auto tempFilePath = QDir::toNativeSeparators( QDir::tempPath() + QDir::separator() +
															QStringLiteral("veyon-addon-%1.%2").
															arg( QUuid::createUuid().toString( QUuid::WithoutBraces ),
																 AddOnManager::packageFileSuffix() ) );
		QFile tempFile( tempFilePath );
		if( tempFile.open( QFile::WriteOnly ) == false || tempFile.write( data ) != data.size() )
		{
			QMessageBox::warning( this, tr( "Install add-on" ), tr( "Could not write file %1." ).arg( tempFilePath ) );
			return;
		}
		tempFile.close();

		installPackageFile( tempFilePath, true );
		QFile::remove( tempFilePath );
	} );
}



void AddOnStoreWidget::restartMaster()
{
	QApplication::closeAllWindows();

	const auto topLevelWidgets = QApplication::topLevelWidgets();
	for( const auto* widget : topLevelWidgets )
	{
		if( widget->isVisible() && qobject_cast<const QMenu *>( widget ) == nullptr )
		{
			// some window refused to close (e.g. because a feature is still active)
			return;
		}
	}

	QProcess::startDetached( QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid( 1 ) );
	QCoreApplication::quit();
}



bool AddOnStoreWidget::canInstallSystemWide() const
{
	return m_mode == Mode::Configurator ||
			VeyonCore::config().addOnStoreForTeachersEnabled() ||
			VeyonCore::platform().coreFunctions().isRunningAsAdmin();
}
