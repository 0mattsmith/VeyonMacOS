#!/bin/bash
#
# deploy-bundle.sh - makes Veyon.app self-contained and signs the application bundles
#
# usage: deploy-bundle.sh <staging directory> <macdeployqt> <signing identity> <entitlements file>
#
# The staging directory contains Veyon.app and "Veyon Configurator.app" as installed by CMake. The signing
# identity is either "-" for ad-hoc signing (sufficient for testing - Gatekeeper asks users to confirm opening
# the app) or a "Developer ID Application: ..." identity, which is required for notarization.
#
# Environment: QCA_PLUGIN_DIR - directory containing libqca-ossl.dylib if it can't be found automatically
#

set -euo pipefail

STAGING="$1"
MACDEPLOYQT="$2"
IDENTITY="$3"
ENTITLEMENTS="$4"

APP="$STAGING/Veyon.app"
LAUNCHER_APP="$STAGING/Veyon Configurator.app"
MAIN_EXECUTABLE="veyon-master"

if [ ! -d "$APP" ] ; then
	echo "error: $APP not found" >&2
	exit 1
fi

is_macho()
{
	file -b "$1" | grep -q "Mach-O"
}

# all Mach-O files of the bundle except the frameworks (they are signed as a whole)
list_binaries()
{
	find "$APP/Contents" -type f -not -path "*.framework/*" | while read -r file ; do
		if is_macho "$file" ; then
			echo "$file"
		fi
	done
}


echo "== Deploying QCA crypto plugin"
# QCA loads its backends from <Qt plugin path>/crypto
qtbin=$(dirname "$MACDEPLOYQT")
candidates=()
if [ -n "${QCA_PLUGIN_DIR:-}" ] ; then
	candidates+=("$QCA_PLUGIN_DIR")
fi
for qtpaths in "$qtbin/qtpaths6" "$qtbin/qtpaths" ; do
	if [ -x "$qtpaths" ] ; then
		candidates+=("$("$qtpaths" --plugin-dir)/crypto")
		break
	fi
done
if command -v brew > /dev/null ; then
	# Homebrew installs the QCA plugins into lib/qt/plugins of the qca formula
	candidates+=("$(brew --prefix qca)/lib/qt/plugins/crypto" "$(brew --prefix)/lib/qt/plugins/crypto"
				 "$(brew --prefix qca)/lib/qt6/plugins/crypto" "$(brew --prefix)/lib/qt6/plugins/crypto")
fi

# QCA builds its plugins as CMake modules, which may have the suffix .so instead of .dylib on macOS
qcaPlugin=""
for dir in "${candidates[@]}" ; do
	for name in libqca-ossl.dylib libqca-ossl.so ; do
		if [ -f "$dir/$name" ] ; then
			qcaPlugin="$dir/$name"
			break 2
		fi
	done
done
if [ -z "$qcaPlugin" ] ; then
	echo "error: QCA OpenSSL plugin (libqca-ossl) not found (searched: ${candidates[*]}) - set QCA_PLUGIN_DIR" >&2
	exit 1
fi
echo "Using $qcaPlugin"
mkdir -p "$APP/Contents/PlugIns/crypto"
cp -f "$qcaPlugin" "$APP/Contents/PlugIns/crypto/"
chmod u+w "$APP/Contents/PlugIns/crypto/$(basename "$qcaPlugin")"


echo "== Deploying Qt and other libraries"
deployArgs=()
while read -r binary ; do
	# helper programs, Veyon plugins and Veyon's own libraries (e.g. libveyon-core) - macdeployqt only
	# analyzes the main executable and the Qt plugins by itself
	if [ "$(basename "$binary")" != "$MAIN_EXECUTABLE" ] ; then
		deployArgs+=("-executable=$binary")
	fi
done < <(list_binaries)

"$MACDEPLOYQT" "$APP" "${deployArgs[@]}" -always-overwrite -verbose=1


echo "== Removing Qt plugins whose Qt modules are not bundled"
# macdeployqt also copies plugins of Qt modules Veyon doesn't use (e.g. the PDF and SVG image format plugins
# or the virtual keyboard) without the modules themselves - Qt can't load these plugins anyway
for plugin in "$APP"/Contents/PlugIns/*/*.dylib ; do
	case "$plugin" in
		"$APP/Contents/PlugIns/veyon/"*|"$APP/Contents/PlugIns/crypto/"*) continue ;;
	esac
	if [ ! -f "$plugin" ] ; then
		continue
	fi
	missing=""
	for dependency in $(otool -L "$plugin" | tail -n +2 | awk '{ print $1 }' | grep '^@rpath/') ; do
		if [ ! -e "$APP/Contents/Frameworks/${dependency#@rpath/}" ] ; then
			missing="$missing ${dependency#@rpath/}"
		fi
	done
	if [ -n "$missing" ] ; then
		echo "removing ${plugin#"$APP/Contents/"} (requires$missing)"
		rm -f "$plugin"
	fi
done


echo "== Removing references to the build machine"
# CMake adds the library directories of the build machine (e.g. Homebrew) as rpaths - everything is
# bundled now, so remove them to make sure the bundled libraries are always used
while read -r binary ; do
	otool -l "$binary" | awk '/cmd LC_RPATH/ { getline; getline; print $2 }' | while read -r rpath ; do
		if [[ "$rpath" == /* ]] ; then
			install_name_tool -delete_rpath "$rpath" "$binary" 2> /dev/null || true
		fi
	done
done < <(list_binaries)

leftovers=$(list_binaries | while read -r binary ; do
	# otool -L also lists the install name of a library itself, which isn't a dependency
	ownName=$(otool -D "$binary" | tail -n +2)
	otool -L "$binary" | tail -n +2 | awk '{ print $1 }' | grep -vxF "${ownName:-/}" | \
		grep -E "^/(opt|usr/local)/" | sed "s|^|$binary: |" || true
done)
if [ -n "$leftovers" ] ; then
	echo "error: the following libraries have not been bundled:" >&2
	echo "$leftovers" >&2
	exit 1
fi


echo "== Signing with identity \"$IDENTITY\""
sign()
{
	local options=(--force --sign "$IDENTITY")
	if [ "$IDENTITY" = "-" ] ; then
		options+=(--timestamp=none)
	else
		# notarization requires the hardened runtime and secure timestamps
		options+=(--timestamp --options runtime)
	fi
	if [ -n "${2:-}" ] ; then
		options+=(--entitlements "$2")
	fi
	codesign "${options[@]}" "$1"
}

# sign from the inside out: libraries and plugins (*.dylib, *.so), frameworks, helper executables, bundles
while read -r binary ; do
	if [[ "$binary" != "$APP/Contents/MacOS/"* ]] ; then
		sign "$binary"
	fi
done < <(list_binaries)

for framework in "$APP"/Contents/Frameworks/*.framework ; do
	if [ -d "$framework" ] ; then
		sign "$framework"
	fi
done

for executable in "$APP"/Contents/MacOS/* ; do
	if [ "$(basename "$executable")" != "$MAIN_EXECUTABLE" ] && is_macho "$executable" ; then
		sign "$executable" "$ENTITLEMENTS"
	fi
done

sign "$APP" "$ENTITLEMENTS"

if [ -d "$LAUNCHER_APP" ] ; then
	sign "$LAUNCHER_APP"
fi

codesign --verify --strict --deep --verbose=1 "$APP"
if [ -d "$LAUNCHER_APP" ] ; then
	codesign --verify --strict --deep --verbose=1 "$LAUNCHER_APP"
fi

echo "== Bundles ready in $STAGING"
