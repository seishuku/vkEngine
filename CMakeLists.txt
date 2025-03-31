cmake_minimum_required (VERSION 3.8)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

project("vkEngine" LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include("fetchDeps")
include("buildShaders")

fetchDeps()

set(PROJECT_SOURCES
	audio/audio.c
	audio/music.c
	audio/qoa.c
	audio/sfx.c
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
	network/client_network.c
	network/network.c
	physics/particle.c
	physics/physics.c
	physics/physicslist.c
	physics/spatialhash.c
	pipelines/composite.c
	pipelines/lighting.c
	pipelines/line.c
	pipelines/linegraph.c
	pipelines/shadow.c
	pipelines/skybox.c
	pipelines/sphere.c
	pipelines/volume.c
	system/memzone.c
	system/threads.c
	ui/bargraph.c
	ui/button.c
	ui/checkbox.c
	ui/cursor.c
	ui/sprite.c
	ui/ui.c
	utils/base64.c
	utils/config.c
	utils/event.c
	utils/list.c
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
	enemy.c
	engine.c
)

# change this for how many asteroids you want
add_definitions(-DNUM_ASTEROIDS=5000)
add_definitions(-DNUM_CUBE=30)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
	add_definitions(-D_DEBUG)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	include("buildWindows")
	buildWindows()
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
	include("buildLinux")
	buildLinux()
elseif(CMAKE_SYSTEM_NAME MATCHES "Android")
	include("buildAndroid")
	buildAndroid()
endif()

buildShaders()
