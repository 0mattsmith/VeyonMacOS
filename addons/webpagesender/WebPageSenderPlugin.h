/*
 * WebPageSenderPlugin.h - declaration of WebPageSenderPlugin class
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

#include <QUrl>

#include "AddOnInterface.h"
#include "FeatureProviderInterface.h"
#include "WorkerIdleTimer.h"

class WebPageSenderPlugin : public QObject, FeatureProviderInterface, PluginInterface, AddOnInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.AddOns.WebPageSender")
	Q_INTERFACES(PluginInterface FeatureProviderInterface AddOnInterface)
public:
	enum class Command
	{
		SendWebPage,
	};
	Q_ENUM(Command)

	enum class Argument
	{
		Url,
		Title,
		Mode,
		Browser,
		NewWindow,
		FullScreen,
	};
	Q_ENUM(Argument)

	enum class Mode
	{
		Open,
		Notify,
		DesktopShortcut,
	};
	Q_ENUM(Mode)

	enum class Browser
	{
		Default,
		Chrome,
		Edge,
		Firefox,
		Safari,
	};
	Q_ENUM(Browser)

	struct WebPage
	{
		QUrl url;
		QString title;
		Mode mode{Mode::Open};
		Browser browser{Browser::Default};
		bool newWindow{true};
		bool fullScreen{false};
	};

	explicit WebPageSenderPlugin( QObject* parent = nullptr );
	~WebPageSenderPlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("2e6f8a3c-9d14-4b72-b5c0-7a1e3d9f6b42") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral( "WebPageSender" );
	}

	QString description() const override
	{
		return tr( "Send web pages to student computers" );
	}

	QString vendor() const override
	{
		return QStringLiteral( "Veyon Community" );
	}

	QString copyright() const override
	{
		return QStringLiteral( "Veyon Community" );
	}

	QString addOnName() const override
	{
		return tr( "Web Page Sender" );
	}

	Category addOnCategory() const override
	{
		return Category::Communication;
	}

	QString addOnIconUrl() const override
	{
		return QStringLiteral(":/webpagesender/webpage-send.png");
	}

	QString addOnSummary() const override
	{
		return tr( "Open a web page on all selected student computers at once - immediately, after the student "
				   "clicked a notification or as shortcut on the desktop. Choose the browser and optionally open "
				   "the page in a new window or in full screen mode." );
	}

	const FeatureList& featureList() const override
	{
		return m_features;
	}

	bool controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						 const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool startFeature( VeyonMasterInterface& master, const Feature& feature,
					   const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool handleFeatureMessage( VeyonServerInterface& server,
							   const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

	bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message ) override;

	void sendWebPage( const ComputerControlInterfaceList& computerControlInterfaces, const WebPage& webPage );

	//! \brief Turns user input like "example.com" into a valid http(s) URL - returns an invalid URL otherwise
	static QUrl normalizeUrl( const QString& input );

private:
	static constexpr auto WorkerIdleTimeout = 2 * 60 * 1000;

	void openWebPage( const WebPage& webPage );
	void showNotification( const WebPage& webPage );
	bool createDesktopShortcut( const WebPage& webPage );
	static bool launchBrowser( const WebPage& webPage );

	const Feature m_webPageSenderFeature;
	const FeatureList m_features;

	WorkerIdleTimer m_idleTimer{WorkerIdleTimeout};

};
