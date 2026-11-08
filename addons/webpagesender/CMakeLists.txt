include(BuildVeyonPlugin)

build_veyon_plugin(webpagesender
	NAME WebPageSender
	SOURCES
	WebPageSenderDialog.cpp
	WebPageSenderDialog.h
	WebPageSenderPlugin.cpp
	WebPageSenderPlugin.h
	webpagesender.qrc
	)

target_link_libraries(webpagesender PRIVATE veyon-addon-common)
