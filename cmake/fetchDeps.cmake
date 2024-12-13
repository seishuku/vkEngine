include(FetchContent)

function(fetchDeps)
	message(STATUS "${PROJECT_NAME}: Fetching OpenXR...")
	add_subdirectory(deps/OpenXR-SDK)
#	FetchContent_Declare(openxr-loader
#		GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
#		GIT_SHALLOW TRUE
#		GIT_TAG 288d3a7ebc1ad959f62d51da75baa3d27438c499 #1.0.34
#	)
#	FetchContent_MakeAvailable(openxr-loader)

	if(NOT CMAKE_SYSTEM_NAME MATCHES "Android") # Android has it's own decent audio library
	message(STATUS "${PROJECT_NAME}: Fetching PortAudio...")
	add_subdirectory(deps/portaudio)
#	FetchContent_Declare(portaudio
#		GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
#		GIT_SHALLOW TRUE
#		GIT_TAG 147dd722548358763a8b649b3e4b41dfffbcfbb6 #v19.7.0
#	)
#	FetchContent_MakeAvailable(portaudio)
	endif()

	message(STATUS "${PROJECT_NAME}: Fetching OGG...")
	add_subdirectory(deps/ogg)
#	FetchContent_Declare(ogg
#		GIT_REPOSITORY https://github.com/xiph/ogg.git
#		GIT_SHALLOW TRUE
#	#	GIT_TAG e1774cd77f471443541596e09078e78fdc342e4f #1.3.5
#	)
#	FetchContent_MakeAvailable(ogg)

	message(STATUS "${PROJECT_NAME}: Fetching Vorbis...")
	add_subdirectory(deps/vorbis)
#	FetchContent_Declare(vorbis
#		GIT_REPOSITORY https://github.com/xiph/vorbis.git
#		GIT_SHALLOW TRUE
#	#	Apparently this commit won't properly find ogg with FetchContent, so just use whatever latest is.
#	#	GIT_TAG 0657aee69dec8508a0011f47f3b69d7538e9d262 #1.3.7
#	)
#	FetchContent_MakeAvailable(vorbis)
endFunction()