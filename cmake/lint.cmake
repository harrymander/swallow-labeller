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

find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT STREQUAL CLANG_FORMAT-NOTFOUND)
    message(WARNING "clang-format not found, disabling lint and format targets")
else()
    set(Python3_FIND_UNVERSIONED_NAMES FIRST)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(
        CLANG_FORMAT_CMD
        ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/run-clang-format.py
        --recursive
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )

    add_custom_target(
        lint
        COMMAND ${CLANG_FORMAT_CMD}
        COMMENT "Linting source code with clang-format"
    )
    if(TARGET check)
        add_custom_target(lint-all DEPENDS check lint)
    endif()

    add_custom_target(
        format
        COMMAND ${CLANG_FORMAT_CMD} --in-place
        COMMENT "Formatting source code with clang-format"
    )
endif()
