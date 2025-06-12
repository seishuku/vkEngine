function("buildLinux")
	find_package(PipeWire REQUIRED)
	list(APPEND PROJECT_SOURCES audio/backend/pipewire.c)

	if(WAYLAND)
		add_definitions(-DLINUX -DWAYLAND -g)
		list(APPEND PROJECT_SOURCES system/linux/linux_wayland.c system/linux/wayland/xdg-shell.c system/linux/wayland/relative-pointer.c system/linux/wayland/pointer-constraints.c)
	else()
		add_definitions(-DLINUX -g)
		list(APPEND PROJECT_SOURCES system/linux/linux_x11.c)
	endif()

	if(CMAKE_C_COMPILER_ID OR CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
		if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
			add_compile_options("-march=x86-64-v3")
		else()
			message(WARNING "Unknown CPU architecture ${CMAKE_SYSTEM_PROCESSOR} not targeted.")
		endif()
	endif()

	add_executable(${CMAKE_PROJECT_NAME} ${PROJECT_SOURCES})

	target_link_libraries(
		${CMAKE_PROJECT_NAME} PUBLIC
		Vulkan::Vulkan
		OpenXR::openxr_loader
		Vorbis::vorbisfile
		PipeWire::PipeWire
		m
	)

	if(WAYLAND)
		target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC xkbcommon wayland-client)
	else()
		target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC X11 Xi Xfixes)
	endif()
endFunction()
