/*
 * WebsiteFilterBackend.cpp - platform independent parts of WebsiteFilterBackend
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

#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>

#include "WebsiteFilterBackend.h"


namespace {

const auto SectionBegin = QStringLiteral("# BEGIN Veyon Website Filter - managed by Veyon, do not edit");
const auto SectionEnd = QStringLiteral("# END Veyon Website Filter");

}



QStringList WebsiteFilterBackend::firefoxPatterns( const QStringList& domains )
{
	QStringList patterns;
	for( const auto& domain : domains )
	{
		patterns.append( QStringLiteral("*://%1/*").arg( domain ) );
		patterns.append( QStringLiteral("*://*.%1/*").arg( domain ) );
	}
	return patterns;
}



bool WebsiteFilterBackend::updateHostsFile( const QString& hostsFilePath, const QStringList& blockedDomains )
{
	QFile file( hostsFilePath );
	QByteArray content;
	if( file.open( QFile::ReadOnly ) )
	{
		content = file.readAll();
		file.close();
	}

	const auto lineEnding = content.contains( "\r\n" ) ? QStringLiteral("\r\n") : QStringLiteral("\n");

	// remove a previous section managed by Veyon
	QStringList lines;
	bool insideSection = false;
	bool sectionFound = false;
	const auto existingLines = QString::fromUtf8( content ).split( QRegularExpression( QStringLiteral("\r?\n") ) );
	for( const auto& line : existingLines )
	{
		if( line.trimmed() == SectionBegin )
		{
			insideSection = true;
			sectionFound = true;
			continue;
		}
		if( line.trimmed() == SectionEnd )
		{
			insideSection = false;
			continue;
		}
		if( insideSection == false )
		{
			lines.append( line );
		}
	}

	while( lines.isEmpty() == false && lines.last().trimmed().isEmpty() )
	{
		lines.removeLast();
	}

	if( blockedDomains.isEmpty() && sectionFound == false )
	{
		return true;
	}

	if( blockedDomains.isEmpty() == false )
	{
		lines.append( QString() );
		lines.append( SectionBegin );
		for( const auto& domain : blockedDomains )
		{
			const QStringList hosts{ domain, QStringLiteral("www.") + domain };
			for( const auto& host : hosts )
			{
				lines.append( QStringLiteral("0.0.0.0 %1").arg( host ) );
				lines.append( QStringLiteral(":: %1").arg( host ) );
			}
		}
		lines.append( SectionEnd );
	}

	const auto data = ( lines.join( lineEnding ) + lineEnding ).toUtf8();

	QSaveFile saveFile( hostsFilePath );
	if( saveFile.open( QFile::WriteOnly ) && saveFile.write( data ) == data.size() && saveFile.commit() )
	{
		return true;
	}

	// replacing the file fails in some environments (e.g. if it is opened by another program or a bind mount)
	QFile directFile( hostsFilePath );
	return directFile.open( QFile::WriteOnly | QFile::Truncate ) && directFile.write( data ) == data.size();
}



QStringList WebsiteFilterBackend::browserProcessNames()
{
#if defined(Q_OS_WIN)
	return { QStringLiteral("chrome.exe"), QStringLiteral("msedge.exe"), QStringLiteral("brave.exe"),
			 QStringLiteral("firefox.exe"), QStringLiteral("opera.exe") };
#elif defined(Q_OS_MACOS)
	return { QStringLiteral("Google Chrome"), QStringLiteral("Microsoft Edge"), QStringLiteral("Brave Browser"),
			 QStringLiteral("firefox"), QStringLiteral("Safari") };
#else
	return { QStringLiteral("chrome"), QStringLiteral("chromium"), QStringLiteral("msedge"), QStringLiteral("brave"),
			 QStringLiteral("firefox") };
#endif
}



void WebsiteFilterBackend::closeBrowsers()
{
	const auto names = browserProcessNames();
	for( const auto& name : names )
	{
#if defined(Q_OS_WIN)
		// without /F the browsers are asked to close (so open tabs can be restored)
		AddOnUtils::runProcess( QStringLiteral("taskkill.exe"), { QStringLiteral("/IM"), name }, 10000 );
#else
		AddOnUtils::runProcess( QStringLiteral("/usr/bin/pkill"), { QStringLiteral("-TERM"), QStringLiteral("-x"), name }, 10000 );
#endif
	}
}
