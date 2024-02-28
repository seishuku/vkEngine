cmake_minimum_required (VERSION 3.8)

include(FetchContent)

project ("vkEngine")

message(STATUS "${PROJECT_NAME}: Fetching OpenXR...")
FetchContent_Declare(openxr-loader
	GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
	GIT_SHALLOW TRUE
	GIT_TAG 288d3a7ebc1ad959f62d51da75baa3d27438c499 #1.0.34
 )
FetchContent_MakeAvailable(openxr-loader)

message(STATUS "${PROJECT_NAME}: Fetching PortAudio...")
FetchContent_Declare(portaudio
	GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
	GIT_SHALLOW TRUE
	GIT_TAG 147dd722548358763a8b649b3e4b41dfffbcfbb6 #v19.7.0
)
FetchContent_MakeAvailable(portaudio)

message(STATUS "${PROJECT_NAME}: Fetching OGG...")
FetchContent_Declare(ogg
	GIT_REPOSITORY https://github.com/xiph/ogg.git
	GIT_SHALLOW TRUE
#	GIT_TAG e1774cd77f471443541596e09078e78fdc342e4f #1.3.5
)
FetchContent_MakeAvailable(ogg)

message(STATUS "${PROJECT_NAME}: Fetching Vorbis...")
FetchContent_Declare(vorbis
	GIT_REPOSITORY https://github.com/xiph/vorbis.git
	GIT_SHALLOW TRUE
#	Apparently this commit won't properly find ogg with FetchContent, so just use whatever latest is.
#	GIT_TAG 0657aee69dec8508a0011f47f3b69d7538e9d262 #1.3.7
)
FetchContent_MakeAvailable(vorbis)

find_package(Vulkan REQUIRED COMPONENTS glslc)

set(SHADER_SOURCES
	shaders/bezier.frag shaders/bezier.geom shaders/bezier.vert
	shaders/threshold.frag shaders/gaussian.frag shaders/composite.frag shaders/fullscreen.vert
	shaders/shadow.vert
	shaders/skybox.frag shaders/skybox.vert
	shaders/lighting.frag shaders/lighting.vert
	shaders/particle.frag shaders/particle.geom shaders/particle.vert
	shaders/volume.frag shaders/volume.vert
	shaders/line.frag shaders/line.vert
	shaders/sphere.frag shaders/sphere.vert
	shaders/font.frag shaders/font.vert
	shaders/ui_sdf.frag shaders/ui_sdf.vert
)

find_program(GLSLC NAMES glslc HINTS Vulkan::glslc)

foreach(GLSL_SOURCE ${SHADER_SOURCES})
	set(SPRIV "${GLSL_SOURCE}.spv")
	
	add_custom_command(
		OUTPUT ${SPRIV}
		DEPENDS ${GLSL_SOURCE}
		COMMAND ${GLSLC} --target-env=vulkan1.1 -O ${CMAKE_SOURCE_DIR}/${GLSL_SOURCE} -o ${CMAKE_SOURCE_DIR}/${SPRIV}
	)

	list(APPEND SHADER_BINARIES ${SPRIV})
endforeach()

set(PROJECT_SOURCES
	audio/audio.c
	audio/wave.c
	camera/camera.c
	console/console.c
	font/font.c
	image/image.c
	image/qoi.c
	image/tga.c
	math/math.c
	math/matrix.c
	math/quat.c
	math/vec2.c
	math/vec3.c
	math/vec4.c
	model/bmodel.c
	network/network.c
	particle/particle.c
	physics/physics.c
	system/memzone.c
	system/threads.c
	system/win32.c
	ui/bargraph.c
	ui/button.c
	ui/checkbox.c
	ui/cursor.c
	ui/sprite.c
	ui/ui.c
	utils/event.c
	utils/input.c
	utils/list.c
	vr/vr.c
	vulkan/vulkan_buffer.c
	vulkan/vulkan_context.c
	vulkan/vulkan_descriptorset.c
	vulkan/vulkan_framebuffer.c
	vulkan/vulkan_instance.c
	vulkan/vulkan_mem.c
	vulkan/vulkan_pipeline.c
	vulkan/vulkan_renderpass.c
	vulkan/vulkan_swapchain.c
	client_network.c
	composite.c
	engine.c
	lighting.c
	line.c
	music.c
	nebula.c
	perframe.c
	sfx.c
	shadow.c
	skybox.c
	sphere.c
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
add_definitions(-DWIN32 -D_CRT_SECURE_NO_WARNINGS -D_CONSOLE)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
add_definitions(-DLINUX)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Android")
add_definitions(-DANDROID)
endif()

add_executable (${CMAKE_PROJECT_NAME} ${PROJECT_SOURCES})

add_custom_target(SHADERS DEPENDS ${SHADER_BINARIES})
add_dependencies(${CMAKE_PROJECT_NAME} SHADERS)

target_link_libraries(
    ${CMAKE_PROJECT_NAME} PUBLIC
	Vulkan::Vulkan
	OpenXR::openxr_loader
	portaudio
	Vorbis::vorbisfile
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC ws2_32.lib)
endif()

if(${CMAKE_CXX_COMPILER_ID} MATCHES "MSVC")
target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC	/experimental:c11atomics)
endif()

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET vkEngine PROPERTY C_STANDARD 11)
endif()
