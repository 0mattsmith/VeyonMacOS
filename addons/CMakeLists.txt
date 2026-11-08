# Veyon add-ons
#
# Every subdirectory containing a CMakeLists.txt file is built as an add-on plugin. Add-ons are
# regular Veyon plugins which additionally implement the AddOnInterface so they show up in the
# Add-On Store where they can be enabled and disabled.

include(BuildVeyonPlugin)

# shared helper code for all add-ons (protocol helpers, platform abstractions, widgets)
add_subdirectory(common)

file(GLOB addons RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)

foreach(addon ${addons})
	if(addon STREQUAL "common" OR addon STREQUAL "sdk" OR addon STREQUAL "tools")
		continue()
	endif()
	if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${addon} AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${addon}/CMakeLists.txt)
		add_subdirectory(${addon})
	endif()
endforeach()
