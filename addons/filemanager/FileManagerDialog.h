/*
 * FileManagerDialog.h - declaration of FileManagerDialog class
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
#include <QTimer>

#include "FileManagerPlugin.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTreeWidget;

class FileManagerDialog : public QDialog
{
	Q_OBJECT
public:
	FileManagerDialog( FileManagerPlugin& plugin, const ComputerControlInterfaceList& computerControlInterfaces,
					   QWidget* parent = nullptr );
	~FileManagerDialog() override;

	void setComputerControlInterfaces( const ComputerControlInterfaceList& computerControlInterfaces );

private:
	using Command = FileManagerPlugin::Command;
	using Argument = FileManagerPlugin::Argument;

	static constexpr auto RequestTimeout = 60000;

	enum class RequestPurpose
	{
		Roots,
		Listing,
		Operation,
		Transfer,
		RecursiveListing,
		Ignore,
	};

	struct Transfer
	{
		enum class Type
		{
			Upload,
			Download,
			ListForDownload,
		};

		Type type{Type::Upload};
		ComputerControlInterface::Pointer computer;
		QString localPath;
		QString remotePath;		// directory for uploads, file/folder for downloads
		QString name;
		qint64 size{0};
		qint64 transferred{0};
		bool overwrite{false};
		bool openLocally{false};
		QUuid transferId{QUuid::createUuid()};
	};

	void currentComputerChanged();
	ComputerControlInterface::Pointer currentComputer() const;

	void navigateTo( const QString& path, bool addToHistory = true );
	void navigateBack();
	void navigateUp();
	void refresh();

	void createFolder();
	void renameSelected();
	void deleteSelected();
	void openSelected();
	void uploadFiles();
	void uploadPaths( const QStringList& localPaths );
	void downloadSelected();
	void viewSelectedFile();
	void renderListing();

	QUuid sendRequest( Command command, const QMap<Argument, QVariant>& arguments, RequestPurpose purpose,
					   const ComputerControlInterface::Pointer& computer = {} );

	void handleReply( ComputerControlInterface::Pointer computerControlInterface, int command, const QVariantMap& arguments );
	void handleTransferReply( Command command, const QVariantMap& arguments );
	void handleRecursiveListing( Command command, const QVariantMap& arguments );
	void updateListing( const QString& path, const QVariantList& entries );
	void updatePlaces( const QVariantList& roots );

	void enqueueTransfer( const Transfer& transfer );
	void processNextTransfer();
	void continueUpload();
	void finishCurrentTransfer( const QString& error = {} );
	void cancelTransfers();
	void checkTransferTimeout();
	void updateTransferStatus();

	QStringList selectedRemotePaths( bool* containsFolders = nullptr ) const;
	QString remotePath( const QString& name ) const;

	FileManagerPlugin& m_plugin;
	ComputerControlInterfaceList m_computerControlInterfaces;

	QString m_currentPath;
	QVariantList m_currentEntries;
	QString m_pendingNavigationPath;
	bool m_pendingNavigationAddsHistory{false};
	QStringList m_history;
	bool m_readOnly{false};
	QString m_lastDownloadDirectory;

	QHash<QUuid, RequestPurpose> m_pendingRequests;

	QList<Transfer> m_transferQueue;
	Transfer m_currentTransfer;
	bool m_transferActive{false};
	QUuid m_currentTransferRequest;
	FileReceiver m_downloadReceiver;
	QFile m_uploadFile;
	QElapsedTimer m_lastTransferActivity;
	QTimer m_transferWatchdog{this};
	int m_completedTransfers{0};
	QStringList m_transferErrors;

	QComboBox* m_computerComboBox{nullptr};
	QLineEdit* m_pathEdit{nullptr};
	QListWidget* m_placesList{nullptr};
	QTreeWidget* m_fileTree{nullptr};
	QCheckBox* m_showHiddenCheckBox{nullptr};
	QLabel* m_statusLabel{nullptr};
	QProgressBar* m_progressBar{nullptr};
	QPushButton* m_cancelTransferButton{nullptr};
	QList<QWidget*> m_modifyingWidgets;

};
