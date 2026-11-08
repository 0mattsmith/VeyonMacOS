include(BuildVeyonPlugin)

set(taskmanager_SOURCES
	ApplicationRules.h
	ProcessBackend.cpp
	ProcessBackend.h
	ProcessInfo.h
	TaskManagerDialog.cpp
	TaskManagerDialog.h
	TaskManagerPlugin.cpp
	TaskManagerPlugin.h
	taskmanager.qrc
	)

if(VEYON_BUILD_WINDOWS)
	list(APPEND taskmanager_SOURCES ProcessBackendWindows.cpp)
elseif(VEYON_BUILD_MACOS)
	list(APPEND taskmanager_SOURCES ProcessBackendMacOS.cpp)
else()
	list(APPEND taskmanager_SOURCES ProcessBackendLinux.cpp)
endif()

build_veyon_plugin(taskmanager
	NAME TaskManager
	SOURCES ${taskmanager_SOURCES}
	)

target_link_libraries(taskmanager PRIVATE veyon-addon-common)

if(VEYON_BUILD_WINDOWS)
	target_link_libraries(taskmanager PRIVATE -lpsapi -ladvapi32 -luser32)
endif()
