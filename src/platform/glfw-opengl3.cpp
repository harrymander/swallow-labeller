// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs,
// OpenGL/Vulkan/Metal graphics context creation, etc.) If you are new to Dear ImGui, read
// documentation from the docs/ folder + read the top of imgui.cpp. Read online:
// https://github.com/ocornut/imgui/tree/master/docs

#include "app/annotation-store.hpp"
#include "gui/gui.hpp"
#include "models/task-info.hpp"
#include "options.h"
#include "platform.hpp"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>

#include <vector>

#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif

#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <spdlog/spdlog.h>

#include <cerrno>
#include <csignal>
#include <cstring>

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of
// testing and compatibility with old VS compilers. To link with VS2010-era libraries, VS2015+
// requires linking with legacy_stdio_definitions.lib, which we do using this pragma. Your own
// project should not be affected, as you are likely to link with a newer binary of GLFW that is
// adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

namespace recap::labeller::platform {

namespace {

void glfw_error_callback(int error, const char *description)
{
    spdlog::error("GLFW error {}: {}\n", error, description);
}

volatile std::sig_atomic_t signal_stop = 0;

void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        signal_stop = 1;
    }
}

int setup_stop_signal_handler()
{
    static struct ::sigaction sigaction{};

    ::sigemptyset(&sigaction.sa_mask);
    sigaction.sa_handler = signal_handler;
    if (::sigaction(SIGINT, &sigaction, NULL)) {
        spdlog::critical("error setting up SIGINT handler: {}", std::strerror(errno));
        return -1;
    }
    if (::sigaction(SIGTERM, &sigaction, NULL)) {
        spdlog::critical("error setting up SIGTERM handler: {}", std::strerror(errno));
        return -1;
    }

    return 0;
}

void run_gui(gui::Gui& gui, const char *glsl_version, GLFWwindow *window)
{
    // If time between two consecutive stop/interrupt signals signals (e.g. from Ctrl-C) is less
    // than this value, force quit (regardless of whether GUI is ready).
    constexpr double ForceQuitIntervalSeconds = 1;

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Main loop
    double last_stop_signal_time = -1;
    bool running = true;
    spdlog::debug("Running main loop...");
    while (running) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui
        // wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main
        // application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main
        // application, or clear/overwrite your copy of the keyboard data. Generally you may always
        // pass all inputs to dear imgui, and hide them from your application based on those two
        // flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        gui.draw();

        bool request_stop = true;
        if (signal_stop) {
            signal_stop = 0;
            spdlog::info("Received stop/interrupt signal");
            double current_time = glfwGetTime();
            if (last_stop_signal_time >= 0
                && current_time - last_stop_signal_time < ForceQuitIntervalSeconds)
            {
                spdlog::warn("Two stop signals received. Force exiting!");
                break;
            }
            last_stop_signal_time = current_time;
        } else if (glfwWindowShouldClose(window)) {
            glfwSetWindowShouldClose(window, 0);
            spdlog::info("Window close requested");
        } else {
            request_stop = false;
        }

        if (request_stop) {
            gui.stop();
        }
        running = !gui.ready_to_stop();
        if (request_stop && running) {
            spdlog::warn("Exit request received, but GUI is blocking exit");
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        constexpr ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        glClearColor(
            clear_color.x * clear_color.w,
            clear_color.y * clear_color.w,
            clear_color.z * clear_color.w,
            clear_color.w
        );
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
}

}; // namespace

int run(
    const std::vector<models::SwallowTaskInfo>& tasks,
    SwallowAnnotationStore& annotation_store,
    const std::filesystem::path& data_dir
)
{
    if (setup_stop_signal_handler() < 0) {
        return 1;
    }

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        spdlog::critical("Error initialising GLFW");
        return 1;
    }
    spdlog::debug("Initialised GLFW");

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char *glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char *glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    // Create window with graphics context
    GLFWwindow *window = glfwCreateWindow(1280, 720, PROJECT_NAME " v" PROJECT_VERSION, NULL, NULL);
    if (window == NULL) {
        spdlog::critical("Error creating GLFW window");
        return 1;
    }
    spdlog::debug("Created GLFW window");
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    gui::Gui gui(tasks, annotation_store, data_dir);
    run_gui(gui, glsl_version, window);

    spdlog::debug("Destroying GLFW window...");
    glfwDestroyWindow(window);
    spdlog::debug("Terminating GLFW session...");
    glfwTerminate();

    return 0;
}

}; // namespace recap::labeller::platform
