#include "font.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cstdlib>

extern const unsigned int adobe_source_sans_compressed_size;
extern const unsigned int adobe_source_sans_compressed_data[];

namespace recap::labeller::font {

namespace {

constexpr float DefaultFontSize = 18;

float get_font_size()
{
    const char *var = std::getenv("RECAP_LABELLER_EMBEDDED_FONT_SIZE");
    if (var == nullptr) {
        return DefaultFontSize;
    }

    spdlog::debug("RECAP_LABELLER_EMBEDDED_FONT_SIZE={}", var);
    auto size = static_cast<float>(std::atof(var));
    return size > 0 ? size : DefaultFontSize;
}

}; // namespace

void setup_fonts()
{
    const char *no_embed_envvar = std::getenv("RECAP_LABELLER_NO_EMBEDDED_FONTS");
    if (no_embed_envvar != nullptr && std::atoi(no_embed_envvar)) {
        spdlog::debug("RECAP_LABELLER_NO_EMBEDDED_FONTS={}", no_embed_envvar);
        spdlog::info("Not loading embedded fonts");
        return;
    }

    const float font_size = get_font_size();
    if (!ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(
            static_cast<const void *>(adobe_source_sans_compressed_data),
            static_cast<int>(adobe_source_sans_compressed_size),
            font_size
        ))
    {
        spdlog::error("Error adding font from memory, falling back to built-in font");
    } else {
        spdlog::debug("Loaded Adobe Source Sans font size {}", font_size);
    }
}

}; // namespace recap::labeller::font
