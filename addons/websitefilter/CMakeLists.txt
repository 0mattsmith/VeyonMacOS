include(BuildVeyonPlugin)

set(websitefilter_SOURCES
	WebsiteFilterBackend.cpp
	WebsiteFilterBackend.h
	WebsiteFilterConfiguration.h
	WebsiteFilterDialog.cpp
	WebsiteFilterDialog.h
	WebsiteFilterPlugin.cpp
	WebsiteFilterPlugin.h
	WebsiteRules.h
	websitefilter.qrc
	)

if(VEYON_BUILD_WINDOWS)
	list(APPEND websitefilter_SOURCES WebsiteFilterBackendWindows.cpp)
elseif(VEYON_BUILD_MACOS)
	list(APPEND websitefilter_SOURCES WebsiteFilterBackendMacOS.cpp)
else()
	list(APPEND websitefilter_SOURCES WebsiteFilterBackendLinux.cpp)
endif()

build_veyon_plugin(websitefilter
	NAME WebsiteFilter
	SOURCES ${websitefilter_SOURCES}
	)

target_link_libraries(websitefilter PRIVATE veyon-addon-common)
