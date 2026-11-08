include(BuildVeyonPlugin)

set(volumecontrol_SOURCES
	VolumeBackend.h
	VolumeControlPlugin.cpp
	VolumeControlPlugin.h
	VolumeManagerDialog.cpp
	VolumeManagerDialog.h
	volumecontrol.qrc
	)

if(VEYON_BUILD_WINDOWS)
	list(APPEND volumecontrol_SOURCES VolumeBackendWindows.cpp)
elseif(VEYON_BUILD_MACOS)
	list(APPEND volumecontrol_SOURCES VolumeBackendMacOS.cpp)
else()
	list(APPEND volumecontrol_SOURCES VolumeBackendLinux.cpp)
endif()

build_veyon_plugin(volumecontrol
	NAME VolumeControl
	SOURCES ${volumecontrol_SOURCES}
	)

target_link_libraries(volumecontrol PRIVATE veyon-addon-common)

if(VEYON_BUILD_WINDOWS)
	target_link_libraries(volumecontrol PRIVATE -lole32)
elseif(VEYON_BUILD_MACOS)
	target_link_libraries(volumecontrol PRIVATE "-framework CoreAudio" "-framework AudioToolbox")
endif()
