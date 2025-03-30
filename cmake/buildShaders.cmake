function(buildShaders)
    file(GLOB PIPELINE_FILES "${PROJECT_SOURCE_DIR}/pipelines/*.pipeline")

    # Possible supported shader stages
    set(STAGE_EXTENSIONS vert tesc tese geom frag comp)

    set(PROCESSED_FILES "")

    foreach(PIPELINE_FILE ${PIPELINE_FILES})
        get_filename_component(FILENAME ${PIPELINE_FILE} NAME_WE)

        # Collect shader dependencies
        set(PIPELINE_SHADER_DEPENDENCIES "")

        foreach(STAGE_EXT ${STAGE_EXTENSIONS})
            set(SHADER_FILE "${PROJECT_SOURCE_DIR}/shaders/${FILENAME}.${STAGE_EXT}")
            set(SHADER_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${FILENAME}.${STAGE_EXT}.spv")

            if(EXISTS ${SHADER_FILE})
                list(APPEND PIPELINE_SHADER_DEPENDENCIES ${SHADER_OUTPUT})

                add_custom_command(
                    OUTPUT ${SHADER_OUTPUT}
                    DEPENDS ${SHADER_FILE}
                    COMMAND ${GLSLC} --target-env=vulkan1.1 -O -o ${SHADER_OUTPUT} ${SHADER_FILE}
                    COMMENT "Compiling ${SHADER_FILE} -> ${SHADER_OUTPUT}"
                )
            endif()
        endforeach()

        set(PIPELINE_MARKERS_DIR "${CMAKE_BINARY_DIR}/pipeline_markers")
        file(MAKE_DIRECTORY ${PIPELINE_MARKERS_DIR})
        set(MARKER_FILE "${PIPELINE_MARKERS_DIR}/${FILENAME}.processed")

        add_custom_command(
            OUTPUT ${MARKER_FILE}
            COMMAND ${PYTHON_EXECUTABLE} ${PROJECT_SOURCE_DIR}/embedPipelineShaders.py ${PIPELINE_FILE} --shader-path ${CMAKE_BINARY_DIR}/shaders
            COMMAND ${CMAKE_COMMAND} -E touch ${MARKER_FILE}
            DEPENDS ${PROJECT_SOURCE_DIR}/embedPipelineShaders.py ${PIPELINE_FILE} ${PIPELINE_SHADER_DEPENDENCIES}
            COMMENT "Embedding shaders into ${PIPELINE_FILE}..."
        )

        list(APPEND PROCESSED_FILES ${MARKER_FILE})
    endforeach()

    # Add a custom target to process all pipelines
    add_custom_target(ProcessPipelines ALL DEPENDS ${PROCESSED_FILES})
    add_dependencies(${CMAKE_PROJECT_NAME} ProcessPipelines)
endfunction()
