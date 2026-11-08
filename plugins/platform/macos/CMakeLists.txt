include(BuildVeyonPlugin)

find_package(PAM)

build_veyon_plugin(macos-platform
	NAME MacOsPlatform
	SOURCES
	MacOsPlatformPlugin.cpp
	MacOsPlatformPlugin.h
	MacOsPlatformConfiguration.h
	MacOsCoreFunctions.cpp
	MacOsCoreFunctions.h
	MacOsCocoaHelpers.cpp
	MacOsCocoaHelpers.h
	MacOsFilesystemFunctions.cpp
	MacOsFilesystemFunctions.h
	MacOsInputDeviceFunctions.cpp
	MacOsInputDeviceFunctions.h
	MacOsKeyboardShortcutTrapper.h
	MacOsNetworkFunctions.cpp
	MacOsNetworkFunctions.h
	MacOsServerProcess.cpp
	MacOsServerProcess.h
	MacOsServiceCore.cpp
	MacOsServiceCore.h
	MacOsServiceFunctions.cpp
	MacOsServiceFunctions.h
	MacOsSessionFunctions.cpp
	MacOsSessionFunctions.h
	MacOsUserFunctions.cpp
	MacOsUserFunctions.h
	../common/LogonHelper.h
	../common/LogonHelper.cpp
	../common/PersistentLogonCredentials.h
	../common/PersistentLogonCredentials.cpp
	../common/PlatformSessionManager.h
	../common/PlatformSessionManager.cpp
	../common/ServiceDataManager.h
	../common/ServiceDataManager.cpp
)

# these files use Objective-C runtime and CoreFoundation types which don't mix well with unity builds
set_source_files_properties(MacOsCocoaHelpers.cpp MacOsInputDeviceFunctions.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE)

target_include_directories(macos-platform PRIVATE ../common)

target_link_libraries(macos-platform PRIVATE
	Qt${QT_MAJOR_VERSION}::Network
	"-framework ApplicationServices"
	"-framework CoreFoundation"
	"-framework CoreGraphics"
	"-framework IOKit"
	"-framework SystemConfiguration"
	objc
)

if(PAM_FOUND)
	target_include_directories(macos-platform PRIVATE ${PAM_INCLUDE_DIR})
	target_link_libraries(macos-platform PRIVATE ${PAM_LIBRARY})
else()
	# OpenPAM is part of every macOS SDK
	target_link_libraries(macos-platform PRIVATE pam)
endif()
