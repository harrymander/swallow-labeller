if(FETCH_GTEST)
    include(FetchContent)
    fetchcontent_declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG b796f7d44681514f58a683a3a71ff17c94edb0c1
    )
    if(WIN32)
        # Prevent overriding the parent project's compiler/linker settings
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    endif()
    fetchcontent_getproperties(googletest)
    if(NOT googletest_POPULATED)
        fetchcontent_populate(googletest)
        add_subdirectory(
            ${googletest_SOURCE_DIR} ${googletest_BINARY_DIR}
            EXCLUDE_FROM_ALL
        )
    endif()
    include(GoogleTest)
else()
    find_package(GTest)
    if(NOT GTest_FOUND)
        message(
            FATAL_ERROR
            "GTest not found - install or re-configure with FETCH_GTEST=on"
        )
    endif()
endif()

set(Python3_FIND_UNVERSIONED_NAMES FIRST)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(DATA_TEST_FILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/data_test_files)
set(DATA_TEST_FILE_HEADER ${DATA_TEST_FILE_DIR}/test-data-paths.h)
file(MAKE_DIRECTORY ${DATA_TEST_FILE_DIR})
add_custom_command(
    OUTPUT ${DATA_TEST_FILE_HEADER}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/generate_test_npy.py
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_test_npy.py
            --header ${DATA_TEST_FILE_HEADER}
            ${DATA_TEST_FILE_DIR}
    COMMENT "Generating test Numpy data files"
)

add_executable(test-data test-data.cpp ../src/data.cpp ${DATA_TEST_FILE_HEADER})
target_include_directories(test-data PRIVATE ../src ${DATA_TEST_FILE_DIR})
target_link_libraries(test-data PRIVATE GTest::gtest_main cnpy::cnpy)
gtest_discover_tests(test-data)

add_executable(
    test-tasks-parsing
    test-tasks-parsing.cpp
    ../src/labelling-task.cpp
)
target_include_directories(test-tasks-parsing PRIVATE ../src)
target_link_libraries(
    test-tasks-parsing
    PRIVATE GTest::gtest_main nlohmann_json::nlohmann_json
)
set(TEST_JSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tasks)
target_compile_definitions(
    test-tasks-parsing
    PRIVATE
        JSON_PATH="${TEST_JSON_DIR}/swallow-tasks.json"
        INVALID_JSON_PATH="${TEST_JSON_DIR}/invalid.json"
        MISSING_FIELD_JSON_PATH="${TEST_JSON_DIR}/swallow-missing-field.json"
        ANNOTATIONS_JSON_PATH="${CMAKE_CURRENT_SOURCE_DIR}/annotations.json"
)
gtest_discover_tests(test-tasks-parsing)

