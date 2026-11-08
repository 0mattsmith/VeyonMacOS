include(BuildVeyonPlugin)

build_veyon_plugin(macos-screensharing-vnc-server
	NAME MacOsScreenSharingVncServer
	SOURCES
	MacOsScreenSharingVncServer.cpp
	MacOsScreenSharingVncServer.h
	MacOsScreenSharingConfiguration.h
	MacOsScreenSharingConfigurationWidget.cpp
	MacOsScreenSharingConfigurationWidget.h
	)
