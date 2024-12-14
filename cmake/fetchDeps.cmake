include(FetchContent)

function(fetchDeps)
	message(STATUS "${PROJECT_NAME}: Fetching OpenXR...")
	add_subdirectory(deps/OpenXR-SDK)

	if(NOT CMAKE_SYSTEM_NAME MATCHES "Android") # Android has it's own decent audio library
	message(STATUS "${PROJECT_NAME}: Fetching PortAudio...")
	set(PA_BUILD_SHARED OFF CACHE BOOL "" FORCE)
	add_subdirectory(deps/portaudio)
	endif()

	set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
	set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

	message(STATUS "${PROJECT_NAME}: Fetching OGG...")
	add_subdirectory(deps/ogg)

	message(STATUS "${PROJECT_NAME}: Fetching Vorbis...")
	add_subdirectory(deps/vorbis)
endFunction()