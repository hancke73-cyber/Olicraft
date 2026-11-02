#include "Image.h"

#include <cstring>
#include <filesystem>

// stb_image / stb_image_write (gemeinfrei), siehe third_party/stb
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wcast-function-type"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace oc {

bool loadImage(const std::string& path, Image& out, std::string* error) {
    int w = 0, h = 0, n = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!data) {
        if (error) *error = std::string("Bild nicht lesbar: ") + path;
        return false;
    }
    out = Image(w, h);
    std::memcpy(out.rgba.data(), data, out.rgba.size());
    stbi_image_free(data);
    return true;
}

bool savePng(const std::string& path, const Image& img, std::string* error) {
    if (!img.valid()) {
        if (error) *error = "Ungueltiges Bild";
        return false;
    }
    try {
        std::filesystem::path p(path);
        if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return false;
    }
    if (!stbi_write_png(path.c_str(), img.w, img.h, 4, img.rgba.data(), img.w * 4)) {
        if (error) *error = std::string("Datei kann nicht geschrieben werden: ") + path;
        return false;
    }
    return true;
}

}  // namespace oc
