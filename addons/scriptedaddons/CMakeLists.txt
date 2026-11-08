include(BuildVeyonPlugin)

build_veyon_plugin(scriptedaddons
	NAME ScriptedAddOns
	SOURCES
	ScriptedAddOnsConfiguration.h
	ScriptedAddOnsPlugin.cpp
	ScriptedAddOnsPlugin.h
	ScriptOutputDialog.cpp
	ScriptOutputDialog.h
	ScriptRunner.cpp
	ScriptRunner.h
	scriptedaddons.qrc
	)

target_link_libraries(scriptedaddons PRIVATE veyon-addon-common)
