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

	set(OUTPUT_APK "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}.apk")
	set(ANDROID_MANIFEST "${PROJECT_SOURCE_DIR}/AndroidManifest.xml")
	set(APK_BUILD_DIR "${PROJECT_SOURCE_DIR}/apk")
	set(ANDROID_VERSION "28")
	set(AAPT "$ENV{ANDROID_HOME}/build-tools/34.0.0/aapt")  # Update to your build-tools version
	set(ZIPALIGN "$ENV{ANDROID_HOME}/build-tools/34.0.0/zipalign")
	set(APK_SIGNER "$ENV{ANDROID_HOME}/build-tools/34.0.0/apksigner")  # Optional: for signing
	set(KEYSTORE "${PROJECT_SOURCE_DIR}/my-release-key.keystore")  # Path to your keystore file
	set(KEY_ALIAS "standkey")  # Replace with your key alias
	set(KEYSTORE_PASSWORD "password")  # Replace with your keystore password

	# Create the APK build directory
	file(MAKE_DIRECTORY ${APK_BUILD_DIR})

	# Collect pipeline file names
	file(GLOB PIPELINE_FILES "${PROJECT_SOURCE_DIR}/pipelines/*.pipeline")

	# Collect shader file names
	file(GLOB SHADER_FILES "${PROJECT_SOURCE_DIR}/shaders/*.spv")

	# Strip debug info/symbols from release binaries
	add_custom_command(
		TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
		COMMAND $<$<CONFIG:release>:${CMAKE_STRIP}> ARGS --strip-all $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
		COMMAND $<$<CONFIG:release>:${CMAKE_STRIP}> ARGS --strip-all $<TARGET_FILE:OpenXR::openxr_loader>
	)

	# Build APK folder
	add_custom_command(
		TARGET ${CMAKE_PROJECT_NAME} POST_BUILD

		# Copy app shared object
		COMMAND ${CMAKE_COMMAND} -E make_directory ${APK_BUILD_DIR}/lib/arm64-v8a
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${APK_BUILD_DIR}/lib/arm64-v8a/lib${CMAKE_PROJECT_NAME}.so
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:OpenXR::openxr_loader> ${APK_BUILD_DIR}/lib/arm64-v8a/

		# Copy shaders
		COMMAND ${CMAKE_COMMAND} -E make_directory ${APK_BUILD_DIR}/assets/shaders
		COMMAND ${CMAKE_COMMAND} -E copy ${SHADER_FILES} ${APK_BUILD_DIR}/assets/shaders/

		# Copy pipelines
		COMMAND ${CMAKE_COMMAND} -E make_directory ${APK_BUILD_DIR}/assets/pipelines
		COMMAND ${CMAKE_COMMAND} -E copy ${PIPELINE_FILES} ${APK_BUILD_DIR}/assets/pipelines/

		# Copy assets and remove music
		COMMAND ${CMAKE_COMMAND} -E make_directory ${APK_BUILD_DIR}/assets/assets
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/assets ${APK_BUILD_DIR}/assets/assets
		COMMAND ${CMAKE_COMMAND} -E remove_directory ${APK_BUILD_DIR}/assets/assets/music

		COMMENT "Preparing APK build directory"
	)

	add_custom_command(
		TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
		COMMAND ${AAPT} package -f -F ${OUTPUT_APK} -I $ENV{ANDROID_HOME}/platforms/android-${ANDROID_VERSION}/android.jar -M ${ANDROID_MANIFEST} -v --app-as-shared-lib --target-sdk-version ${ANDROID_VERSION} ${APK_BUILD_DIR}
		COMMENT "Creating APK with aapt"
	)

	add_custom_command(
		TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
		COMMAND ${ZIPALIGN} -f -p 4 ${OUTPUT_APK} ${OUTPUT_APK}.aligned
		COMMAND ${CMAKE_COMMAND} -E rename ${OUTPUT_APK}.aligned ${OUTPUT_APK}
		COMMENT "Aligning APK with zipalign"
	)

	add_custom_command(
		TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
		COMMAND ${APK_SIGNER} sign --ks ${KEYSTORE} --ks-key-alias ${KEY_ALIAS} --ks-pass pass:${KEYSTORE_PASSWORD} ${OUTPUT_APK}
		COMMENT "Signing APK with apksigner"
	)

	# Additional cleaning
	set_property(DIRECTORY PROPERTY ADDITIONAL_CLEAN_FILES ${PROJECT_SOURCE_DIR}/apk)
endFunction()
