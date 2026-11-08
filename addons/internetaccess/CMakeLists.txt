include(BuildVeyonPlugin)

set(internetaccess_SOURCES
	FirewallBackend.h
	InternetAccessConfiguration.h
	InternetAccessPlugin.cpp
	InternetAccessPlugin.h
	NetworkRanges.cpp
	NetworkRanges.h
	internetaccess.qrc
	)

if(VEYON_BUILD_WINDOWS)
	list(APPEND internetaccess_SOURCES FirewallBackendWindows.cpp)
elseif(VEYON_BUILD_MACOS)
	list(APPEND internetaccess_SOURCES FirewallBackendMacOS.cpp)
else()
	list(APPEND internetaccess_SOURCES FirewallBackendLinux.cpp)
endif()

build_veyon_plugin(internetaccess
	NAME InternetAccess
	SOURCES ${internetaccess_SOURCES}
	)

target_link_libraries(internetaccess PRIVATE veyon-addon-common)

if(VEYON_BUILD_WINDOWS)
	target_link_libraries(internetaccess PRIVATE -liphlpapi -lws2_32)
endif()
