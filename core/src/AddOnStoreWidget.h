/*
 * AddOnStoreWidget.h - declaration of AddOnStoreWidget class
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

#include <QJsonObject>
#include <QUrl>
#include <QWidget>

#include "AddOnManager.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QPushButton;
class QTabWidget;
class QVBoxLayout;

/*!
 * \brief The Add-On Store - lists, enables/disables, installs and removes add-ons
 *
 * The widget is used inside Veyon Master (as dialog) and Veyon Configurator (as configuration page).
 * In Master mode changes are written immediately (with privilege elevation if required) while in
 * Configurator mode changes to the configuration are applied together with all other settings.
 */
class VEYON_CORE_EXPORT AddOnStoreWidget : public QWidget
{
	Q_OBJECT
public:
	enum class Mode
	{
		Master,
		Configurator,
	};
	Q_ENUM(Mode)

	explicit AddOnStoreWidget( Mode mode, QWidget* parent = nullptr );
	~AddOnStoreWidget() override = default;

	void refresh();

	static QIcon addOnIcon( const AddOnManager::AddOn& addOn );

Q_SIGNALS:
	void configurationChanged();

private:
	struct CardInfo
	{
		QWidget* widget{nullptr};
		QString searchText;
		AddOnInterface::Category category{AddOnInterface::Category::Other};
		AddOnManager::Kind kind{AddOnManager::Kind::AddOn};
	};

	void rebuildInstalledList();
	void rebuildCatalogList();
	void applyFilter();
	void updateFooter();

	QWidget* createCard( const AddOnManager::AddOn& addOn );
	QWidget* createCatalogCard( const QJsonObject& entry, const QUrl& indexUrl );
	QWidget* createSectionHeader( const QString& title, const QString& subtitle );

	void toggleAddOn( const AddOnManager::AddOn& addOn, bool enabled );
	void removeAddOn( const AddOnManager::AddOn& addOn );
	void exportAddOn( const AddOnManager::AddOn& addOn );
	void editScriptAddOn( const AddOnManager::AddOn& addOn );
	void createScriptAddOn();
	void installFromFile();
	bool installPackageFile( const QString& filePath, bool askForConfirmation );
	bool installScriptManifest( const QJsonObject& manifest, AddOnManager::InstallScope scope );

	void fetchCatalog();
	void manageRepositories();
	void installFromCatalog( const QJsonObject& entry, const QUrl& indexUrl );

	void restartMaster();

	bool canInstallSystemWide() const;

	const Mode m_mode;
	AddOnManager m_manager{};
	AddOnManager::AddOnList m_addOns;

	QLineEdit* m_searchEdit{nullptr};
	QComboBox* m_categoryComboBox{nullptr};
	QCheckBox* m_showSystemComponentsCheckBox{nullptr};
	QTabWidget* m_tabWidget{nullptr};
	QVBoxLayout* m_installedLayout{nullptr};
	QVBoxLayout* m_catalogLayout{nullptr};
	QLabel* m_catalogStatusLabel{nullptr};
	QLabel* m_footerLabel{nullptr};
	QPushButton* m_restartButton{nullptr};

	QList<CardInfo> m_cards;
	QList<QWidget*> m_sectionHeaders;
	QList<CardInfo> m_catalogCards;
	QList<std::pair<QJsonObject, QUrl>> m_catalogEntries;
	int m_pendingCatalogRequests{0};

	QNetworkAccessManager* m_networkAccessManager{nullptr};

	bool m_changesPending{false};

};
