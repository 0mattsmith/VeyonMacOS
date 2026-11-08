#!/bin/bash
#
# build-package.sh - creates the Veyon installer package (.pkg) and a disk image (.dmg) containing it
#
# usage: build-package.sh <staging directory> <output directory> <version> <macos source directory> <license file>
#
# The staging directory contains the deployed and signed bundles (see deploy-bundle.sh).
#
# Environment (all optional):
#   VEYON_MACOS_SIGNING_IDENTITY   - "Developer ID Application: ..." identity for signing the disk image
#   VEYON_MACOS_INSTALLER_IDENTITY - "Developer ID Installer: ..." identity for signing the installer package
#   VEYON_MACOS_NOTARY_PROFILE     - keychain profile for notarization ("xcrun notarytool store-credentials")
#

set -euo pipefail

STAGING="$1"
OUTPUT="$2"
VERSION="$3"
SOURCES="$4"
LICENSE="$5"

SIGNING_IDENTITY="${VEYON_MACOS_SIGNING_IDENTITY:-}"
INSTALLER_IDENTITY="${VEYON_MACOS_INSTALLER_IDENTITY:-}"
NOTARY_PROFILE="${VEYON_MACOS_NOTARY_PROFILE:-}"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

ARCHITECTURES=$(lipo -archs "$STAGING/Veyon.app/Contents/MacOS/veyon-master" | tr ' ' ',')
MINIMUM_MACOS_VERSION=$(/usr/libexec/PlistBuddy -c "Print :LSMinimumSystemVersion" "$STAGING/Veyon.app/Contents/Info.plist")
BASENAME="veyon-$VERSION-macos-$(echo "$ARCHITECTURES" | tr ',' '-')"

notarize()
{
	if [ -n "$NOTARY_PROFILE" ] ; then
		echo "== Notarizing $(basename "$1")"
		xcrun notarytool submit "$1" --keychain-profile "$NOTARY_PROFILE" --wait
		xcrun stapler staple "$1"
	fi
}


echo "== Building component packages"
mkdir -p "$WORK/root/Applications" "$WORK/packages" "$WORK/scripts"
ditto "$STAGING/Veyon.app" "$WORK/root/Applications/Veyon.app"
ditto "$STAGING/Veyon Configurator.app" "$WORK/root/Applications/Veyon Configurator.app"

# the installer must not "relocate" the bundles to other copies it finds on the system (e.g. in a build folder)
pkgbuild --analyze --root "$WORK/root" "$WORK/components.plist"
index=0
while /usr/libexec/PlistBuddy -c "Print :$index" "$WORK/components.plist" > /dev/null 2>&1 ; do
	/usr/libexec/PlistBuddy -c "Set :$index:BundleIsRelocatable false" "$WORK/components.plist"
	index=$((index + 1))
done

# file permissions may get lost when the sources are checked out on other platforms
cp -R "$SOURCES/installer/scripts-app" "$SOURCES/installer/scripts-service" "$WORK/scripts/"
chmod 755 "$WORK"/scripts/*/*

pkgbuild --root "$WORK/root" \
		 --component-plist "$WORK/components.plist" \
		 --identifier io.veyon.Veyon \
		 --version "$VERSION" \
		 --install-location / \
		 --scripts "$WORK/scripts/scripts-app" \
		 "$WORK/packages/VeyonApp.pkg"

pkgbuild --nopayload \
		 --identifier io.veyon.Veyon.service \
		 --version "$VERSION" \
		 --scripts "$WORK/scripts/scripts-service" \
		 "$WORK/packages/VeyonService.pkg"


echo "== Building installer package"
mkdir -p "$WORK/resources" "$OUTPUT"
cp "$LICENSE" "$WORK/resources/license.txt"
sed -e "s/@VERSION@/$VERSION/g" \
	-e "s/@ARCHITECTURES@/$ARCHITECTURES/g" \
	-e "s/@MINIMUM_MACOS_VERSION@/$MINIMUM_MACOS_VERSION/g" \
	"$SOURCES/installer/Distribution.xml" > "$WORK/Distribution.xml"

PKG="$OUTPUT/$BASENAME.pkg"
signArgs=()
if [ -n "$INSTALLER_IDENTITY" ] ; then
	signArgs=(--sign "$INSTALLER_IDENTITY" --timestamp)
fi
productbuild --distribution "$WORK/Distribution.xml" \
			 --package-path "$WORK/packages" \
			 --resources "$WORK/resources" \
			 "${signArgs[@]+"${signArgs[@]}"}" \
			 "$PKG"
notarize "$PKG"


echo "== Building disk image"
mkdir -p "$WORK/dmg"
cp "$PKG" "$WORK/dmg/Install Veyon.pkg"
cp "$SOURCES/uninstall.sh" "$WORK/dmg/Uninstall Veyon.command"
chmod 755 "$WORK/dmg/Uninstall Veyon.command"

DMG="$OUTPUT/$BASENAME.dmg"
# hdiutil occasionally fails with "Resource busy", especially on CI machines
for attempt in 1 2 3 ; do
	if hdiutil create -volname "Veyon $VERSION" -srcfolder "$WORK/dmg" -fs HFS+ -format UDZO -ov "$DMG" ; then
		break
	fi
	if [ $attempt -eq 3 ] ; then
		exit 1
	fi
	echo "hdiutil failed - retrying in 10 seconds"
	sleep 10
done
if [ -n "$SIGNING_IDENTITY" ] && [ "$SIGNING_IDENTITY" != "-" ] ; then
	codesign --force --sign "$SIGNING_IDENTITY" --timestamp "$DMG"
fi
notarize "$DMG"

echo "== Created $PKG and $DMG"
