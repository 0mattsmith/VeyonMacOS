include(BuildVeyonPlugin)

build_veyon_plugin(filemanager
	NAME FileManager
	SOURCES
	FileManagerConfiguration.h
	FileManagerDialog.cpp
	FileManagerDialog.h
	FileManagerPlugin.cpp
	FileManagerPlugin.h
	filemanager.qrc
	)

target_link_libraries(filemanager PRIVATE veyon-addon-common)
