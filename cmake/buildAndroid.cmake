function("buildAndroid")
	add_definitions(-DANDROID -D__USE_BSD)
	add_compile_options(-O3 -ffast-math -fpic -fstack-protector -fno-strict-aliasing -fno-rtti -fno-omit-frame-pointer -fno-exceptions -fno-short-enums -std=gnu17 -I$(NDK)/sysroot/usr/include -I$(NDK)/sysroot/usr/include/android -I$(NDK)/toolchains/llvm/prebuilt/$(HOST)/sysroot/usr/include -I$(NDK)/toolchains/llvm/prebuilt/$(HOST)/sysroot/usr/include/android -include ${PROJECT_SOURCE_DIR}/system/android_fopen.h)

	list(APPEND PROJECT_SOURCES
		system/android_fopen.c
		system/android_main.c
		system/android_native_app_glue.c
	)

	add_library(${CMAKE_PROJECT_NAME} SHARED ${PROJECT_SOURCES})
	add_dependencies(${CMAKE_PROJECT_NAME} ShaderCompilation)

	target_link_libraries(
		${CMAKE_PROJECT_NAME} PUBLIC
		Vulkan::Vulkan
		OpenXR::openxr_loader
		Vorbis::vorbisfile
		m
		android
		log
		aaudio
		nativewindow
	)

#	add_custom_command(
#		TARGET ${CMAKE_PROJECT_NAME}
#		POST_BUILD
#		COMMAND 
#	)
endFunction()
