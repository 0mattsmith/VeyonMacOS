include(BuildVeyonPlugin)

build_veyon_plugin(filesender
	NAME FileSender
	SOURCES
	FileSenderDialog.cpp
	FileSenderDialog.h
	FileSenderPlugin.cpp
	FileSenderPlugin.h
	filesender.qrc
	)

target_link_libraries(filesender PRIVATE veyon-addon-common)
