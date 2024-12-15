function("buildShaders")
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
		shaders/linegraph.frag shaders/linegraph.vert
		shaders/sphere.frag shaders/sphere.vert
		shaders/font.frag shaders/font.vert
		shaders/ui_sdf.frag shaders/ui_sdf.vert
	)

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
endFunction()