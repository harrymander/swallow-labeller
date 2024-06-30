#include "font.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cstdlib>

extern const unsigned int adobe_source_sans_compressed_size;
extern const unsigned int adobe_source_sans_compressed_data[];

namespace recap::labeller::font {

constexpr float FontSize = 18;

void setup_fonts()
{
    const char *no_embed_envvar = std::getenv("RECAP_LABELLER_NO_EMBEDDED_FONTS");
    if (no_embed_envvar != nullptr && std::atoi(no_embed_envvar)) {
        spdlog::info(
            "RECAP_LABELLER_NO_EMBEDDED_FONTS={}; not loading embedded fonts", no_embed_envvar
        );
        return;
    }

    if (!ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(
            static_cast<const void *>(adobe_source_sans_compressed_data),
            static_cast<int>(adobe_source_sans_compressed_size),
            FontSize
        ))
    {
        spdlog::error("Error adding font from memory, falling back to built-in font");
    } else {
        spdlog::debug("Loaded Adobe Source Sans font size {}", FontSize);
    }
}

}; // namespace recap::labeller::font
