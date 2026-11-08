# MacOSBundle.cmake - application bundles, installer package and disk image for macOS
#
# "cmake --install" installs Veyon.app (all programs, libraries and plugins) and "Veyon Configurator.app"
# (a launcher for the Configurator in Veyon.app) into CMAKE_INSTALL_PREFIX, which defaults to /Applications.
#
# For distribution, the following targets create self-contained bundles which run on Macs without Qt or
# Homebrew installed:
#
#   macos-bundle   - installs into <build>/macos-staging, bundles Qt and all other libraries and signs everything
#   macos-package  - creates an installer package (.pkg) and a disk image (.dmg) in <build>/macos-dist
#
# By default, bundles are signed ad-hoc (Gatekeeper asks users to confirm opening them). Set the cache variables
# below for Developer ID signing and notarization.

set(VEYON_MACOS_BUNDLE_IDENTIFIER "io.veyon.Veyon")
set(VEYON_MACOS_SIGNING_IDENTITY "-" CACHE STRING "Code signing identity (\"Developer ID Application: ...\" or \"-\" for ad-hoc signing)")
set(VEYON_MACOS_INSTALLER_IDENTITY "" CACHE STRING "Identity for signing the installer package (\"Developer ID Installer: ...\")")
set(VEYON_MACOS_NOTARY_PROFILE "" CACHE STRING "Keychain profile for notarization (see \"xcrun notarytool store-credentials\")")

set(VEYON_MACOS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/macos")
set(VEYON_MACOS_BINARY_DIR "${CMAKE_BINARY_DIR}/macos")
set(VEYON_MACOS_CONFIGURATOR_BUNDLE_CONTENTS_DIR "Veyon Configurator.app/Contents")
set(VEYON_MACOS_STAGING_DIR "${CMAKE_BINARY_DIR}/macos-staging")
set(VEYON_MACOS_DIST_DIR "${CMAKE_BINARY_DIR}/macos-dist")

file(WRITE "${VEYON_MACOS_BINARY_DIR}/PkgInfo" "APPL????")


#
# Veyon.app - executables, libraries and plugins are installed by the corresponding targets
#
configure_file("${VEYON_MACOS_SOURCE_DIR}/Info.plist.in" "${VEYON_MACOS_BINARY_DIR}/Info.plist" @ONLY)
install(FILES "${VEYON_MACOS_BINARY_DIR}/Info.plist" "${VEYON_MACOS_BINARY_DIR}/PkgInfo"
	DESTINATION "${VEYON_MACOS_BUNDLE_CONTENTS_DIR}")
install(FILES "${VEYON_MACOS_SOURCE_DIR}/Veyon.icns" DESTINATION "${VEYON_INSTALL_DATA_DIR}")
install(PROGRAMS "${VEYON_MACOS_SOURCE_DIR}/uninstall.sh" DESTINATION "${VEYON_INSTALL_DATA_DIR}")


#
# Veyon Configurator.app
#
add_executable(veyon-configurator-launcher "${VEYON_MACOS_SOURCE_DIR}/ConfiguratorLauncher.c")
target_link_libraries(veyon-configurator-launcher PRIVATE "-framework CoreFoundation")
target_compile_options(veyon-configurator-launcher PRIVATE -Wall)

configure_file("${VEYON_MACOS_SOURCE_DIR}/ConfiguratorInfo.plist.in" "${VEYON_MACOS_BINARY_DIR}/ConfiguratorInfo.plist" @ONLY)
install(TARGETS veyon-configurator-launcher RUNTIME DESTINATION "${VEYON_MACOS_CONFIGURATOR_BUNDLE_CONTENTS_DIR}/MacOS")
install(FILES "${VEYON_MACOS_BINARY_DIR}/ConfiguratorInfo.plist" RENAME Info.plist
	DESTINATION "${VEYON_MACOS_CONFIGURATOR_BUNDLE_CONTENTS_DIR}")
install(FILES "${VEYON_MACOS_BINARY_DIR}/PkgInfo" DESTINATION "${VEYON_MACOS_CONFIGURATOR_BUNDLE_CONTENTS_DIR}")
install(FILES "${VEYON_MACOS_SOURCE_DIR}/VeyonConfigurator.icns" DESTINATION "${VEYON_MACOS_CONFIGURATOR_BUNDLE_CONTENTS_DIR}/Resources")


#
# distribution
#
if(TARGET Qt${QT_MAJOR_VERSION}::qmake)
	get_target_property(_qmake_location Qt${QT_MAJOR_VERSION}::qmake IMPORTED_LOCATION)
	get_filename_component(_qt_bin_dir "${_qmake_location}" DIRECTORY)
endif()
find_program(MACDEPLOYQT_EXECUTABLE NAMES macdeployqt macdeployqt6
	HINTS "${_qt_bin_dir}" "${QT_TRANSLATIONS_DIR}/../bin" /opt/homebrew/opt/qt/bin /usr/local/opt/qt/bin)

if(NOT MACDEPLOYQT_EXECUTABLE)
	message(WARNING "macdeployqt not found - the macos-bundle and macos-package targets are not available")
	return()
endif()

add_custom_target(macos-bundle
	COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --config $<CONFIG>
	COMMAND ${CMAKE_COMMAND} -E rm -rf "${VEYON_MACOS_STAGING_DIR}"
	COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --config $<CONFIG> --prefix "${VEYON_MACOS_STAGING_DIR}" --strip
	COMMAND /bin/bash "${VEYON_MACOS_SOURCE_DIR}/deploy-bundle.sh"
		"${VEYON_MACOS_STAGING_DIR}"
		"${MACDEPLOYQT_EXECUTABLE}"
		"${VEYON_MACOS_SIGNING_IDENTITY}"
		"${VEYON_MACOS_SOURCE_DIR}/Veyon.entitlements"
	WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
	COMMENT "Creating self-contained application bundles in ${VEYON_MACOS_STAGING_DIR}"
	USES_TERMINAL
	VERBATIM)

add_custom_target(macos-package
	COMMAND ${CMAKE_COMMAND} -E env
		"VEYON_MACOS_SIGNING_IDENTITY=${VEYON_MACOS_SIGNING_IDENTITY}"
		"VEYON_MACOS_INSTALLER_IDENTITY=${VEYON_MACOS_INSTALLER_IDENTITY}"
		"VEYON_MACOS_NOTARY_PROFILE=${VEYON_MACOS_NOTARY_PROFILE}"
		/bin/bash "${VEYON_MACOS_SOURCE_DIR}/build-package.sh"
		"${VEYON_MACOS_STAGING_DIR}"
		"${VEYON_MACOS_DIST_DIR}"
		"${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
		"${VEYON_MACOS_SOURCE_DIR}"
		"${CMAKE_SOURCE_DIR}/COPYING"
	WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
	COMMENT "Creating installer package and disk image in ${VEYON_MACOS_DIST_DIR}"
	USES_TERMINAL
	VERBATIM)
add_dependencies(macos-package macos-bundle)
