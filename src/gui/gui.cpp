#include "gui.hpp"

#include "../old-gui.hpp"
#include "font.hpp"
#include "widgets/util.hpp"

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <optional>

namespace recap::labeller::gui {

namespace {

std::optional<std::string> get_custom_ini_path()
{
    const char *const env = std::getenv("RECAP_LABELLER_IMGUI_INI_PATH");
    if (env) {
        std::filesystem::path path(env);
        if (std::filesystem::is_directory(path)) {
            spdlog::error("Invalid ImGui INI path: '{}' is a directory", path);
        } else if (!std::filesystem::is_directory(path.parent_path())) {
            spdlog::error(
                "Invalid ImGui INI path: parent directory '{}' does not exist or not a directory",
                path.parent_path()
            );
        } else {
            std::string path_str = path.make_preferred().string();
            spdlog::info("Custom ImGui INI path: {}", path_str);
            return path_str;
        }

        spdlog::warn("Ignoring RECAP_LABELLER_IMGUI_INI_PATH, using default ImGui INI path");
    } else {
        spdlog::debug("RECAP_LABELLER_IMGUI_INI_PATH not set, using default ImGui INI path");
    }

    return std::nullopt;
}

}; // namespace

Gui::Gui(app::App& app) :
    m_app(app), old_gui(m_app.swallow_tasks(), m_app.annotation_manager(), m_app.data_dir(), false)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    auto custom_ini_path = get_custom_ini_path();
    if (custom_ini_path) {
        m_ini_path = std::move(*custom_ini_path);
        io.IniFilename = m_ini_path->c_str();
    }

    setup_fonts();
}

Gui::~Gui()
{
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

void Gui::stop()
{
    spdlog::info("GUI close requested");
    m_stop_requested = true;
    m_ready_to_stop = true;
}

static const char *const SidebarWindowId = "##sidebar";
static const char *const MainWindowId = "##mainwindow";

void Gui::draw()
{
    constexpr ImGuiWindowFlags WindowFlags =
        (ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove
         | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

    if (ImGui::BeginMainMenuBar()) {
        draw_menu_bar();
        ImGui::EndMainMenuBar();
    }
    setup_dockspace();

    if (ImGui::Begin(SidebarWindowId, nullptr, WindowFlags)) {
        ImGui::End();
    }

    if (ImGui::Begin(MainWindowId, nullptr, WindowFlags)) {
        draw_main_window();
        ImGui::End();
    }

    if (m_show_imgui_demo_window) {
        ImGui::ShowDemoWindow(&m_show_imgui_demo_window);
    }
    if (m_show_imgui_metrics) {
        ImGui::ShowMetricsWindow(&m_show_imgui_metrics);
    }
    if (m_show_implot_demo_window) {
        ImPlot::ShowDemoWindow(&m_show_implot_demo_window);
    }

    m_first_draw = false;
}

void Gui::setup_dockspace() const
{
    constexpr ImGuiDockNodeFlags DockspaceFlags =
        (ImGuiDockNodeFlags_NoUndocking | ImGuiDockNodeFlags_AutoHideTabBar
         | ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoTabBar);

    // If the dockspace ID already exists, the the node sizes are already set in imgui.ini
    ImGuiID id = ImGui::GetID("##dockspace");
    if (m_first_draw && ImGui::DockBuilderGetNode(id) == nullptr) [[unlikely]] {
        ImGui::DockSpaceOverViewport(id, ImGui::GetMainViewport(), DockspaceFlags);
        // The following is adapted from
        // https://gist.github.com/AidanSun05/953f1048ffe5699800d2c92b88c36d9f
        spdlog::debug("Setting up dockspace");
        ImGui::DockBuilderRemoveNode(id);
        ImGui::DockBuilderAddNode(id);
        constexpr float SidebarRatio = 0.2;

        ImGuiID dock_sidebar = 1;
        ImGuiID dock_main = 2;
        ImGui::DockBuilderSplitNode(id, ImGuiDir_Left, SidebarRatio, &dock_sidebar, &dock_main);

        ImGui::DockBuilderDockWindow(SidebarWindowId, dock_sidebar);
        ImGui::DockBuilderDockWindow(MainWindowId, dock_main);
        ImGui::DockBuilderFinish(id);
    } else {
        ImGui::DockSpaceOverViewport(id, ImGui::GetMainViewport(), DockspaceFlags);
    }
}

void Gui::draw_main_window()
{
    if (m_show_debug_info) {
        draw_debug_info();
    }
}

void Gui::set_scaling_factor(float scaling_factor)
{
    ImGui::GetStyle().ScaleAllSizes(scaling_factor);
}

void Gui::draw_menu_bar()
{
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Quit", "Alt+F4")) {
            stop();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        m_color_scheme_selector.draw();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools")) {
        if (ImGui::MenuItem("ImGui demo...") && !m_show_imgui_demo_window) {
            m_show_imgui_demo_window = true;
        }
        if (ImGui::MenuItem("Debug/metrics...") && !m_show_imgui_metrics) {
            spdlog::debug("Opening ImGui debug/metrics window");
            m_show_imgui_metrics = true;
        }
        if (ImGui::MenuItem("ImPlot demo...") && !m_show_implot_demo_window) {
            m_show_implot_demo_window = true;
        }
        ImGui::MenuItem("Show debug info", nullptr, &m_show_debug_info);
        ImGui::EndMenu();
    }
}

void Gui::draw_debug_info()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text(
        ICON_FA_GEAR
        "  Mouse Position: [%.0f,%.0f]. Application average: %.3f ms/frame (%.1f FPS).",
        io.MousePos.x,
        io.MousePos.y,
        1000.0f / io.Framerate,
        io.Framerate
    );
}
}; // namespace recap::labeller::gui
