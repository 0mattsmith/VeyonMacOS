/*
 * ScriptAddOnEditorDialog.h - declaration of ScriptAddOnEditorDialog class
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

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>

#include "VeyonCore.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QWidget;

/*!
 * \brief Editor for script add-ons ("custom add-ons") which consist of one or more actions,
 * each running a script for Windows, macOS and/or Linux on the selected student computers
 */
class VEYON_CORE_EXPORT ScriptAddOnEditorDialog : public QDialog
{
	Q_OBJECT
public:
	explicit ScriptAddOnEditorDialog( const QJsonObject& manifest, QWidget* parent = nullptr );
	~ScriptAddOnEditorDialog() override = default;

	QJsonObject manifest() const;

	static QJsonObject createDefaultManifest();
	static QJsonObject createAction( const QString& name );

	static QStringList platforms()
	{
		return { QStringLiteral("windows"), QStringLiteral("macos"), QStringLiteral("linux") };
	}

	static QStringList interpreters( const QString& platform );

	void accept() override;

private:
	struct PlatformEditor
	{
		QComboBox* interpreter{nullptr};
		QPlainTextEdit* code{nullptr};
	};

	void loadAction( int index );
	void storeCurrentAction();
	void updateActionList();
	void addAction();
	void duplicateAction();
	void removeAction();
	void moveAction( int offset );
	void insertExample( int exampleIndex );
	void chooseIcon();
	void exportPackage();
	bool validate( QString* errorString ) const;

	QJsonObject m_manifest;
	QJsonArray m_actions;
	QByteArray m_iconData;
	int m_currentAction{-1};

	QLineEdit* m_nameEdit{nullptr};
	QLineEdit* m_descriptionEdit{nullptr};
	QLineEdit* m_vendorEdit{nullptr};
	QLineEdit* m_versionEdit{nullptr};
	QPushButton* m_iconButton{nullptr};

	QListWidget* m_actionList{nullptr};
	QWidget* m_actionEditor{nullptr};
	QLineEdit* m_actionNameEdit{nullptr};
	QLineEdit* m_actionDescriptionEdit{nullptr};
	QComboBox* m_runAsComboBox{nullptr};
	QCheckBox* m_confirmCheckBox{nullptr};
	QCheckBox* m_showOutputCheckBox{nullptr};
	QCheckBox* m_inputCheckBox{nullptr};
	QLineEdit* m_inputLabelEdit{nullptr};
	QMap<QString, PlatformEditor> m_platformEditors;

};
