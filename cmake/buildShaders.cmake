function("buildShaders")
	set(SHADER_SOURCES
		shaders/bezier.frag shaders/bezier.geom shaders/bezier.vert
		shaders/threshold.frag shaders/threshold.vert
		shaders/gaussian.frag shaders/gaussian.vert
		shaders/composite.frag shaders/composite.vert
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

	file(GLOB PIPELINE_FILES "${PROJECT_SOURCE_DIR}/pipelines/*.pipeline")

	# Create a directory for phony output markers
	set(PIPELINE_MARKERS_DIR "${CMAKE_BINARY_DIR}/pipeline_markers")
	file(MAKE_DIRECTORY ${PIPELINE_MARKERS_DIR})

	foreach(PIPELINE_FILE ${PIPELINE_FILES})
		get_filename_component(FILENAME ${PIPELINE_FILE} NAME)
		set(MARKER_FILE "${PIPELINE_MARKERS_DIR}/${FILENAME}.processed")

		add_custom_command(
			OUTPUT ${MARKER_FILE}
			COMMAND ${PYTHON_EXECUTABLE} ${PROJECT_SOURCE_DIR}/embedPipelineShaders.py ${PIPELINE_FILE} --shader-path ${PROJECT_SOURCE_DIR}/shaders
			COMMAND ${CMAKE_COMMAND} -E touch ${MARKER_FILE}
			DEPENDS ${PROJECT_SOURCE_DIR}/embedPipelineShaders.py ${PIPELINE_FILE}
			COMMENT "Embedding shaders into ${PIPELINE_FILE}..."
		)

		list(APPEND PROCESSED_FILES ${MARKER_FILE})
	endforeach()

	# Add a custom target to process all files
	add_custom_target(process_pipelines ALL DEPENDS ${PROCESSED_FILES})
endFunction()