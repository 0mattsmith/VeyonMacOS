include(BuildVeyonPlugin)

# ScreenCaptureKit is an Objective-C API (see MacOsScreenCapture.mm)
enable_language(OBJCXX)

build_veyon_plugin(macos-builtin-vnc-server
	NAME MacOsBuiltinVncServer
	SOURCES
	MacOsBuiltinVncServer.cpp
	MacOsBuiltinVncServer.h
	MacOsVncServerCore.cpp
	MacOsVncServerCore.h
	MacOsScreenCapture.mm
	MacOsScreenCapture.h
	MacOsInputInjector.cpp
	MacOsInputInjector.h
)

# these files do not use Qt (so they can be built and tried without Veyon, see README.md) and include Apple headers
# which don't mix well with the precompiled Qt headers or unity builds
set_source_files_properties(MacOsVncServerCore.cpp MacOsScreenCapture.mm MacOsInputInjector.cpp PROPERTIES
	SKIP_PRECOMPILE_HEADERS TRUE
	SKIP_UNITY_BUILD_INCLUSION TRUE
)
set_source_files_properties(MacOsScreenCapture.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")

if(WITH_BUNDLED_LIBVNC)
	target_link_libraries(macos-builtin-vnc-server PRIVATE vncserver)
else()
	target_link_libraries(macos-builtin-vnc-server PRIVATE LibVNC::LibVNCServer)
endif()

target_link_libraries(macos-builtin-vnc-server PRIVATE
	"-framework Accelerate"
	"-framework ApplicationServices"
	"-framework Carbon"
	"-framework CoreGraphics"
	"-framework CoreMedia"
	"-framework CoreVideo"
	"-framework Foundation"
	"-framework ScreenCaptureKit"
)

target_compile_options(macos-builtin-vnc-server PRIVATE -Wno-parentheses)
