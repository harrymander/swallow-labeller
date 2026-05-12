find_program(UV uv REQUIRED)

add_custom_target(
    check
    COMMENT "Checking source code with cppcheck"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND
        ${UV} run cppcheck
        --error-exitcode=1 --quiet --force --language=c++ --std=c++20
        --enable=style
        --check-level=exhaustive
        --inline-suppr --suppressions-list=cppcheck-suppressions.txt
        --relative-paths=${CMAKE_SOURCE_DIR}
        src tests
    USES_TERMINAL
)

set(VENV_DIR ${CMAKE_SOURCE_DIR}/.venv)
if(WIN32)
    set(CLANG_FORMAT_EXE ${VENV_DIR}/Scripts/clang-format.exe)
else()
    set(CLANG_FORMAT_EXE ${VENV_DIR}/bin/clang-format)
endif()

set(
    CLANG_FORMAT_CMD
    ${UV} run ${CMAKE_SOURCE_DIR}/scripts/run-clang-format.py
    --clang-format-executable ${CLANG_FORMAT_EXE}
    --recursive
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)
add_custom_target(
    lint
    DEPENDS ${CLANG_FORMAT_EXE}
    COMMENT "Linting source code with clang-format"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ${CLANG_FORMAT_CMD}
    USES_TERMINAL
)
add_custom_target(
    format
    DEPENDS ${CLANG_FORMAT_EXE}
    COMMENT "Formatting source code with clang-format"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ${CLANG_FORMAT_CMD} --in-place
    USES_TERMINAL
)

add_custom_target(lint-all DEPENDS check lint)
