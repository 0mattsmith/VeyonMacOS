/*
 * ScriptAddOnEditorDialog.cpp - implementation of ScriptAddOnEditorDialog class
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

#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QStandardPaths>
#include <QTabWidget>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

#include "AddOnManager.h"
#include "Filesystem.h"
#include "PlatformUserFunctions.h"
#include "ScriptAddOnEditorDialog.h"
#include "VeyonCore.h"


namespace {

struct Example
{
	const char* name;
	const char* description;
	bool showOutput;
	bool input;
	const char* inputLabel;
	const char* windowsScript;
	const char* macosScript;
	const char* linuxScript;
};

// clazy:excludeall=non-pod-global-static
static const Example examples[] = {
	{ QT_TRANSLATE_NOOP("ScriptAddOnEditorDialog", "Show network configuration"),
	  QT_TRANSLATE_NOOP("ScriptAddOnEditorDialog", "Displays the IP configuration of the student computers"),
	  true, false, "",
	  "ipconfig /all",
	  "ifconfig",
	  "ip address" },
	{ QT_TRANSLATE_NOOP("ScriptAddOnEditorDialog", "Empty recycle bin"),
	  QT_TRANSLATE_NOOP("ScriptAddOnEditorDialog", "Empties the recycle bin of the logged on student"),
	  false, false, "",
	  "Clear-RecycleBin -Force -ErrorAction SilentlyContinue",
	  "rm -rf ~/.Trash/* ~/.Trash/.[!.]* 2>/dev/null; exit 0",
	  "rm -rf ~/.local/share/Trash/files/* ~/.local/share/Trash/info/* ; exit 0" },
	{ QT_TRANSLATE_NOOP("ScriptAddOnEditorDialog", "Close web browsers"),
	  QT_TRANSLATE_NOOP("ScriptAddOnEditorDialog", "Closes all common web browsers"),
	  false, false, "",
	  "Stop-Process -Name chrome,msedge,firefox,brave,opera -ErrorAction SilentlyContinue",
	  "for app in Safari \"Google Chrome\" Firefox \"Microsoft Edge\"; do osascript -e \"quit app \\\"$app\\\"\"; done; exit 0",
	  "pkill -f firefox; pkill -f chrome; pkill -f chromium; exit 0" },
	{ QT_TRANSLATE_NOOP("ScriptAddOnEditorDialog", "Open document from network share"),
	  QT_TRANSLATE_NOOP("ScriptAddOnEditorDialog", "Opens the file or folder entered by the teacher"),
	  false, true, QT_TRANSLATE_NOOP("ScriptAddOnEditorDialog", "Path of file or folder"),
	  "Start-Process -FilePath $env:VEYON_INPUT",
	  "open \"$VEYON_INPUT\"",
	  "xdg-open \"$VEYON_INPUT\"" },
	{ QT_TRANSLATE_NOOP("ScriptAddOnEditorDialog", "Show free disk space"),
	  QT_TRANSLATE_NOOP("ScriptAddOnEditorDialog", "Displays the free space of all drives"),
	  true, false, "",
	  "Get-PSDrive -PSProvider FileSystem | Format-Table -AutoSize Name,@{n='Free (GB)';e={[math]::Round($_.Free/1GB,1)}},@{n='Used (GB)';e={[math]::Round($_.Used/1GB,1)}}",
	  "df -h /",
	  "df -h" },
};

QFont monospaceFont()
{
	return QFontDatabase::systemFont( QFontDatabase::FixedFont );
}

}



ScriptAddOnEditorDialog::ScriptAddOnEditorDialog( const QJsonObject& manifest, QWidget* parent ) :
	QDialog( parent ),
	m_manifest( manifest ),
	m_actions( manifest[QStringLiteral("actions")].toArray() ),
	m_iconData( QByteArray::fromBase64( manifest[QStringLiteral("icon")].toString().toLatin1() ) )
{
	setWindowTitle( tr( "Script add-on editor" ) );
	setWindowIcon( VeyonCore::icon( QStringLiteral("veyon-addon-script") ) );
	resize( 980, 700 );

	auto mainLayout = new QVBoxLayout( this );

	auto introLabel = new QLabel( tr( "Script add-ons add buttons to Veyon Master which run scripts on the selected "
									  "student computers. Provide a script for every operating system used by your "
									  "students. The text entered by the teacher (if enabled) is available in the "
									  "environment variable <tt>VEYON_INPUT</tt>." ) );
	introLabel->setWordWrap( true );
	mainLayout->addWidget( introLabel );

	// general information
	auto generalGroup = new QGroupBox( tr( "Add-on" ) );
	auto generalLayout = new QHBoxLayout( generalGroup );

	m_iconButton = new QPushButton;
	m_iconButton->setIconSize( QSize( 48, 48 ) );
	m_iconButton->setFixedSize( 64, 64 );
	m_iconButton->setToolTip( tr( "Choose an icon (PNG file)" ) );
	connect( m_iconButton, &QPushButton::clicked, this, &ScriptAddOnEditorDialog::chooseIcon );
	generalLayout->addWidget( m_iconButton, 0, Qt::AlignTop );

	auto generalForm = new QFormLayout;
	m_nameEdit = new QLineEdit( manifest[QStringLiteral("name")].toString() );
	m_descriptionEdit = new QLineEdit( manifest[QStringLiteral("description")].toString() );
	m_vendorEdit = new QLineEdit( manifest[QStringLiteral("vendor")].toString() );
	m_versionEdit = new QLineEdit( manifest[QStringLiteral("version")].toString() );
	m_versionEdit->setMaximumWidth( 120 );
	generalForm->addRow( tr( "Name" ), m_nameEdit );
	generalForm->addRow( tr( "Description" ), m_descriptionEdit );
	auto vendorVersionLayout = new QHBoxLayout;
	vendorVersionLayout->addWidget( m_vendorEdit, 1 );
	vendorVersionLayout->addWidget( new QLabel( tr( "Version" ) ) );
	vendorVersionLayout->addWidget( m_versionEdit );
	generalForm->addRow( tr( "Author" ), vendorVersionLayout );
	generalLayout->addLayout( generalForm, 1 );
	mainLayout->addWidget( generalGroup );

	// actions
	auto splitter = new QSplitter( Qt::Horizontal );

	auto listPanel = new QWidget;
	auto listLayout = new QVBoxLayout( listPanel );
	listLayout->setContentsMargins( 0, 0, 0, 0 );
	listLayout->addWidget( new QLabel( tr( "Actions" ) ) );
	m_actionList = new QListWidget;
	listLayout->addWidget( m_actionList, 1 );

	auto listButtons = new QHBoxLayout;
	const auto addListButton = [&]( const QString& text, const QString& toolTip, const std::function<void()>& handler ) {
		auto button = new QToolButton;
		button->setText( text );
		button->setToolTip( toolTip );
		connect( button, &QToolButton::clicked, this, handler );
		listButtons->addWidget( button );
		return button;
	};
	addListButton( QStringLiteral("+"), tr( "Add action" ), [this]() { addAction(); } );
	addListButton( QStringLiteral("−"), tr( "Remove action" ), [this]() { removeAction(); } );
	addListButton( QStringLiteral("⧉"), tr( "Duplicate action" ), [this]() { duplicateAction(); } );
	addListButton( QStringLiteral("↑"), tr( "Move up" ), [this]() { moveAction( -1 ); } );
	addListButton( QStringLiteral("↓"), tr( "Move down" ), [this]() { moveAction( 1 ); } );

	auto examplesButton = new QToolButton;
	examplesButton->setText( tr( "Examples" ) );
	examplesButton->setPopupMode( QToolButton::InstantPopup );
	auto examplesMenu = new QMenu( examplesButton );
	for( int i = 0; i < int(std::size(examples)); ++i )
	{
		examplesMenu->addAction( tr( examples[i].name ), this, [this, i]() { insertExample( i ); } );
	}
	examplesButton->setMenu( examplesMenu );
	listButtons->addWidget( examplesButton );
	listButtons->addStretch( 1 );
	listLayout->addLayout( listButtons );

	splitter->addWidget( listPanel );

	m_actionEditor = new QWidget;
	auto editorLayout = new QVBoxLayout( m_actionEditor );
	editorLayout->setContentsMargins( 6, 0, 0, 0 );
	auto actionForm = new QFormLayout;
	m_actionNameEdit = new QLineEdit;
	m_actionDescriptionEdit = new QLineEdit;
	m_runAsComboBox = new QComboBox;
	m_runAsComboBox->addItem( tr( "Logged on student (recommended)" ), QStringLiteral("user") );
	m_runAsComboBox->addItem( tr( "System / administrator (must be allowed on student computers)" ), QStringLiteral("system") );
	actionForm->addRow( tr( "Button text" ), m_actionNameEdit );
	actionForm->addRow( tr( "Tooltip" ), m_actionDescriptionEdit );
	actionForm->addRow( tr( "Run as" ), m_runAsComboBox );

	m_confirmCheckBox = new QCheckBox( tr( "Ask for confirmation before running" ) );
	m_showOutputCheckBox = new QCheckBox( tr( "Show output of the script in Veyon Master" ) );
	auto inputLayout = new QHBoxLayout;
	m_inputCheckBox = new QCheckBox( tr( "Ask teacher for input:" ) );
	m_inputLabelEdit = new QLineEdit;
	m_inputLabelEdit->setPlaceholderText( tr( "Question shown to the teacher" ) );
	connect( m_inputCheckBox, &QCheckBox::toggled, m_inputLabelEdit, &QLineEdit::setEnabled );
	inputLayout->addWidget( m_inputCheckBox );
	inputLayout->addWidget( m_inputLabelEdit, 1 );
	actionForm->addRow( QString(), m_confirmCheckBox );
	actionForm->addRow( QString(), m_showOutputCheckBox );
	actionForm->addRow( QString(), inputLayout );
	editorLayout->addLayout( actionForm );

	auto platformTabs = new QTabWidget;
	const QMap<QString, QString> platformNames{
		{ QStringLiteral("windows"), QStringLiteral("Windows") },
		{ QStringLiteral("macos"), QStringLiteral("macOS") },
		{ QStringLiteral("linux"), QStringLiteral("Linux") },
	};
	for( const auto& platform : platforms() )
	{
		auto page = new QWidget;
		auto pageLayout = new QVBoxLayout( page );
		auto interpreterLayout = new QHBoxLayout;
		interpreterLayout->addWidget( new QLabel( tr( "Interpreter" ) ) );
		PlatformEditor editor;
		editor.interpreter = new QComboBox;
		editor.interpreter->addItems( interpreters( platform ) );
		interpreterLayout->addWidget( editor.interpreter );
		interpreterLayout->addStretch( 1 );
		pageLayout->addLayout( interpreterLayout );
		editor.code = new QPlainTextEdit;
		editor.code->setFont( monospaceFont() );
		editor.code->setLineWrapMode( QPlainTextEdit::NoWrap );
		editor.code->setPlaceholderText( tr( "Leave empty if this action is not available on %1" ).arg( platformNames.value( platform ) ) );
		pageLayout->addWidget( editor.code, 1 );
		m_platformEditors[platform] = editor;
		platformTabs->addTab( page, platformNames.value( platform ) );
	}
	editorLayout->addWidget( platformTabs, 1 );

	splitter->addWidget( m_actionEditor );
	splitter->setStretchFactor( 0, 1 );
	splitter->setStretchFactor( 1, 3 );
	mainLayout->addWidget( splitter, 1 );

	connect( m_actionNameEdit, &QLineEdit::textEdited, this, [this]( const QString& text ) {
		if( auto item = m_actionList->currentItem() )
		{
			item->setText( text.isEmpty() ? tr( "(unnamed)" ) : text );
		}
	} );

	auto buttonBox = new QDialogButtonBox( QDialogButtonBox::Save | QDialogButtonBox::Cancel );
	auto exportButton = buttonBox->addButton( tr( "Export…" ), QDialogButtonBox::ActionRole );
	exportButton->setToolTip( tr( "Save the add-on as a file which can be shared with other teachers" ) );
	connect( exportButton, &QPushButton::clicked, this, &ScriptAddOnEditorDialog::exportPackage );
	connect( buttonBox, &QDialogButtonBox::accepted, this, &ScriptAddOnEditorDialog::accept );
	connect( buttonBox, &QDialogButtonBox::rejected, this, &ScriptAddOnEditorDialog::reject );
	mainLayout->addWidget( buttonBox );

	m_iconButton->setIcon( m_iconData.isEmpty() ? VeyonCore::icon( QStringLiteral("veyon-addon-script") )
												: QIcon( QPixmap::fromImage( QImage::fromData( m_iconData ) ) ) );

	connect( m_actionList, &QListWidget::currentRowChanged, this, [this]( int row ) {
		storeCurrentAction();
		loadAction( row );
	} );

	if( m_actions.isEmpty() )
	{
		m_actions.append( createAction( tr( "New action" ) ) );
	}

	updateActionList();
	m_actionList->setCurrentRow( 0 );
}



QJsonObject ScriptAddOnEditorDialog::manifest() const
{
	auto manifest = m_manifest;

	manifest[QStringLiteral("veyonAddOn")] = AddOnManager::PackageFormatVersion;
	manifest[QStringLiteral("type")] = QStringLiteral("script");
	manifest[QStringLiteral("name")] = m_nameEdit->text().trimmed();
	manifest[QStringLiteral("description")] = m_descriptionEdit->text().trimmed();
	manifest[QStringLiteral("vendor")] = m_vendorEdit->text().trimmed();
	manifest[QStringLiteral("version")] = m_versionEdit->text().trimmed().isEmpty() ? QStringLiteral("1.0")
																					: m_versionEdit->text().trimmed();
	manifest[QStringLiteral("category")] = AddOnManager::categoryToString( AddOnInterface::Category::Scripting );
	manifest[QStringLiteral("minimumVeyonVersion")] = QStringLiteral("4.11");
	manifest.remove( QStringLiteral("_location") );
	manifest.remove( QStringLiteral("_userPackage") );

	if( m_iconData.isEmpty() == false )
	{
		manifest[QStringLiteral("icon")] = QString::fromLatin1( m_iconData.toBase64() );
	}
	else
	{
		manifest.remove( QStringLiteral("icon") );
	}

	// const_cast-free copy of current action state
	auto actions = m_actions;
	if( m_currentAction >= 0 && m_currentAction < actions.size() )
	{
		auto action = actions[m_currentAction].toObject();
		action[QStringLiteral("name")] = m_actionNameEdit->text().trimmed();
		action[QStringLiteral("description")] = m_actionDescriptionEdit->text().trimmed();
		action[QStringLiteral("runAs")] = m_runAsComboBox->currentData().toString();
		action[QStringLiteral("confirm")] = m_confirmCheckBox->isChecked();
		action[QStringLiteral("showOutput")] = m_showOutputCheckBox->isChecked();
		action[QStringLiteral("input")] = QJsonObject{ { QStringLiteral("enabled"), m_inputCheckBox->isChecked() },
													   { QStringLiteral("label"), m_inputLabelEdit->text() } };
		QJsonObject scripts;
		for( auto it = m_platformEditors.constBegin(), end = m_platformEditors.constEnd(); it != end; ++it )
		{
			const auto code = it->code->toPlainText();
			if( code.trimmed().isEmpty() == false )
			{
				scripts[it.key()] = QJsonObject{ { QStringLiteral("interpreter"), it->interpreter->currentText() },
												 { QStringLiteral("code"), code } };
			}
		}
		action[QStringLiteral("scripts")] = scripts;
		actions[m_currentAction] = action;
	}

	manifest[QStringLiteral("actions")] = actions;

	QStringList clientPlatforms;
	for( const auto& action : std::as_const(actions) )
	{
		const auto scripts = action.toObject()[QStringLiteral("scripts")].toObject();
		for( const auto& platform : platforms() )
		{
			if( scripts.contains( platform ) && clientPlatforms.contains( platform ) == false )
			{
				clientPlatforms.append( platform );
			}
		}
	}
	manifest[QStringLiteral("clientPlatforms")] = QJsonArray::fromStringList( clientPlatforms );

	return manifest;
}



QJsonObject ScriptAddOnEditorDialog::createDefaultManifest()
{
	const auto userName = VeyonCore::platform().userFunctions().queryCurrentUserProperty( PlatformUserFunctions::UserProperty::LoginName );
	auto fullName = VeyonCore::platform().userFunctions().queryCurrentUserProperty( PlatformUserFunctions::UserProperty::FullName );
	if( fullName.isEmpty() )
	{
		fullName = userName;
	}

	auto idUserPart = userName.toLower();
	idUserPart.replace( QRegularExpression( QStringLiteral("[^a-z0-9]") ), QStringLiteral("-") );
	if( idUserPart.isEmpty() )
	{
		idUserPart = QStringLiteral("user");
	}

	return {
		{ QStringLiteral("veyonAddOn"), AddOnManager::PackageFormatVersion },
		{ QStringLiteral("type"), QStringLiteral("script") },
		{ QStringLiteral("id"), QStringLiteral("custom.%1.%2").arg( idUserPart,
																	 QUuid::createUuid().toString( QUuid::WithoutBraces ).left( 8 ) ) },
		{ QStringLiteral("uid"), QUuid::createUuid().toString( QUuid::WithoutBraces ) },
		{ QStringLiteral("name"), tr( "My add-on" ) },
		{ QStringLiteral("vendor"), fullName },
		{ QStringLiteral("version"), QStringLiteral("1.0") },
		{ QStringLiteral("actions"), QJsonArray{} },
	};
}



QJsonObject ScriptAddOnEditorDialog::createAction( const QString& name )
{
	return {
		{ QStringLiteral("uid"), QUuid::createUuid().toString( QUuid::WithoutBraces ) },
		{ QStringLiteral("name"), name },
		{ QStringLiteral("runAs"), QStringLiteral("user") },
		{ QStringLiteral("confirm"), false },
		{ QStringLiteral("showOutput"), false },
		{ QStringLiteral("scripts"), QJsonObject{} },
	};
}



QStringList ScriptAddOnEditorDialog::interpreters( const QString& platform )
{
	if( platform == QLatin1String("windows") )
	{
		return { QStringLiteral("powershell"), QStringLiteral("cmd") };
	}

	if( platform == QLatin1String("macos") )
	{
		return { QStringLiteral("zsh"), QStringLiteral("bash"), QStringLiteral("sh"), QStringLiteral("osascript") };
	}

	return { QStringLiteral("bash"), QStringLiteral("sh"), QStringLiteral("python3") };
}



void ScriptAddOnEditorDialog::accept()
{
	storeCurrentAction();

	QString errorString;
	if( validate( &errorString ) == false )
	{
		QMessageBox::warning( this, windowTitle(), errorString );
		return;
	}

	QDialog::accept();
}



void ScriptAddOnEditorDialog::loadAction( int index )
{
	m_currentAction = index;
	m_actionEditor->setEnabled( index >= 0 && index < m_actions.size() );
	if( m_actionEditor->isEnabled() == false )
	{
		return;
	}

	const auto action = m_actions[index].toObject();
	m_actionNameEdit->setText( action[QStringLiteral("name")].toString() );
	m_actionDescriptionEdit->setText( action[QStringLiteral("description")].toString() );
	m_runAsComboBox->setCurrentIndex( qMax( 0, m_runAsComboBox->findData( action[QStringLiteral("runAs")].toString() ) ) );
	m_confirmCheckBox->setChecked( action[QStringLiteral("confirm")].toBool() );
	m_showOutputCheckBox->setChecked( action[QStringLiteral("showOutput")].toBool() );
	const auto input = action[QStringLiteral("input")].toObject();
	m_inputCheckBox->setChecked( input[QStringLiteral("enabled")].toBool() );
	m_inputLabelEdit->setText( input[QStringLiteral("label")].toString() );
	m_inputLabelEdit->setEnabled( m_inputCheckBox->isChecked() );

	const auto scripts = action[QStringLiteral("scripts")].toObject();
	for( auto it = m_platformEditors.begin(), end = m_platformEditors.end(); it != end; ++it )
	{
		const auto script = scripts[it.key()].toObject();
		const auto interpreter = script[QStringLiteral("interpreter")].toString();
		it->interpreter->setCurrentIndex( qMax( 0, it->interpreter->findText( interpreter ) ) );
		it->code->setPlainText( script[QStringLiteral("code")].toString() );
	}
}



void ScriptAddOnEditorDialog::storeCurrentAction()
{
	if( m_currentAction < 0 || m_currentAction >= m_actions.size() )
	{
		return;
	}

	m_actions[m_currentAction] = manifest()[QStringLiteral("actions")].toArray()[m_currentAction];
}



void ScriptAddOnEditorDialog::updateActionList()
{
	const auto currentRow = m_actionList->currentRow();

	m_actionList->blockSignals( true );
	m_actionList->clear();
	for( const auto& action : std::as_const(m_actions) )
	{
		const auto name = action.toObject()[QStringLiteral("name")].toString();
		m_actionList->addItem( name.isEmpty() ? tr( "(unnamed)" ) : name );
	}
	m_actionList->blockSignals( false );

	if( currentRow >= 0 && currentRow < m_actionList->count() )
	{
		m_actionList->setCurrentRow( currentRow );
	}
}



void ScriptAddOnEditorDialog::addAction()
{
	storeCurrentAction();
	m_actions.append( createAction( tr( "New action" ) ) );
	m_currentAction = -1;
	updateActionList();
	m_actionList->setCurrentRow( int(m_actions.size()) - 1 );
}



void ScriptAddOnEditorDialog::duplicateAction()
{
	storeCurrentAction();
	if( m_currentAction < 0 )
	{
		return;
	}

	auto action = m_actions[m_currentAction].toObject();
	action[QStringLiteral("uid")] = QUuid::createUuid().toString( QUuid::WithoutBraces );
	action[QStringLiteral("name")] = tr( "%1 (copy)" ).arg( action[QStringLiteral("name")].toString() );
	m_actions.insert( m_currentAction + 1, action );
	const auto newRow = m_currentAction + 1;
	m_currentAction = -1;
	updateActionList();
	m_actionList->setCurrentRow( newRow );
}



void ScriptAddOnEditorDialog::removeAction()
{
	if( m_currentAction < 0 || m_actions.size() <= 1 )
	{
		return;
	}

	const auto row = m_currentAction;
	m_actions.removeAt( row );
	m_currentAction = -1;
	updateActionList();
	m_actionList->setCurrentRow( qMin( row, int(m_actions.size()) - 1 ) );
	loadAction( m_actionList->currentRow() );
}



void ScriptAddOnEditorDialog::moveAction( int offset )
{
	storeCurrentAction();
	const auto row = m_currentAction;
	const auto newRow = row + offset;
	if( row < 0 || newRow < 0 || newRow >= m_actions.size() )
	{
		return;
	}

	const auto action = m_actions[row];
	m_actions.removeAt( row );
	m_actions.insert( newRow, action );
	m_currentAction = -1;
	updateActionList();
	m_actionList->setCurrentRow( newRow );
	loadAction( newRow );
}



void ScriptAddOnEditorDialog::insertExample( int exampleIndex )
{
	if( exampleIndex < 0 || exampleIndex >= int(std::size(examples)) )
	{
		return;
	}

	const auto& example = examples[exampleIndex];

	storeCurrentAction();

	auto action = createAction( tr( example.name ) );
	action[QStringLiteral("description")] = tr( example.description );
	action[QStringLiteral("showOutput")] = example.showOutput;
	action[QStringLiteral("input")] = QJsonObject{ { QStringLiteral("enabled"), example.input },
												   { QStringLiteral("label"), tr( example.inputLabel ) } };
	action[QStringLiteral("scripts")] = QJsonObject{
		{ QStringLiteral("windows"), QJsonObject{ { QStringLiteral("interpreter"), QStringLiteral("powershell") },
												  { QStringLiteral("code"), QString::fromUtf8( example.windowsScript ) } } },
		{ QStringLiteral("macos"), QJsonObject{ { QStringLiteral("interpreter"), QStringLiteral("zsh") },
												{ QStringLiteral("code"), QString::fromUtf8( example.macosScript ) } } },
		{ QStringLiteral("linux"), QJsonObject{ { QStringLiteral("interpreter"), QStringLiteral("bash") },
												{ QStringLiteral("code"), QString::fromUtf8( example.linuxScript ) } } },
	};

	m_actions.append( action );
	m_currentAction = -1;
	updateActionList();
	m_actionList->setCurrentRow( int(m_actions.size()) - 1 );
}



void ScriptAddOnEditorDialog::chooseIcon()
{
	const auto fileName = QFileDialog::getOpenFileName( this, tr( "Choose icon" ),
														VeyonCore::filesystem().userStandardLocation( QStandardPaths::PicturesLocation ),
														tr( "Images (*.png *.jpg *.jpeg *.bmp)" ) );
	if( fileName.isEmpty() )
	{
		return;
	}

	QImage image( fileName );
	if( image.isNull() )
	{
		QMessageBox::warning( this, windowTitle(), tr( "Could not load image %1." ).arg( fileName ) );
		return;
	}

	image = image.scaled( 128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation );

	QBuffer buffer( &m_iconData );
	m_iconData.clear();
	buffer.open( QBuffer::WriteOnly );
	image.save( &buffer, "PNG" );

	m_iconButton->setIcon( QIcon( QPixmap::fromImage( image ) ) );
}



void ScriptAddOnEditorDialog::exportPackage()
{
	storeCurrentAction();

	QString errorString;
	if( validate( &errorString ) == false )
	{
		QMessageBox::warning( this, windowTitle(), errorString );
		return;
	}

	auto fileName = m_nameEdit->text().trimmed();
	fileName.replace( QRegularExpression( QStringLiteral("[^\\w\\-. ]") ), QStringLiteral("_") );

	const auto filePath = QFileDialog::getSaveFileName( this, tr( "Export add-on" ),
														VeyonCore::filesystem().userStandardLocation( QStandardPaths::DocumentsLocation ) +
														QDir::separator() + fileName + QLatin1Char('.') + AddOnManager::packageFileSuffix(),
														tr( "Veyon add-ons (*.%1)" ).arg( AddOnManager::packageFileSuffix() ) );
	if( filePath.isEmpty() )
	{
		return;
	}

	if( AddOnManager::writePackage( filePath, manifest(), &errorString ) == false )
	{
		QMessageBox::warning( this, windowTitle(), errorString );
		return;
	}

	VeyonCore::filesystem().transferOwnershipToUser( filePath );
}



bool ScriptAddOnEditorDialog::validate( QString* errorString ) const
{
	const auto currentManifest = manifest();

	if( currentManifest[QStringLiteral("name")].toString().isEmpty() )
	{
		*errorString = tr( "Please enter a name for the add-on." );
		return false;
	}

	const auto actions = currentManifest[QStringLiteral("actions")].toArray();
	if( actions.isEmpty() )
	{
		*errorString = tr( "Please add at least one action." );
		return false;
	}

	for( const auto& actionValue : actions )
	{
		const auto action = actionValue.toObject();
		if( action[QStringLiteral("name")].toString().isEmpty() )
		{
			*errorString = tr( "Please enter a button text for every action." );
			return false;
		}
		if( action[QStringLiteral("scripts")].toObject().isEmpty() )
		{
			*errorString = tr( "The action \"%1\" does not contain a script for any operating system." ).
						   arg( action[QStringLiteral("name")].toString() );
			return false;
		}
	}

	const auto package = AddOnManager::parsePackage( QJsonDocument( currentManifest ).toJson() );
	if( package.isValid() == false )
	{
		*errorString = package.errorString;
		return false;
	}

	return true;
}
