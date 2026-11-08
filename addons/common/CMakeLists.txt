add_library(veyon-addon-common STATIC
	AddOnState.cpp
	AddOnState.h
	AddOnUtils.cpp
	AddOnUtils.h
	FileReceiver.cpp
	FileReceiver.h
	SimpleConfigurationPage.cpp
	SimpleConfigurationPage.h
	WorkerIdleTimer.h
	)

set_default_target_properties(veyon-addon-common)
set_target_properties(veyon-addon-common PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
target_include_directories(veyon-addon-common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(veyon-addon-common PUBLIC veyon-core)
