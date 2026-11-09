/*
 * CryptoCore.cpp - core functions for crypto features
 *
 * Copyright (c) 2017-2026 Tobias Junghans <tobydox@veyon.io>
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

#include <QCoreApplication>
#include <QDir>
#include <QPluginLoader>
#include <QRandomGenerator>

#include "CryptoCore.h"

CryptoCore::CryptoCore() :
	m_qcaInitializer(),
	m_defaultPrivateKey()
{
#ifdef Q_OS_MACOS
	loadBundledQcaPlugins();
#endif
	const auto features = QCA::supportedFeatures();

	vDebug() << "CryptoCore instance created - features supported by QCA" << qcaVersionStr() << features;

	if( features.contains( QStringLiteral( "rsa" ) ) == false )
	{
		qFatal( "CryptoCore: RSA not supported! Please install a QCA plugin which provides RSA support "
				"(e.g. packages such as libqca-qt5-2-plugins or qca-qt5-ossl)." );
	}

	m_defaultPrivateKey = PrivateKey::fromPEMFile( QStringLiteral(":/core/default-pkey.pem") );
}



CryptoCore::~CryptoCore()
{
	vDebug();
}



void CryptoCore::loadBundledQcaPlugins()
{
	// Besides Qt's library paths, QCA always scans the plugin directory it has been built for (e.g. by Homebrew)
	// and loads each plugin found there before checking whether it already has the provider. If QCA is installed
	// on the computer as well, this would load a second copy of Qt and other libraries into the application.
	// Therefore, when running from an application bundle, disable QCA's plugin scanning and only add the
	// providers of the plugins bundled with the application.
	const auto applicationDirPath = QCoreApplication::applicationDirPath();
	const QDir bundledPluginDir( applicationDirPath + QStringLiteral("/../PlugIns/crypto") );
	if( applicationDirPath.endsWith( QLatin1String(".app/Contents/MacOS") ) == false || bundledPluginDir.exists() == false )
	{
		return;
	}

	// checked by QCA whenever it scans for plugins, i.e. has to remain set
	qputenv( "QCA_NO_PLUGINS", "1" );

	const auto fileNames = bundledPluginDir.entryList( QDir::Files );
	for( const auto& fileName : fileNames )
	{
		QPluginLoader loader( bundledPluginDir.absoluteFilePath( fileName ) );
		const auto plugin = qobject_cast<QCAPlugin *>( loader.instance() );
		const auto provider = plugin ? plugin->createProvider() : nullptr;
		if( provider == nullptr )
		{
			vWarning() << "could not load QCA plugin" << fileName << loader.errorString();
		}
		else if( QCA::insertProvider( provider ) == false )
		{
			vWarning() << "could not add QCA provider of" << fileName;
			delete provider;
		}
	}
}



QByteArray CryptoCore::generateChallenge()
{
	static_assert(ChallengeSize % sizeof(quint32) == 0);

	using ChallengeWords = std::array<quint32, ChallengeSize / sizeof(quint32)>;

	ChallengeWords challenge{};
	QRandomGenerator::system()->fillRange(challenge.data(), challenge.size());

	return QByteArray(reinterpret_cast<const char*>(challenge.data()), ChallengeSize);
}



QString CryptoCore::encryptPassword( const PlaintextPassword& password ) const
{
	return QString::fromLatin1( m_defaultPrivateKey.toPublicKey().
								encrypt( password, DefaultEncryptionAlgorithm ).toByteArray().toHex() );
}



CryptoCore::PlaintextPassword CryptoCore::decryptPassword( const QString& encryptedPassword ) const
{
	PlaintextPassword decryptedPassword;

	if( PrivateKey( m_defaultPrivateKey ).decrypt( QByteArray::fromHex( encryptedPassword.toUtf8() ),
												   &decryptedPassword, DefaultEncryptionAlgorithm ) )
	{
		return decryptedPassword;
	}

	vCritical() << "failed to decrypt password!";

	return {};
}
