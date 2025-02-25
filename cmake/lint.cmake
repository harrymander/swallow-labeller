set(Python3_FIND_UNVERSIONED_NAMES FIRST)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(CLANG_FORMAT_VERSION 18.1.3)
set(CPPCHECK_VERSION 1.4.7)  # equivalent to cppcheck 2.16.1

set(VENV_DIR ${CMAKE_CURRENT_BINARY_DIR}/clang-format-venv)
if(WIN32)
    set(VENV_PYTHON ${VENV_DIR}/Scripts/python.exe)
    set(CLANG_FORMAT_EXE ${VENV_DIR}/Scripts/clang-format.exe)
    set(CPPCHECK_EXE ${VENV_DIR}/Scripts/cppcheck.exe)
else()
    set(VENV_PYTHON ${VENV_DIR}/bin/python3)
    set(CLANG_FORMAT_EXE ${VENV_DIR}/bin/clang-format)
    set(CPPCHECK_EXE ${VENV_DIR}/bin/cppcheck)
endif()

add_custom_command(
    OUTPUT ${VENV_PYTHON}
    COMMENT "Installing virtualenv for linters"
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${VENV_DIR}
    COMMAND ${Python3_EXECUTABLE} -m venv ${VENV_DIR}
)
add_custom_command(
    OUTPUT ${CPPCHECK_EXE}
    DEPENDS ${VENV_PYTHON}
    COMMENT "Installing cppcheck into Python venv"
    COMMAND ${VENV_PYTHON} -m pip install cppcheck==${CPPCHECK_VERSION}
)
add_custom_command(
    OUTPUT ${CLANG_FORMAT_EXE}
    DEPENDS ${VENV_PYTHON}
    COMMENT "Installing clang-format into Python venv"
    COMMAND ${VENV_PYTHON} -m pip install clang-format==${CLANG_FORMAT_VERSION}
)

add_custom_target(
    check
    DEPENDS ${CPPCHECK_EXE}
    COMMENT "Checking source code with cppcheck"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND
        ${CPPCHECK_EXE}
        --error-exitcode=1 --quiet --force --language=c++ --std=c++20
        --enable=style
        --check-level=exhaustive
        --inline-suppr --suppressions-list=cppcheck-suppressions.txt
        --relative-paths=${CMAKE_SOURCE_DIR}
        src tests
)

set(
    CLANG_FORMAT_CMD
    ${VENV_PYTHON} ${CMAKE_SOURCE_DIR}/scripts/run-clang-format.py
    --clang-format-executable ${CLANG_FORMAT_EXE}
    --recursive
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)
add_custom_target(
    lint
    DEPENDS ${CLANG_FORMAT_EXE}
    COMMENT "Linting source code with clang-format"
    COMMAND ${CLANG_FORMAT_CMD}
)
add_custom_target(
    format
    DEPENDS ${CLANG_FORMAT_EXE}
    COMMENT "Formatting source code with clang-format"
    COMMAND ${CLANG_FORMAT_CMD} --in-place
)

add_custom_target(lint-all DEPENDS check lint)
