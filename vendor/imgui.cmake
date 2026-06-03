include(FetchContent)
fetchcontent_declare(
    imgui
    # v1.92.8 docking branch
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG b61e56346a92cfcaf1f43a545ca37b0b32239654
)
fetchcontent_makeavailable(imgui)

add_library(imgui-core INTERFACE)
target_sources(imgui-core INTERFACE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
)
target_include_directories(imgui-core INTERFACE
    ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/misc/cpp
)
add_library(imgui::imgui ALIAS imgui-core)

add_library(imgui-demo INTERFACE)
target_sources(imgui-demo INTERFACE ${imgui_SOURCE_DIR}/imgui_demo.cpp)
add_library(imgui::demo ALIAS imgui-demo)

set(backends ${imgui_SOURCE_DIR}/backends)

if(LINUX)
    set(_imgui_backend_glfw_opengl_opt_default ON)
else()
    set(_imgui_backend_glfw_opengl_opt_default OFF)
endif()
option(
    IMGUI_BACKEND_GLFW_OPENGL
    "Enable GLFW/OpenGL ImGui backend"
    ${_imgui_backend_glfw_opengl_opt_default}
)
if(IMGUI_BACKEND_GLFW_OPENGL)
    find_package(OpenGL REQUIRED)
    add_library(imgui-opengl INTERFACE)
    target_sources(
        imgui-opengl INTERFACE ${backends}/imgui_impl_opengl3.cpp
    )
    target_link_libraries(imgui-opengl INTERFACE OpenGL::GL)
    add_library(imgui::backend::opengl ALIAS imgui-opengl)
    set(IMGUI_BACKEND_OPENGL_FOUND TRUE PARENT_SCOPE)

    find_package(glfw3 3.3 REQUIRED)
    add_library(imgui-glfw INTERFACE)
    target_sources(imgui-glfw INTERFACE ${backends}/imgui_impl_glfw.cpp)
    target_link_libraries(imgui-glfw INTERFACE glfw)
    add_library(imgui::backend::glfw ALIAS imgui-glfw)
    set(IMGUI_BACKEND_GLFW_FOUND TRUE PARENT_SCOPE)
endif()

if(APPLE)
    add_library(imgui-macos-native INTERFACE)
    target_sources(imgui-macos-native INTERFACE
        ${backends}/imgui_impl_osx.mm
        ${backends}/imgui_impl_metal.mm
    )
    target_link_libraries(imgui-macos-native INTERFACE
        "-framework Cocoa"
        "-framework GameController"
        "-framework Metal"
        "-framework MetalKit"
    )
    add_library(imgui::backend::macos-native ALIAS imgui-macos-native)
endif()

if(WIN32 AND MSVC)
    add_library(imgui-win32-native INTERFACE)
    target_sources(imgui-win32-native INTERFACE
        ${backends}/imgui_impl_dx12.cpp
        ${backends}/imgui_impl_win32.cpp
    )
    target_link_libraries(imgui-win32-native INTERFACE
        d3d12.lib dxgi.lib d3dcompiler.lib Shcore.lib
    )
    target_compile_definitions(imgui-win32-native INTERFACE ImTextureID=ImU64)
    add_library(imgui::backend::win32-native ALIAS imgui-win32-native)
endif()

add_executable(
    imgui-binary-to-compressed-c EXCLUDE_FROM_ALL
    binary_to_compressed_c.cpp
)
macro(imgui_compressed_c_array INPUT OUTPUT ARRAY_NAME)
    string(MAKE_C_IDENTIFIER ${ARRAY_NAME} c_symbol)
    get_filename_component(dirname ${OUTPUT} DIRECTORY)
    add_custom_command(
        OUTPUT ${OUTPUT}
        DEPENDS imgui-binary-to-compressed-c ${INPUT}
        MAIN_DEPENDENCY ${INPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${dirname}
        COMMAND imgui-binary-to-compressed-c -extern
                ${INPUT} ${c_symbol} > ${OUTPUT}
        COMMENT "Generating compressed C array '${c_symbol}' from ${INPUT}"
    )
endmacro()
