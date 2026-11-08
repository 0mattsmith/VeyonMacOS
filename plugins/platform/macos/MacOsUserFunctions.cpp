/*
 * MacOsUserFunctions.cpp - implementation of MacOsUserFunctions class
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

#include <QProcess>
#include <QRegularExpression>
#include <QTimer>

#include <grp.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <unistd.h>

#include "MacOsPlatformConfiguration.h"
#include "MacOsSessionFunctions.h"
#include "MacOsUserFunctions.h"
#include "ProcessHelper.h"
#include "VeyonConfiguration.h"


namespace {

struct PamCredentials
{
	QByteArray username;
	QByteArray password;
};

void secureZero( QByteArray& data )
{
	volatile char* p = data.data();
	for( qsizetype i = 0; i < data.size(); ++i )
	{
		p[i] = 0;
	}
}

char* duplicateString( const QByteArray& data )
{
	auto copy = static_cast<char *>( malloc( size_t(data.size()) + 1 ) );
	if( copy )
	{
		memcpy( copy, data.constData(), size_t(data.size()) );
		copy[data.size()] = 0;
	}
	return copy;
}

int pamConversation( int messageCount, const struct pam_message** messages, struct pam_response** responses, void* appData )
{
	if( messageCount <= 0 || appData == nullptr )
	{
		return PAM_CONV_ERR;
	}

	const auto credentials = static_cast<PamCredentials *>( appData );

	auto reply = static_cast<pam_response *>( calloc( size_t(messageCount), sizeof(pam_response) ) );
	if( reply == nullptr )
	{
		return PAM_BUF_ERR;
	}

	for( int i = 0; i < messageCount; ++i )
	{
		switch( messages[i]->msg_style )
		{
		case PAM_PROMPT_ECHO_ON:
			reply[i].resp = duplicateString( credentials->username );
			break;
		case PAM_PROMPT_ECHO_OFF:
			reply[i].resp = duplicateString( credentials->password );
			break;
		case PAM_TEXT_INFO:
		case PAM_ERROR_MSG:
			reply[i].resp = nullptr;
			break;
		default:
			for( int j = 0; j < i; ++j )
			{
				free( reply[j].resp );
			}
			free( reply );
			return PAM_CONV_ERR;
		}
		reply[i].resp_retcode = 0;
	}

	*responses = reply;

	return PAM_SUCCESS;
}

const QStringList& ignoredGroups()
{
	static const QStringList groups{
		QStringLiteral("nobody"), QStringLiteral("nogroup"), QStringLiteral("daemon"), QStringLiteral("sys"),
		QStringLiteral("tty"), QStringLiteral("operator"), QStringLiteral("mail"), QStringLiteral("bin"),
		QStringLiteral("procview"), QStringLiteral("procmod"), QStringLiteral("owner"), QStringLiteral("everyone"),
		QStringLiteral("group"), QStringLiteral("kmem"), QStringLiteral("certusers"), QStringLiteral("localaccounts"),
		QStringLiteral("netusers"), QStringLiteral("authedusers"), QStringLiteral("interactusers"),
		QStringLiteral("consoleusers"), QStringLiteral("accessibility"), QStringLiteral("utmpx"),
		QStringLiteral("dialer"), QStringLiteral("network"), QStringLiteral("smmsp"), QStringLiteral("netdev"),
	};
	return groups;
}

}



QString MacOsUserFunctions::queryCurrentUserProperty( UserProperty property )
{
	switch( property )
	{
	case UserProperty::LoginName: return currentUserLoginName();
	case UserProperty::FullName: return fullNameOfUser( currentUserLoginName() );
	case UserProperty::None: break;
	}

	return {};
}



QStringList MacOsUserFunctions::userGroups( bool queryDomainGroups )
{
	const auto node = queryDomainGroups ? QStringLiteral("/Search") : QStringLiteral(".");
	const auto output = QString::fromUtf8( ProcessHelper( QStringLiteral("/usr/bin/dscl"),
														 { node, QStringLiteral("-list"), QStringLiteral("/Groups") } ).runAndReadAll() );

	QStringList groups;
	const auto lines = output.split( QLatin1Char('\n') );
	for( const auto& line : lines )
	{
		const auto group = line.trimmed();
		if( group.isEmpty() ||
			group.startsWith( QLatin1Char('_') ) ||
			group.startsWith( QLatin1String("com.apple.") ) ||
			ignoredGroups().contains( group ) ||
			groups.contains( group ) )
		{
			continue;
		}
		groups.append( group );
	}

	return groups;
}



QStringList MacOsUserFunctions::groupsOfUser( const QString& username, bool queryDomainGroups )
{
	Q_UNUSED(queryDomainGroups)

	// id also resolves groups of directory (e.g. Active Directory) users
	const auto output = QString::fromUtf8( ProcessHelper( QStringLiteral("/usr/bin/id"),
														 { QStringLiteral("-Gn"), username } ).runAndReadAll() );

	static const QRegularExpression whitespaceRX( QStringLiteral("\\s+") );
	auto groups = output.trimmed().split( whitespaceRX );
	groups.removeAll( {} );

	return groups;
}



QString MacOsUserFunctions::userGroupSecurityIdentifier( const QString& groupName )
{
	const auto group = getgrnam( groupName.toUtf8().constData() );
	if( group )
	{
		return QString::number( group->gr_gid );
	}

	return {};
}



bool MacOsUserFunctions::isAnyUserLoggedOn()
{
	return MacOsSessionFunctions::consoleUser().isValid();
}



bool MacOsUserFunctions::prepareLogon( const QString& username, const Password& password )
{
	Q_UNUSED(username)
	Q_UNUSED(password)

	vWarning() << "remote logon is not supported on macOS";

	return false;
}



bool MacOsUserFunctions::performLogon( const QString& username, const Password& password )
{
	Q_UNUSED(username)
	Q_UNUSED(password)

	vWarning() << "remote logon is not supported on macOS";

	return false;
}



void MacOsUserFunctions::logoff()
{
	const auto user = MacOsSessionFunctions::consoleUser();
	if( user.isValid() == false )
	{
		return;
	}

	// ask loginwindow to log out without confirmation dialog (applications can still save documents)
	const auto logoutScript = QStringLiteral("tell application \"loginwindow\" to «event aevtrlgo»");

	if( geteuid() == 0 )
	{
		QProcess::startDetached( QStringLiteral("/bin/launchctl"),
								 { QStringLiteral("asuser"), QString::number( user.uid ),
								   QStringLiteral("/usr/bin/sudo"), QStringLiteral("-u"), user.name,
								   QStringLiteral("/usr/bin/osascript"), QStringLiteral("-e"), logoutScript } );

		// terminate the GUI session forcefully if the user is still logged in after the timeout
		const auto uid = user.uid;
		QTimer::singleShot( LogoffTimeout, [uid]() {
			if( MacOsSessionFunctions::consoleUser().uid == uid )
			{
				vWarning() << "user still logged in - terminating GUI session";
				QProcess::execute( QStringLiteral("/bin/launchctl"),
								   { QStringLiteral("bootout"), QStringLiteral("gui/%1").arg( uid ) } );
			}
		} );
	}
	else
	{
		QProcess::startDetached( QStringLiteral("/usr/bin/osascript"), { QStringLiteral("-e"), logoutScript } );
	}
}



bool MacOsUserFunctions::authenticate( const QString& username, const Password& password )
{
	PamCredentials credentials{ username.toUtf8(), password.toByteArray() };

	const auto service = MacOsPlatformConfiguration( &VeyonCore::config() ).pamServiceName().toUtf8();

	struct pam_conv conversation{ &pamConversation, &credentials };
	pam_handle_t* pamHandle = nullptr;

	auto result = pam_start( service.constData(), credentials.username.constData(), &conversation, &pamHandle );
	if( result == PAM_SUCCESS )
	{
		// services such as "checkpw" use "use_first_pass" and therefore expect the token to be set
		pam_set_item( pamHandle, PAM_AUTHTOK, credentials.password.constData() );

		result = pam_authenticate( pamHandle, PAM_SILENT );
		if( result == PAM_SUCCESS )
		{
			result = pam_acct_mgmt( pamHandle, PAM_SILENT );
		}

		if( result != PAM_SUCCESS )
		{
			vWarning() << "PAM authentication of user" << username << "failed:" << pam_strerror( pamHandle, result );
		}
	}
	else
	{
		vCritical() << "pam_start() failed for service" << service;
	}

	if( pamHandle )
	{
		pam_end( pamHandle, result );
	}

	secureZero( credentials.password );

	return result == PAM_SUCCESS;
}



MacOsUserFunctions::UserId MacOsUserFunctions::userIdFromName( const QString& username )
{
	const auto passwordEntry = getpwnam( username.toUtf8().constData() );
	if( passwordEntry )
	{
		return passwordEntry->pw_uid;
	}

	return InvalidUserId;
}



QString MacOsUserFunctions::currentUserLoginName() const
{
	if( geteuid() == 0 )
	{
		// Veyon Server and Service run as root - report the user logged in at the console
		const auto user = MacOsSessionFunctions::consoleUser();
		return user.isValid() ? user.name : QString{};
	}

	const auto passwordEntry = getpwuid( getuid() );
	if( passwordEntry )
	{
		return QString::fromUtf8( passwordEntry->pw_name );
	}

	return QString::fromUtf8( qgetenv( "USER" ) );
}



QString MacOsUserFunctions::fullNameOfUser( const QString& username )
{
	if( username.isEmpty() )
	{
		return {};
	}

	const auto passwordEntry = getpwnam( username.toUtf8().constData() );
	if( passwordEntry && passwordEntry->pw_gecos )
	{
		return QString::fromUtf8( passwordEntry->pw_gecos ).section( QLatin1Char(','), 0, 0 );
	}

	return {};
}
