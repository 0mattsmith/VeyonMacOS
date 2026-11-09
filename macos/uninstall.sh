#!/bin/bash
#
# uninstall.sh - removes Veyon from this Mac
#
# All restrictions applied by add-ons (e.g. website filters) are removed and the Screen Sharing settings found
# before Veyon was installed are restored. The Veyon configuration and installed add-ons are kept so a later
# installation picks them up again - pass --purge to remove them as well.
#
# Installed as Veyon.app/Contents/Resources/uninstall.sh and as "Uninstall Veyon.command" on the disk image.
#

set -u

if [ "$(id -u)" -ne 0 ] ; then
	echo "Administrator privileges are required for uninstalling Veyon - please enter your password."
	exec sudo /bin/bash "$0" "$@"
fi

PURGE=0
if [ "${1:-}" = "--purge" ] ; then
	PURGE=1
fi

APPLICATIONS=/Applications
APP="$APPLICATIONS/Veyon.app"
CLI="$APP/Contents/Helpers/veyon-cli"
SERVICE_LABEL=io.veyon.veyon-service
DATA_DIR="/Library/Application Support/Veyon"
SCREEN_SHARING_STATE="$DATA_DIR/ScreenSharingOriginalState.json"

echo "Stopping Veyon ..."
launchctl bootout "system/$SERVICE_LABEL" 2> /dev/null
rm -f "/Library/LaunchDaemons/$SERVICE_LABEL.plist"
for program in veyon-server veyon-worker veyon-master veyon-configurator ; do
	pkill -x "$program" 2> /dev/null
done

if [ -x "$CLI" ] ; then
	echo "Removing restrictions applied by add-ons ..."
	"$CLI" addons revert
fi

if [ -f "$SCREEN_SHARING_STATE" ] ; then
	echo "Restoring Screen Sharing settings ..."
	state()
	{
		plutil -extract "$1" raw -o - "$SCREEN_SHARING_STATE" 2> /dev/null
	}

	if [ "$(state vncSettingsFileExisted)" = "true" ] ; then
		state vncSettings | base64 -D > /Library/Preferences/com.apple.VNCSettings.txt
		chmod 600 /Library/Preferences/com.apple.VNCSettings.txt
	else
		rm -f /Library/Preferences/com.apple.VNCSettings.txt
	fi

	if [ "$(state legacyConnectionsEnabled)" != "true" ] ; then
		defaults delete /Library/Preferences/com.apple.RemoteManagement VNCLegacyConnectionsEnabled 2> /dev/null
	fi

	if [ "$(state screenSharingRunning)" != "true" ] ; then
		launchctl bootout system/com.apple.screensharing 2> /dev/null
		launchctl disable system/com.apple.screensharing
	fi

	rm -f "$SCREEN_SHARING_STATE"
fi

echo "Removing applications ..."
rm -rf "$APP" "$APPLICATIONS/Veyon Configurator.app"
pkgutil --forget io.veyon.Veyon > /dev/null 2>&1
pkgutil --forget io.veyon.Veyon.service > /dev/null 2>&1

if [ $PURGE -eq 1 ] ; then
	echo "Removing configuration, add-ons and logs ..."
	rm -rf "$DATA_DIR" /Library/Logs/Veyon
	# the configuration is stored as system wide preferences (cached by cfprefsd)
	defaults delete /Library/Preferences/io.veyon.Veyon 2> /dev/null
	rm -f /Library/Preferences/io.veyon.Veyon.plist
else
	echo "The configuration and add-ons have been kept in \"$DATA_DIR\" - run with --purge to remove them."
fi

echo "Veyon has been removed."
