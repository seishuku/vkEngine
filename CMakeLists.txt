cmake_minimum_required (VERSION 3.8)

include(FetchContent)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

project ("vkEngine" LANGUAGES C)

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

set(SHADER_SOURCES
	shaders/bezier.frag shaders/bezier.geom shaders/bezier.vert
	shaders/threshold.frag shaders/gaussian.frag shaders/composite.frag shaders/fullscreen.vert
	shaders/shadow.vert
	shaders/skybox.frag shaders/skybox.vert
	shaders/lighting.frag shaders/lighting.vert
	shaders/particle.frag shaders/particle.geom shaders/particle.vert
	shaders/volume_gen.comp
	shaders/volume.frag shaders/volume.vert
	shaders/line.frag shaders/line.vert
	shaders/sphere.frag shaders/sphere.vert
	shaders/font.frag shaders/font.vert
	shaders/ui_sdf.frag shaders/ui_sdf.vert
)

find_package(Vulkan COMPONENTS glslc)
find_program(GLSLC NAMES glslc HINTS Vulkan::glslc)

foreach(SOURCE ${SHADER_SOURCES})
	set(SHADER_OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE}.spv)
	set(SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE})
	add_custom_command(
		OUTPUT ${SHADER_OUTPUT}
		DEPENDS ${SOURCE}
		COMMAND ${GLSLC} --target-env=vulkan1.1 -O -o ${SHADER_OUTPUT} ${SHADER_SOURCE}
	)
	list(APPEND SHADER_BINARIES ${SHADER_OUTPUT})
endforeach()

add_custom_target(ShaderCompilation DEPENDS ${SHADER_BINARIES})

set(PROJECT_SOURCES
	audio/audio.c
	audio/qoa.c
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
	ui/bargraph.c
	ui/button.c
	ui/checkbox.c
	ui/cursor.c
	ui/sprite.c
	ui/ui.c
	utils/event.c
	utils/list.c
	utils/lz4.c
	utils/pipeline.c
	utils/spvparse.c
	utils/tokenizer.c
	vr/vr.c
	vulkan/vulkan_buffer.c
	vulkan/vulkan_context.c
	vulkan/vulkan_descriptorset.c
	vulkan/vulkan_framebuffer.c
	vulkan/vulkan_heaps.c
	vulkan/vulkan_instance.c
	vulkan/vulkan_mem.c
	vulkan/vulkan_pipeline.c
	vulkan/vulkan_renderpass.c
	vulkan/vulkan_swapchain.c
	client_network.c
	composite.c
	enemy.c
	engine.c
	lighting.c
	line.c
	music.c
	volume.c
	perframe.c
	sfx.c
	shadow.c
	skybox.c
	sphere.c
)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
	add_definitions(-D_DEBUG)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	add_definitions(-DWIN32 -D_CRT_SECURE_NO_WARNINGS -D_CONSOLE)
	list(APPEND PROJECT_SOURCES system/win32.c)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
	add_definitions(-DLINUX -g)
	list(APPEND PROJECT_SOURCES system/linux_x11.c)
elseif(CMAKE_SYSTEM_NAME MATCHES "Android")
	add_definitions(-DANDROID)
	list(APPEND PROJECT_SOURCES
		system/android_fopen.c
		system/android_main.c
		system/android_native_app_glue.c
	)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        add_compile_options(
			"-march=x86-64-v3"
			"-ggdb3"
			"-fprofile-instr-generate"
			"/PROFILE"
		)
    else()
        message(WARNING "Unknown CPU architecture ${CMAKE_SYSTEM_PROCESSOR} not targeted.")
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
        add_compile_options("/arch:AVX2")
        else()
            message(WARNING "Unknown CPU architecture ${CMAKE_SYSTEM_PROCESSOR} not targeted.")
    endif()
endif()

add_executable(${CMAKE_PROJECT_NAME} ${PROJECT_SOURCES})
add_dependencies(${CMAKE_PROJECT_NAME} ShaderCompilation)

target_link_libraries(
    ${CMAKE_PROJECT_NAME} PUBLIC
	Vulkan::Vulkan
	OpenXR::openxr_loader
	Vorbis::vorbisfile
)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC portaudio_static ws2_32.lib xinput.lib)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC m portaudio_static X11 Xi Xfixes)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Android")
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC m android log aaudio)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC /experimental:c11atomics)
endif()
