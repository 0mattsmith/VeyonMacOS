/*
 * AddOnManager.h - declaration of AddOnManager class
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

#include "AddOnInterface.h"
#include "Plugin.h"

/*!
 * \brief Manages bundled plugins and installed add-on packages
 *
 * Add-on packages (*.veyonaddon) are JSON documents containing a manifest and either
 * the binaries of a compiled plugin for one or multiple platforms ("plugin" packages) or
 * a set of script actions ("script" packages, executed by the Scripted Add-Ons plugin).
 *
 * Installed packages are extracted into subdirectories of the system-wide add-ons directory
 * (or for script packages optionally into the add-ons directory of the current user).
 */
class VEYON_CORE_EXPORT AddOnManager : public QObject
{
	Q_OBJECT
public:
	enum class Kind
	{
		AddOn,
		BuiltinFeature,
		SystemComponent,
	};
	Q_ENUM(Kind)

	enum class Origin
	{
		Bundled,
		Installed,
		UserInstalled,
	};
	Q_ENUM(Origin)

	enum class InstallScope
	{
		System,
		CurrentUser,
	};
	Q_ENUM(InstallScope)

	struct AddOn
	{
		Plugin::Uid uid;
		QString id;
		QString name;
		QString description;
		QString summary;
		QString vendor;
		QString version;
		QString iconUrl;
		QByteArray iconData;
		AddOnInterface::Category category{AddOnInterface::Category::Other};
		Kind kind{Kind::AddOn};
		Origin origin{Origin::Bundled};
		QString packageType;
		QString location;
		QStringList clientPlatforms;
		bool enabled{true};
		bool loaded{false};
		bool canBeDisabled{true};
		bool removable{false};

		bool restartRequired() const
		{
			return enabled != loaded;
		}
	};
	using AddOnList = QList<AddOn>;

	struct Package
	{
		QJsonObject manifest;
		QString errorString;

		bool isValid() const
		{
			return errorString.isEmpty() && manifest.isEmpty() == false;
		}
	};

	static constexpr int PackageFormatVersion = 1;
	static constexpr qint64 MaximumPackageSize = 256 * 1024 * 1024;

	explicit AddOnManager( QObject* parent = nullptr );
	~AddOnManager() override = default;

	AddOnList addOns() const;

	bool setEnabled( const AddOn& addOn, bool enabled, bool saveConfiguration, QString* errorString = nullptr );
	bool setEnabled( Plugin::Uid uid, bool enabled, bool saveConfiguration, QString* errorString = nullptr );

	bool installPackage( const QString& packageFilePath, InstallScope scope, QString* errorString = nullptr );
	bool uninstall( const AddOn& addOn, QString* errorString = nullptr );

	bool canModifySystemAddOns() const;

	//! \brief Runs veyon-cli with administrative privileges, e.g. to modify the system configuration
	static bool runCliElevated( const QStringList& arguments, QString* errorString = nullptr );

	static Package readPackage( const QString& packageFilePath );
	static Package parsePackage( const QByteArray& packageData );
	static bool writePackage( const QString& packageFilePath, const QJsonObject& manifest, QString* errorString = nullptr );
	static bool addBinaryToPackage( QJsonObject& manifest, const QString& platformKey, const QString& binaryFilePath,
									QString* errorString = nullptr );

	static QString packageFileSuffix()
	{
		return QStringLiteral("veyonaddon");
	}

	static QString currentPlatform();
	static QStringList currentPlatformKeys();

	static QString systemAddOnsDirectory();
	static QString userAddOnsDirectory();

	//! \brief Returns manifests of all installed (and enabled) packages of the given type
	static QList<QJsonObject> installedManifests( const QString& packageType, bool includeUserPackages, bool includeDisabled = false );

	static QJsonObject readManifest( const QString& packageDirectory );

	static AddOnInterface::Category categoryFromString( const QString& category );
	static QString categoryToString( AddOnInterface::Category category );
	static QString categoryDisplayName( AddOnInterface::Category category );

	static bool isValidPackageId( const QString& id );

	static QString humanReadableName( const QString& camelCaseName );

	//! \brief Called by plugins which process script packages to report packages being in use
	static void markScriptPackageLoaded( Plugin::Uid uid );

Q_SIGNALS:
	void addOnsChanged();

private:
	static constexpr auto ManifestFileName = "manifest.json";
	static constexpr auto IconFileName = "icon.png";
	static constexpr auto UserDisabledMarkerFileName = ".disabled";

	AddOn addOnFromPluginObject( QObject* pluginObject, bool loaded ) const;
	static AddOn addOnFromManifest( const QJsonObject& manifest, const QString& location, Origin origin );

	static bool isDisabledInConfiguration( Plugin::Uid uid );

	static bool removeDirectory( const QString& path, const QString& baseDirectory );

};
