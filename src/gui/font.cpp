#include "font.hpp"

#include "util/os.hpp"

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

extern const unsigned int adobe_source_sans_compressed_size;
extern const unsigned int adobe_source_sans_compressed_data[];
extern const unsigned int fontawesome_free_solid_compressed_size;
extern const unsigned int fontawesome_free_solid_compressed_data[];

namespace recap::labeller::gui {

namespace {

constexpr float DefaultFontSize = 18;

float get_font_size()
{
    auto var = os::getenv("RECAP_LABELLER_EMBEDDED_FONT_SIZE");
    if (!var || var->empty()) {
        return DefaultFontSize;
    }

    spdlog::debug("RECAP_LABELLER_EMBEDDED_FONT_SIZE={}", *var);
    auto size = static_cast<float>(std::atof(var->c_str()));
    return size > 0 ? size : DefaultFontSize;
}

void merge_icon_font()
{
    // Merge the icon font into main font
    static ImFontConfig font_cfg;
    font_cfg.MergeMode = true;
    static const ImWchar glyph_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
    auto& font_atlas = ImGui::GetIO().Fonts;
    const float font_size = .75F * font_atlas->ConfigData.back().SizePixels;
    if (font_atlas->AddFontFromMemoryCompressedTTF(
            static_cast<const void *>(fontawesome_free_solid_compressed_data),
            static_cast<int>(fontawesome_free_solid_compressed_size),
            font_size,
            &font_cfg,
            glyph_ranges
        )
        == nullptr)
    {
        spdlog::error("Error loading embedded FontAwesome font - icons won't be displayed!");
    } else {
        spdlog::debug("Loaded embedded FontAwesome font, size {} px", font_size);
    }
}

}; // namespace

void setup_fonts()
{
    const auto no_embed_envvar = os::getenv("RECAP_LABELLER_NO_EMBEDDED_FONTS");
    if (no_embed_envvar.has_value() && std::atoi(no_embed_envvar->c_str())) {
        spdlog::debug("RECAP_LABELLER_NO_EMBEDDED_FONTS={}", *no_embed_envvar);
        spdlog::info("Not loading embedded fonts");
        return;
    }

    const float font_size = get_font_size();
    ImFontAtlas *atlas = ImGui::GetIO().Fonts;
    if (!atlas->AddFontFromMemoryCompressedTTF(
            static_cast<const void *>(adobe_source_sans_compressed_data),
            static_cast<int>(adobe_source_sans_compressed_size),
            font_size,
            nullptr,
            atlas->GetGlyphRangesGreek() // includes Basic Latin and Latin 1 Supplement
        ))
    {
        spdlog::error("Error adding font from memory, falling back to built-in font");
    } else {
        spdlog::debug("Loaded Adobe Source Sans font size {}", font_size);
    }

    merge_icon_font();
}

}; // namespace recap::labeller::gui
