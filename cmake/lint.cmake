find_program(CPPCHECK cppcheck)
if(CPPCHECK STREQUAL CPPCHECK-NOTFOUND)
    message(WARNING "cppcheck not found, disabling check target")
else()
    add_custom_target(
        check
        COMMAND
            ${CPPCHECK}
            --error-exitcode=1 --quiet --force --language=c++ --std=c++20
            --enable=style,unusedFunction
            --inline-suppr --suppressions-list=cppcheck-suppressions.txt
            --relative-paths=${CMAKE_SOURCE_DIR}
            src tests
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking source code with cppcheck"
    )
endif()

set(Python3_FIND_UNVERSIONED_NAMES FIRST)
find_package(Python3 COMPONENTS Interpreter)
if(NOT Python3_FOUND)
    message(WARNING "Python3 not found, disabling lint and format targets")
else()
    set(VENV_DIR ${CMAKE_CURRENT_BINARY_DIR}/clang-format-venv)
    set(CLANG_FORMAT_EXE ${VENV_DIR}/bin/clang-format)
    set(CLANG_FORMAT_VERSION 18.1.3)
    add_custom_command(
        OUTPUT ${CLANG_FORMAT_EXE}
        COMMENT "Installing clang-format into Python venv"
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${VENV_DIR}
        COMMAND ${Python3_EXECUTABLE} -m venv ${VENV_DIR}
        COMMAND
            ${VENV_DIR}/bin/python3 -m pip install
            clang-format==${CLANG_FORMAT_VERSION}
    )
    add_custom_target(download-clang-format DEPENDS ${CLANG_FORMAT_EXE})

    set(
        CLANG_FORMAT_CMD
        ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/run-clang-format.py
        --clang-format-executable ${CLANG_FORMAT_EXE}
        --recursive
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )

    add_custom_target(
        lint
        DEPENDS download-clang-format
        COMMAND ${CLANG_FORMAT_CMD}
        COMMENT "Linting source code with clang-format"
    )
    if(TARGET check)
        add_custom_target(lint-all DEPENDS check lint)
    endif()

    add_custom_target(
        format
        DEPENDS download-clang-format
        COMMAND ${CLANG_FORMAT_CMD} --in-place
        COMMENT "Formatting source code with clang-format"
    )
endif()
