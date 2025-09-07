#include "gui/gui.hpp"

#include "app/annotation-store.hpp"
#include "app/task-loader.hpp"
#include "gui/font.hpp"
#include "gui/icons.h"
#include "gui/task-list.hpp"
#include "gui/task-view.hpp"
#include "gui/widgets/util.hpp"
#include "models/task-info.hpp"
#include "util/os.hpp"

#include <fmt/std.h>
#include <imgui.h>
#include <implot.h>
#include <nfd.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <memory>
#include <string>

namespace recap::labeller::gui {

class Gui::Impl {
    bool m_nfd_available = false;
    bool m_ready_to_stop = false;
    std::string m_ini_path;
    app::TaskLoader m_task_loader;

    TaskList m_task_list;
    std::filesystem::path m_data_dir;
    SwallowAnnotationStore& m_annotation_store;
    std::unique_ptr<TaskView> m_task_view;

    static bool is_valid_ini_path(const std::filesystem::path& path)
    {
        namespace fs = std::filesystem;
        if (fs::is_directory(path)) {
            spdlog::error("Invalid INI path: '{}' is a directory", path);
            return false;
        }

        const auto parent = path.parent_path();
        if (!parent.empty() && !fs::is_directory(parent)) {
            spdlog::error(
                "Invalid INI path: parent directory '{}' does not exist or is not a directory",
                parent
            );
            return false;
        }

        return true;
    }

    void setup_imgui_ini()
    {
        ImGuiIO& io = ImGui::GetIO();
        const auto env = os::getenv("RECAP_LABELLER_IMGUI_INI_PATH");
        if (env.has_value()) {
            if (!env->empty()) {
                std::filesystem::path path(*env);
                if (is_valid_ini_path(path)) {
                    m_ini_path = path.make_preferred().string();
                    spdlog::info("Custom ImGui INI path: '{}'", m_ini_path);
                    io.IniFilename = m_ini_path.c_str();
                } else {
                    spdlog::error("Using default ImGui INI path");
                }
            } else {
                spdlog::info("RECAP_LABELLER_IMGUI_INI_PATH empty: disabling INI file");
                io.IniFilename = nullptr;
            }
        } else {
            spdlog::debug("RECAP_LABELLER_IMGUI_INI_PATH not set, using default ImGui INI path");
        }
    }

    static void draw_status_bar()
    {
        constexpr float FramePadding = 5;
        widgets::ScopedImStyle style_scope = {
            {ImGuiStyleVar_WindowBorderSize, 0.0F},
            {ImGuiStyleVar_WindowPadding, ImVec2{ImGui::GetStyle().WindowPadding.x, 0}},
            {ImGuiStyleVar_FramePadding, ImVec2{FramePadding, FramePadding}},
        };

        constexpr ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;
        if (!ImGui::BeginViewportSideBar(
                "##viewport_status_bar",
                ImGui::GetMainViewport(),
                ImGuiDir_Down,
                ImGui::GetFrameHeight(),
                WindowFlags
            ))
        {
            return;
        }

        ImGui::AlignTextToFramePadding();
        const ImGuiIO& io = ImGui::GetIO();
        ImGui::Text(
            DEBUG_INFO_ICON ICON_TEXT_SPACE
            "Mouse Position: [%.0f,%.0f]. Application average: %.3f ms/frame (%.1f FPS).",
            io.MousePos.x,
            io.MousePos.y,
            1000.0f / io.Framerate,
            io.Framerate
        );

        ImGui::End();
    }

public:
    Impl(
        const std::vector<models::SwallowTaskInfo>& tasks,
        SwallowAnnotationStore& annotation_store,
        const std::filesystem::path& data_dir
    ) :
        m_task_list(tasks, annotation_store, m_task_loader, data_dir),
        m_data_dir(data_dir),
        m_annotation_store(annotation_store),
        m_task_view(load_task_view(m_task_loader, m_data_dir, m_task_list.currently_selected_task())
        )
    {
        if (NFD::Init() != NFD_OKAY) {
            spdlog::error("Error initialising NFD: {}", NFD::GetError());
        } else {
            m_nfd_available = true;
            spdlog::debug("NFD initialised");
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
        setup_imgui_ini();
        setup_fonts();
    }

    ~Impl()
    {
        if (m_nfd_available) {
            NFD::Quit();
        }

        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    void draw()
    {
        draw_status_bar();
        const models::SwallowTaskInfo *new_task = m_task_list.draw("Labelling tasks");
        if (new_task) {
            m_task_view = load_task_view(m_task_loader, m_data_dir, *new_task);
        }

        if (m_task_view) {
            if (ImGui::Begin("Labelling task")) {
                m_task_view->draw();
            }
            ImGui::End();
        }
    }

    void stop() { m_ready_to_stop = true; }

    bool ready_to_stop() const { return m_ready_to_stop; }
};

Gui::Gui(
    const std::vector<models::SwallowTaskInfo>& tasks,
    SwallowAnnotationStore& annotation_store,
    const std::filesystem::path& data_dir
) :
    m_pimpl(std::make_unique<Impl>(tasks, annotation_store, data_dir))
{}

Gui::~Gui() = default;

void Gui::draw()
{
    m_pimpl->draw();
}

void Gui::stop()
{
    m_pimpl->stop();
}

void Gui::set_scaling_factor(float scaling_factor)
{
    ImGui::GetStyle().ScaleAllSizes(scaling_factor);
}

bool Gui::ready_to_stop() const
{
    return m_pimpl->ready_to_stop();
}

}; // namespace recap::labeller::gui
