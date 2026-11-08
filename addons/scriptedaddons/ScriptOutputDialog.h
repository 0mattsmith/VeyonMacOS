/*
 * ScriptOutputDialog.h - shows the results of a script add-on action
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
#include <QUuid>

#include "ComputerControlInterface.h"

class QPlainTextEdit;
class QTreeWidget;
class ScriptedAddOnsPlugin;

class ScriptOutputDialog : public QDialog
{
	Q_OBJECT
public:
	ScriptOutputDialog( ScriptedAddOnsPlugin& plugin, QUuid requestId, const QString& actionName,
						const ComputerControlInterfaceList& computerControlInterfaces, QWidget* parent = nullptr );
	~ScriptOutputDialog() override = default;

private:
	void handleResult( ComputerControlInterface::Pointer computerControlInterface, const QUuid& requestId,
					   int exitCode, const QString& output, const QString& error );
	void showSelectedOutput();

	const QUuid m_requestId;
	QHash<QString, QString> m_outputs;

	QTreeWidget* m_resultTree{nullptr};
	QPlainTextEdit* m_outputEdit{nullptr};

};
