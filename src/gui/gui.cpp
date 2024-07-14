#include "gui.hpp"

#include "../variant-visitor.hpp"
#include "font.hpp"

#include <IconsFontAwesome6.h>
#include <fmt/format.h>
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

Gui::Gui(app::App& app) : m_app(app)
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
        draw_task_list();
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

    const auto& task = m_app.tasks()[m_app.active_task_index()];
    const auto& info = task.info();
    ImGui::Text(
        "Subject #%u, %s swallows, repeat #%u, swallow #%u (%s)",
        info.subject,
        swallow_test_type_string(info.test_type).c_str(),
        info.repeatnum,
        info.swallownum,
        task.data_path().c_str()
    );

    VariantVisitor{
        [this](const app::SwallowLabellingTaskError& error) {
            ImGui::Text(ICON_FA_TRIANGLE_EXCLAMATION "  %s", error.message.c_str());
            if (ImGui::Button("Retry...")) {
                m_app.reload_active_task();
            }
        },
        [this](app::SwallowLabellingTaskManager& task_manager) { draw_active_task(task_manager); },
    }(m_app.active_task());
}

void Gui::draw_active_task(app::SwallowLabellingTaskManager& task_manager) {}

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

namespace {

const char *swallow_task_icon(const app::SwallowLabellingTask& task)
{
    using enum app::SwallowLabellingTaskState;
    switch (task.state()) {
    case Annotated:
        return ICON_FA_SQUARE_CHECK "  ";
    case DataFileNotFound:
        return ICON_FA_FILE_CIRCLE_EXCLAMATION "  ";
    default:
        break;
    }

    return "";
}

std::string task_info_str(const app::SwallowLabellingTask& task)
{
    const SwallowTaskInfo& info = task.info();
    return fmt::format(
        "{}Subject #{}, {}\nRepeat #{}, swallow #{}",
        swallow_task_icon(task),
        info.subject,
        swallow_test_type_string(info.test_type),
        info.repeatnum,
        info.swallownum
    );
}

}; // namespace

void Gui::draw_task_list()
{
    const auto& tasks = m_app.tasks();
    const std::size_t num_annotated = m_app.num_annotated_tasks();
    ImGui::Text(
        ICON_FA_SQUARE_CHECK "  %zu task%s out of %zu annotated",
        num_annotated,
        num_annotated == 1 ? "" : "s",
        tasks.size()
    );

    ImGui::BeginDisabled(num_annotated == 0);
    if (ImGui::Button(
            num_annotated != 0 && m_only_show_annotated_tasks ? "Show all" : "Show annotated only"
        ))
    {
        m_only_show_annotated_tasks = !m_only_show_annotated_tasks;
    }
    ImGui::EndDisabled();

    m_task_list_text_filter.Draw("Filter##task_info_list_filter");
    const std::size_t active_index = m_app.active_task_index();
    if (ImGui::BeginListBox("##task_info_list", {-1, -1})) {
        for (std::size_t i = 0; i < tasks.size(); i++) {
            const std::string str = task_info_str(tasks[i]);
            if (m_task_list_text_filter.PassFilter(str.c_str())) {
                if (ImGui::Selectable(str.c_str(), active_index == i)) {
                    m_app.set_active_task_index(i);
                }
            }
        }
        ImGui::EndListBox();
    }
}

void Gui::draw_debug_info()
{
    const ImGuiIO& io = ImGui::GetIO();
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
