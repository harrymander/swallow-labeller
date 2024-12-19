# Generates a small amount of demo data, including corrupted and missing data
# files

set(Python3_FIND_UNVERSIONED_NAMES FIRST)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(DEMO_GEN_PY ${CMAKE_CURRENT_SOURCE_DIR}/gen-demo.py)

set(DEMO_DIR ${CMAKE_CURRENT_BINARY_DIR}/demofiles)
set_property(
    DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    APPEND
    PROPERTY ADDITIONAL_CLEAN_FILES demofiles
)

set(DEMO_TASKS_JSON ${DEMO_DIR}/tasks.json)
set(DEMO_SUGGESTIONS_JSON ${DEMO_DIR}/suggested-annotations.json)
set(DEMO_DATA_DIR ${DEMO_DIR}/data)
set(DEMO_ANNOTATIONS_JSON ${DEMO_DIR}/demo-annotations.json)
set(DEMO_LOG_FILE ${DEMO_DIR}/log.txt)

set(DEMO_OUTPUTS ${DEMO_TASKS_JSON} ${DEMO_SUGGESTIONS_JSON})
add_custom_command(
    OUTPUT ${DEMO_OUTPUTS}
    DEPENDS ${DEMO_GEN_PY}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${DEMO_DATA_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DEMO_DIR} ${DEMO_DATA_DIR}
    COMMAND
        ${Python3_EXECUTABLE} ${DEMO_GEN_PY}
        --tasks-json ${DEMO_TASKS_JSON}
        --suggestions-json ${DEMO_SUGGESTIONS_JSON}
        --data-dir ${DEMO_DATA_DIR}
    COMMENT "Generating demo tasks"
)

add_custom_target(demo-files DEPENDS ${DEMO_OUTPUTS})
add_custom_target(
    run-demo
    DEPENDS recap-labeller demo-files
    COMMAND
        recap-labeller
        --tasks ${DEMO_TASKS_JSON}
        --data-dir ${DEMO_DATA_DIR}
        --annotations ${DEMO_ANNOTATIONS_JSON}
        --suggestions ${DEMO_SUGGESTIONS_JSON}
        --log ${DEMO_LOG_FILE}
        --no-app-data-dir
    WORKING_DIRECTORY ${DEMO_DIR}
    COMMENT
"\nRunning demo:
  Tasks JSON: ${DEMO_TASKS_JSON}
  Data dir: ${DEMO_DATA_DIR}
  Suggested annotations JSON: ${DEMO_SUGGESTIONS_JSON}
  Annotations JSON: ${DEMO_ANNOTATIONS_JSON}
  Log file: ${DEMO_LOG_FILE}"
)
