#include "gui/gui.hpp"

#include "app/app.hpp"
#include "app/labeller.hpp"
#include "gui/font.hpp"
#include "gui/icons.h"
#include "gui/labeller-view.hpp"
#include "gui/widgets/color-scheme-selector.hpp"
#include "gui/widgets/util.hpp"
#include "util/os.hpp"

#include <fmt/std.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <nfd.h>
#include <nfd.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <future>
#include <memory>

namespace recap::labeller::gui {

namespace {

template <typename ShowFunc> void show_window(bool& open, ShowFunc&& show)
{
    if (open) {
        show(&open);
    }
}

void draw_status_bar()
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
        1000.0F / io.Framerate,
        io.Framerate
    );

    ImGui::End();
}

void annotations_save_copy(app::Labeller& labeller)
{
    static const std::array<nfdfilteritem_t, 1> save_filters = {{
        {"JSON", "json"},
    }};

    spdlog::info("Selecting annotations file save copy path...");
    NFD::UniquePath save_path;
    nfdresult_t res = NFD::SaveDialog(
        save_path,
        save_filters.data(),
        static_cast<nfdfiltersize_t>(save_filters.size()),
        nullptr,
        "annotations.json"
    );
    if (res == NFD_OKAY) {
        if (save_path) {
            labeller.save_annotations_to_path(save_path.get());
        } else {
            spdlog::error("NFD::SaveDialog returned okay, but path string is null");
        }
    } else if (res == NFD_CANCEL) {
        spdlog::info("Annotations file save copy cancelled");
    } else {
        spdlog::error("Error picking annotations save path: {}", NFD::GetError());
    }
}

}; // namespace

class Gui::Impl {
private:
    app::App& m_app;
    app::App::LabellerUpdateObservable::Observer m_labeller_update_observer;

    bool m_stop_requested = false;

    app::Labeller *m_labeller = nullptr;
    std::unique_ptr<LabellerView> m_labeller_view;

    bool m_nfd_available = false;
    std::string m_ini_path;

#if NDEBUG
    static constexpr bool DefaultShowDebugInfo = false;
#else
    static constexpr bool DefaultShowDebugInfo = true;
#endif

    bool m_show_imgui_demo_window = false;
    bool m_show_implot_demo_window = false;
    bool m_show_imgui_metrics = false;
    widgets::ColorSchemeSelector m_color_scheme_selector;
    bool m_show_debug_info = DefaultShowDebugInfo;

    std::future<os::OsOpenStatus> m_open_annotations_path_future;

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

    void draw_menu_bar()
    {
        if (ImGui::BeginMenu("File")) {
            ImGui::BeginDisabled(!(m_labeller && m_nfd_available));
            if (ImGui::MenuItem("Save a copy of annotations file...") && m_nfd_available) {
                annotations_save_copy(*m_labeller);
            }
            ImGui::EndDisabled();
            annotations_path_open();

            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Alt+F4")) {
                stop();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Reset window layout")) {
                if (m_labeller_view) {
                    m_labeller_view->reset_view();
                }
                m_show_imgui_demo_window = false;
                m_show_implot_demo_window = false;
                m_show_imgui_metrics = false;
                m_show_debug_info = DefaultShowDebugInfo;
            }
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
#ifndef NDEBUG
            if (ImGui::MenuItem("Show critical error")) {
                spdlog::error("Set critical error from debug tools menu");
                m_app.set_critical_error("Critical error set from debug tools");
            }
#endif
            ImGui::EndMenu();
        }
    }

    void annotations_path_open()
    {
        if (m_open_annotations_path_future.valid()) {
            if (m_open_annotations_path_future.wait_for(std::chrono::seconds(0))
                == std::future_status::ready)
            {
                m_open_annotations_path_future.get();
            }
        }
        bool can_open = !m_open_annotations_path_future.valid();
        ImGui::BeginDisabled(!(m_labeller && can_open));
        if (ImGui::MenuItem("Open annotations file in explorer...") && can_open) {
            m_open_annotations_path_future =
                os::open_path_in_file_explorer(m_labeller->annotations_path());
        }
        ImGui::EndDisabled();
    }

    void draw_critical_error(const std::string& error)
    {
        constexpr ImGuiWindowFlags WindowsFlags = ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoMove;
        constexpr ImU32 TitleColor = 0xCC2929FF;
        constexpr ImVec2 CentrePos = {0.5F, 0.5F};

        widgets::ScopedImColor color_scope = {
            {ImGuiCol_TitleBg, TitleColor},
            {ImGuiCol_TitleBgActive, TitleColor},
        };
        ImGui::SetNextWindowPos(
            ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, CentrePos
        );
        if (ImGui::Begin(ERR_ICON ICON_TEXT_SPACE "Critical error", nullptr, WindowsFlags)) {
            ImGui::TextUnformatted(error.c_str());
            ImGui::Spacing();
            if (widgets::ButtonRed(EXIT_ICON ICON_TEXT_SPACE "Quit")) {
                stop();
            }
        }
        ImGui::End();
    }

public:
    explicit Impl(app::App& app) :
        m_app(app),
        m_labeller_update_observer(m_app.subscribe_labeller_update([this](app::Labeller& labeller) {
            m_labeller = &labeller;
            m_labeller_view = std::make_unique<LabellerView>(labeller);
        }))
    {
        if (NFD::Init() != NFD_OKAY) {
            spdlog::error("Error initialising NFD: {}", NFD::GetError());
        } else {
            spdlog::debug("NFD initialised");
            m_nfd_available = true;
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
        if (ImGui::BeginMainMenuBar()) {
            draw_menu_bar();
            ImGui::EndMainMenuBar();
        }

        const auto& critical_error = m_app.critical_error();
        if (critical_error) {
            draw_critical_error(*critical_error);
        }

        show_window(m_show_imgui_demo_window, ImGui::ShowDemoWindow);
        show_window(m_show_imgui_metrics, ImGui::ShowMetricsWindow);
        show_window(m_show_implot_demo_window, ImPlot::ShowDemoWindow);

        if (m_show_debug_info) {
            draw_status_bar();
        }
    }

    void stop()
    {
        m_stop_requested = true;
        if (m_labeller_view) {
            m_labeller_view->stop();
        }
    }

    [[nodiscard]] bool ready_to_stop() const
    {
        return m_labeller_view ? m_labeller_view->ready_to_stop() : m_stop_requested;
    }
};

Gui::Gui(app::App& app) : m_pimpl(std::make_unique<Impl>(app)) {}

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
