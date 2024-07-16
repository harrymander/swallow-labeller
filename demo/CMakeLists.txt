# Generates a small amount of demo data, including corrupted and missing data
# files

set(Python3_FIND_UNVERSIONED_NAMES FIRST)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(DEMO_GEN_PY ${CMAKE_CURRENT_SOURCE_DIR}/gen-demo.py)

set(DEMO_DIR ${CMAKE_CURRENT_BINARY_DIR}/demofiles)
set(DEMO_TASKS_JSON ${DEMO_DIR}/tasks.json)
set(DEMO_DATA_DIR ${DEMO_DIR}/data)
set(DEMO_ANNOTATIONS_JSON ${DEMO_DIR}/demo-annotations.json)

add_custom_command(
    OUTPUT ${DEMO_TASKS_JSON}
    DEPENDS ${DEMO_GEN_PY}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${DEMO_DATA_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DEMO_DIR} ${DEMO_DATA_DIR}
    COMMAND
        ${Python3_EXECUTABLE} ${DEMO_GEN_PY}
        --tasks-json ${DEMO_TASKS_JSON}
        --data-dir ${DEMO_DATA_DIR}
    COMMENT "Generating demo tasks"
)

add_custom_target(demo-files DEPENDS ${DEMO_TASKS_JSON})
add_custom_target(
    run-demo
    DEPENDS recap-labeller demo-files
    COMMAND
        recap-labeller
        --tasks ${DEMO_TASKS_JSON}
        --data-dir ${DEMO_DATA_DIR}
        --annotations ${DEMO_ANNOTATIONS_JSON}
    WORKING_DIRECTORY ${DEMO_DIR}
    COMMENT
"\nRunning demo:
  Tasks JSON: ${DEMO_TASKS_JSON}
  Data dir: ${DEMO_DATA_DIR}
  Annotations JSON: ${DEMO_ANNOTATIONS_JSON}"
)
