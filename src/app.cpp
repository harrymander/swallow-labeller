#include "app.hpp"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>

namespace app {

static bool to_close = false;

int setup()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    return 0;
}

void teardown()
{
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

static void draw_demo_windows()
{
    static bool show_imgui_demo = false;
    static bool show_implot_demo = false;
    ImGui::Checkbox("ImGui demo window", &show_imgui_demo);
    ImGui::SameLine();
    ImGui::Checkbox("ImPlot demo window", &show_implot_demo);
    if (show_imgui_demo)
        ImGui::ShowDemoWindow(&show_imgui_demo);
    if (show_implot_demo)
        ImPlot::ShowDemoWindow(&show_implot_demo);
}

struct PlotData {
    static constexpr std::size_t size = 1001;
    std::array<float, size> y;
    std::array<float, size> x;

    PlotData()
    {
        for (std::size_t i = 0; i < size; i++) {
            x[i] = i * 0.001f;
            y[i] = 0.25f + 0.25f * sinf(25 * x[i]) * sinf(5 * x[i]);
        }
    }
};

template <class BidirIt, class T>
BidirIt binary_search_closest(BidirIt first, BidirIt last, const T& value)
{
    BidirIt found = std::lower_bound(first, last, value);
    if (found != last && found != first) {
        const auto prev = std::prev(found);
        if (value - *prev < *found - value)
            found = prev;
    }
    return found;
}

static void add_plot_marker(ImDrawList *draw_list, const ImVec2& pos)
{
    constexpr float half_width = 4;
    draw_list->AddRect(
        ImVec2(pos.x - half_width, pos.y - half_width),
        ImVec2(pos.x + half_width, pos.y + half_width),
        ImColor(128, 128, 128)
    );
}

/**
 * Add text in position (xp, yp), automatically right-aligining text if it would be greater than
 * xend
 */
static void
add_text_autoalign(ImDrawList *draw_list, const char *text, float xp, float yp, float xend)
{
    static constexpr float align_margin = 15;
    static constexpr float padding = 6;
    const auto text_size = ImGui::CalcTextSize(text);
    if (xp + text_size.x + align_margin > xend) {
        xp -= text_size.x + padding;
    } else {
        xp += padding;
    }
    draw_list->AddText(ImVec2(xp, yp), ImGui::GetColorU32(ImGuiCol_Text), text);
}

static void add_plot_vline(ImDrawList *draw_list, const ImVec2& posplot, const ImVec2& pospx)
{
    const ImVec2 plot_pos = ImPlot::GetPlotPos();
    const ImVec2 plot_size = ImPlot::GetPlotSize();
    const ImVec2 top(pospx.x, plot_pos.y);
    const ImVec2 bottom(pospx.x, top.y + plot_size.y);
    draw_list->AddLine(top, bottom, ImColor(128, 128, 128));

    const float xend = plot_pos.x + plot_size.x;
    char xtext[20];
    std::snprintf(xtext, sizeof(xtext), "x=%g", posplot.x);
    add_text_autoalign(
        draw_list, xtext, bottom.x, bottom.y - ImGui::GetTextLineHeightWithSpacing(), xend
    );

    char ytext[20];
    std::snprintf(ytext, sizeof(ytext), "y=%g", posplot.y);
    add_text_autoalign(draw_list, ytext, top.x, top.y, xend);
}

static void draw_plot_cursor(float xplot, float yplot)
{
    ImDrawList *draw_list = ImPlot::GetPlotDrawList();
    const auto pospx = ImPlot::PlotToPixels(xplot, yplot);
    add_plot_vline(draw_list, ImVec2(xplot, yplot), pospx);
    add_plot_marker(draw_list, pospx);

    static std::optional<ImPlotRect> rect = std::nullopt;
    if ((ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl))
        && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const auto limits = ImPlot::GetPlotLimits();
        if (!rect.has_value()) {
            rect = std::make_optional<ImPlotRect>();
            rect->X.Min = xplot;
        }
        rect->X.Max = xplot;
        rect->Y.Max = limits.Y.Max;
        rect->Y.Min = limits.Y.Min;

        draw_list->AddRectFilled(
            ImPlot::PlotToPixels(rect->Min()),
            ImPlot::PlotToPixels(rect->Max()),
            ImColor(120, 0, 0, 90)
        );
    } else {
        rect.reset();
    }
}

static void draw_plot_hovered(const PlotData& data)
{
    const auto mouse = ImPlot::GetPlotMousePos();
    if (mouse.x > data.x[0]) {
        const auto xplot = binary_search_closest(data.x.begin(), data.x.end(), mouse.x);
        if (xplot != data.x.end())
            draw_plot_cursor(*xplot, data.y[std::distance(data.x.begin(), xplot)]);
    }
}

static void setup_plot(const PlotData& data)
{
    ImPlot::SetupAxis(ImAxis_Y1, nullptr, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit);
    ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, data.x[0], data.x[data.size - 1]);
}

static inline bool isnear(double a, double b, double eps)
{
    return std::abs(a - b) <= eps;
}

class DragRect {
public:
    explicit DragRect(const ImPlotRange xrange = ImPlotRange(0, 0)) : xrange(xrange) {}

    void draw(ImDrawList *draw_list)
    {
        const auto yrange = ImPlot::GetPlotLimits().Y;
        draw_list->AddRectFilled(
            ImPlot::PlotToPixels(ImVec2(xrange.Min, yrange.Min)),
            ImPlot::PlotToPixels(ImVec2(xrange.Max, yrange.Max)),
            ImColor(128, 128, 128, 100)
        );

        const double xmouse = ImPlot::GetPlotMousePos().x;
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            switch (state) {
                using enum State;
            case Dragging:
                drag(xmouse);
                break;
            case MinResizing:
                min_resize(xmouse);
                break;
            case MaxResizing:
                max_resize(xmouse);
                break;
            case None:
                break;
            }
        } else {
            if (state != State::None)
                sort_xrange();
            check_mouse(xmouse);
        }

        draw_cursor();
    }

    void sort_xrange()
    {
        if (xrange.Min > xrange.Max)
            std::swap(xrange.Min, xrange.Max);
    }

    void debug() const
    {
        ImGui::Text(
            "xrange_dragstart = (%g, %g), xmouse_dragstart = %g",
            xrange_dragstart.Min,
            xrange_dragstart.Max,
            xmouse_dragstart
        );
    }

    ImPlotRange xrange;

private:
    enum class State {
        None,
        Dragging,
        MinResizing,
        MaxResizing,
    };

    State state = State::None;
    ImPlotRange xrange_dragstart;
    double xmouse_dragstart = 0;

    void check_mouse(double xmouse)
    {
        using enum State;
        if (ImPlot::IsPlotHovered()) {
            const double mouse_near = ImPlot::PixelsToPlot(20, 0).x;
            if (isnear(xmouse, xrange.Min, mouse_near)) {
                state = MinResizing;
            } else if (isnear(xmouse, xrange.Max, mouse_near)) {
                state = MaxResizing;
            } else if (xmouse > xrange.Min && xmouse < xrange.Max) {
                state = Dragging;
                xrange_dragstart = xrange;
                xmouse_dragstart = xmouse;
            } else {
                state = None;
            }
        } else {
            state = None;
        }
    }

    void draw_cursor() const
    {
        using enum State;
        switch (state) {
        case Dragging:
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            break;
        case MinResizing:
        case MaxResizing:
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            break;
        case None:
            break;
        }
    }

    void drag(double xmouse)
    {
        const double dx = xmouse - xmouse_dragstart;
        const auto xlim = ImPlot::GetPlotLimits().X;

        if (!xlim.Contains(xrange_dragstart.Min + dx)) {
            xrange.Max = xlim.Min + xrange.Size();
            xrange.Min = xlim.Min;
        } else if (!xlim.Contains(xrange_dragstart.Max + dx)) {
            xrange.Min = xlim.Max - xrange.Size();
            xrange.Max = xlim.Max;
        } else {
            xrange.Min = xrange_dragstart.Min + dx;
            xrange.Max = xrange_dragstart.Max + dx;
        }
    }

    void min_resize(double xmouse) { xrange.Min = ImPlot::GetPlotLimits().X.Clamp(xmouse); }

    void max_resize(double xmouse) { xrange.Max = ImPlot::GetPlotLimits().X.Clamp(xmouse); }
};

static void draw_plot()
{
    static PlotData data;
    static DragRect range_rect(ImPlotRange(data.x[data.size / 4], data.x[data.size * 3 / 4]));

    if (ImPlot::BeginPlot(
            "##mainplot", ImVec2(-1, 0), ImPlotFlags_NoMouseText | ImPlotFlags_NoBoxSelect
        ))
    {
        setup_plot(data);
        ImPlot::PlotLine("##data", data.x.data(), data.y.data(), data.size);
        if (ImPlot::IsPlotHovered())
            draw_plot_hovered(data);
        ImPlot::EndPlot();
    }

    if (ImPlot::BeginPlot("##summary", ImVec2(-1, 75), ImPlotFlags_CanvasOnly)) {
        static constexpr ImPlotAxisFlags axis_flags =
            ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_AutoFit;
        ImPlot::SetupAxes(nullptr, nullptr, axis_flags, axis_flags);
        range_rect.draw(ImPlot::GetPlotDrawList());
        ImPlot::PlotLine("##data", data.x.data(), data.y.data(), data.size);
        ImPlot::EndPlot();
    }

    ImGui::Text("%g %g", range_rect.xrange.Min, range_rect.xrange.Max);
    range_rect.debug();
}

static void draw_window_contents()
{
    draw_demo_windows();
    draw_plot();
}

bool draw()
{
    const auto& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::Begin(
        "##mainwindow",
        nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize
    );
    draw_window_contents();
    ImGui::End();
    return !to_close;
}

void close()
{
    to_close = true;
}

}; // namespace app
