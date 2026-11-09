#!/usr/bin/env bash
#
# msys2-package.sh - creates the Windows installer from a native MSYS2 build (UCRT64 or MINGW64 shell)
#
# usage: msys2-package.sh [--strip] <build directory> [output directory]
#
# Assembles the programs, Veyon plugins and add-ons, Qt and QCA plugins and all DLLs they depend on (resolved
# with ntldd) in the layout expected by nsis/veyon.nsi and runs makensis. The cross-compiling counterpart is the
# windows-binaries target (cmake/modules/WindowsInstaller.cmake).
#
# --strip removes symbols from the binaries (smaller installer for releases, but no function names in backtraces)
#

set -euo pipefail

STRIP=0
if [ "${1:-}" = "--strip" ] ; then
	STRIP=1
	shift
fi

SRC=$(cd "$(dirname "$0")/../.." && pwd)
BUILD=$(cd "$1" && pwd)
OUTPUT=$(mkdir -p "${2:-.}" && cd "${2:-.}" && pwd)
PREFIX=${MINGW_PREFIX:?run this script in an MSYS2 UCRT64 or MINGW64 shell}
QT_PLUGINS=$PREFIX/share/qt6/plugins

STAGE=$(mktemp -d)/veyon
mkdir -p "$STAGE"/{plugins,translations,crypto,imageformats,platforms,styles,tls,interception}

echo "== Collecting Veyon"
find "$BUILD" -mindepth 2 -name 'veyon-*.exe' -not -path '*/CMakeFiles/*' -exec cp {} "$STAGE/" \;
cp "$BUILD/core/veyon-core.dll" "$STAGE/"
find "$BUILD/plugins" "$BUILD/addons" -name '*.dll' -not -path '*/CMakeFiles/*' -exec cp {} "$STAGE/plugins/" \;
# shared libraries built along with the plugins (e.g. kldap-light) and the VNC hooks belong next to the programs
mv "$STAGE"/plugins/lib*.dll "$STAGE/" 2> /dev/null || true
mv "$STAGE/plugins/vnchooks.dll" "$STAGE/" 2> /dev/null || true
cp "$BUILD"/translations/*.qm "$STAGE/translations/"
cp "$SRC"/3rdparty/interception/* "$STAGE/interception/"
cp "$SRC/3rdparty/ddengine/ddengine64.dll" "$STAGE/"

echo "== Collecting Qt and QCA plugins"
cp "$QT_PLUGINS/crypto/libqca-ossl.dll" "$STAGE/crypto/"
cp "$QT_PLUGINS/imageformats/qjpeg.dll" "$STAGE/imageformats/"
cp "$QT_PLUGINS/platforms/qwindows.dll" "$STAGE/platforms/"
cp "$QT_PLUGINS"/styles/*.dll "$STAGE/styles/"
cp "$QT_PLUGINS/tls/qopensslbackend.dll" "$STAGE/tls/"

echo "== Collecting DLL dependencies"
# ntldd prints e.g. "Qt6Core.dll => C:\msys64\ucrt64\bin\Qt6Core.dll (0x...)" - copy everything from the MSYS2 prefix
prefixBinWindows=$(cygpath -w "$PREFIX/bin")
while true ; do
	missing=$(find "$STAGE" -iname '*.exe' -o -iname '*.dll' | while read -r binary ; do
		ntldd -R "$binary" 2> /dev/null | awk -F ' => ' '{ print $2 }' | sed -E 's/ \(0x[0-9a-fA-F]+\)$//'
	done | grep -iF "$prefixBinWindows\\" | sort -u | while read -r dll ; do
		if [ ! -e "$STAGE/$(basename "$(cygpath -u "$dll")")" ] ; then
			cygpath -u "$dll"
		fi
	done || true)
	if [ -z "$missing" ] ; then
		break
	fi
	echo "$missing" | while read -r dll ; do
		echo "  $(basename "$dll")"
		cp "$dll" "$STAGE/"
	done
done

echo "== Checking for unresolved dependencies"
unresolved=$(find "$STAGE" -iname '*.exe' -o -iname '*.dll' | while read -r binary ; do
	# DLLs which are next to the program at runtime (e.g. veyon-core.dll for plugins) aren't found when
	# analyzing a plugin in its subdirectory, so only report DLLs which are missing in the staging directory
	# API sets (api-ms-win-*, ext-ms-win-*) are virtual DLLs resolved by Windows itself
	ntldd "$binary" 2> /dev/null | grep -i "not found" | awk '{ print $1 }' | grep -viE "^(api|ext)-ms-win-" | while read -r dll ; do
		if [ ! -e "$STAGE/$dll" ] && [ ! -e "$(dirname "$binary")/$dll" ] ; then
			echo "$(basename "$binary"): $dll"
		fi
	done
done | sort -u || true)
if [ -n "$unresolved" ] ; then
	echo "error: unresolved DLL dependencies:" >&2
	echo "$unresolved" >&2
	exit 1
fi

if [ $STRIP -eq 1 ] ; then
	echo "== Stripping binaries"
	# third party binaries are left untouched (e.g. the signed Interception installer)
	find "$STAGE" \( -iname '*.exe' -o -iname '*.dll' \) -not -path '*/interception/*' -not -name 'ddengine64.dll' \
		-exec strip --strip-unneeded {} \;
fi

echo "== Building installer"
cp "$SRC/COPYING" "$STAGE/"
cp "$SRC/COPYING" "$STAGE/LICENSE.TXT"
cp "$SRC/README.md" "$STAGE/README.TXT"
unix2dos -q "$STAGE"/*.TXT
cp -r "$SRC/nsis" "$STAGE/"
cp "$BUILD/nsis/veyon.nsi" "$STAGE/"
( cd "$STAGE" && makensis -V2 veyon.nsi )
mv "$STAGE"/veyon-*setup.exe "$OUTPUT/"
rm -rf "$(dirname "$STAGE")"

echo "== Created $(ls "$OUTPUT"/veyon-*setup.exe)"
