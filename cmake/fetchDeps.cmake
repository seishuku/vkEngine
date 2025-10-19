include(FetchContent)

function(fetchDeps)
	find_package(Python COMPONENTS Interpreter)

	find_package(Vulkan COMPONENTS glslc)
	find_program(GLSLC NAMES glslc HINTS Vulkan::glslc)

	set(BUILD_WITH_SYSTEM_JSONCPP OFF CACHE BOOL "" FORCE)

	message(STATUS "${PROJECT_NAME}: Fetching OpenXR...")
	add_subdirectory(deps/OpenXR-SDK EXCLUDE_FROM_ALL)

	set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
	set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

	message(STATUS "${PROJECT_NAME}: Fetching OGG...")
	add_subdirectory(deps/ogg EXCLUDE_FROM_ALL)

	message(STATUS "${PROJECT_NAME}: Fetching Vorbis...")
	add_subdirectory(deps/vorbis EXCLUDE_FROM_ALL)
endFunction()
