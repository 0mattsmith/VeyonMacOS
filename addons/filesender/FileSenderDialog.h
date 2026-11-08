/*
 * FileSenderDialog.h - declaration of FileSenderDialog class
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
#include <QElapsedTimer>
#include <QFile>
#include <QTimer>

#include "FileSenderPlugin.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class FileSenderDialog : public QDialog
{
	Q_OBJECT
public:
	FileSenderDialog( FileSenderPlugin& plugin, const ComputerControlInterfaceList& computerControlInterfaces,
					  QWidget* parent = nullptr );
	~FileSenderDialog() override;

	void setComputerControlInterfaces( const ComputerControlInterfaceList& computerControlInterfaces );

protected:
	void closeEvent( QCloseEvent* event ) override;

private:
	using Command = FileSenderPlugin::Command;
	using Argument = FileSenderPlugin::Argument;

	static constexpr auto ReplyTimeout = 60000;
	static constexpr auto MaximumFilesToOpen = 10;

	struct Job
	{
		QString localPath;
		QString relativePath;
		qint64 size{0};
	};

	struct ComputerState
	{
		ComputerControlInterface::Pointer computer;
		QTreeWidgetItem* item{nullptr};
		QProgressBar* progressBar{nullptr};
		int jobIndex{0};
		QFile file;
		QUuid transferId;
		QUuid pendingRequest;
		Command pendingCommand{Command::Result};
		qint64 bytesSent{0};
		QStringList filesToOpen;
		QStringList errors;
		bool active{false};
		QElapsedTimer lastActivity;
	};

	void addFiles();
	void addFolder();
	void addPaths( const QStringList& paths );
	void removeSelectedFiles();
	void updateSummary();

	void startSending();
	void cancelSending();
	void request( ComputerState* state, Command command, QMap<Argument, QVariant> arguments );
	void sendNextFile( ComputerState* state );
	void sendChunk( ComputerState* state );
	void finishComputer( ComputerState* state );
	void handleReply( ComputerControlInterface::Pointer computerControlInterface, const QVariantMap& arguments );
	void checkTimeouts();
	void updateComputerRow( ComputerState* state, const QString& status = {} );
	void updateSendingState();

	QMap<Argument, QVariant> destinationArguments() const;
	QList<Job> collectJobs() const;

	FileSenderPlugin& m_plugin;
	ComputerControlInterfaceList m_computerControlInterfaces;

	QStringList m_paths;
	QList<Job> m_jobs;
	qint64 m_totalSize{0};
	QList<ComputerState*> m_states;
	bool m_sending{false};
	QTimer m_timeoutTimer{this};

	QTreeWidget* m_fileTree{nullptr};
	QLabel* m_summaryLabel{nullptr};
	QComboBox* m_destinationComboBox{nullptr};
	QLineEdit* m_customPathEdit{nullptr};
	QLineEdit* m_subfolderEdit{nullptr};
	QComboBox* m_existingFilesComboBox{nullptr};
	QCheckBox* m_openFilesCheckBox{nullptr};
	QCheckBox* m_openFolderCheckBox{nullptr};
	QCheckBox* m_showMessageCheckBox{nullptr};
	QLineEdit* m_messageEdit{nullptr};
	QTreeWidget* m_computerTree{nullptr};
	QPushButton* m_sendButton{nullptr};
	QPushButton* m_cancelButton{nullptr};

};
