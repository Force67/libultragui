#include "svg_types.h"

#include <ultragui/rhi/rhi.h>
#include <ultragui/svg/svg.h>

#include <cstdio>

namespace ugui {

bool LoadSvg(const char* path, SvgImage& out, u32 target_width, u32 target_height) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) {
        std::fprintf(stderr, "ultragui: failed to open SVG file '%s'\n", path);
        return false;
    }

    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        std::fclose(f);
        return false;
    }

    Vector<char> data(static_cast<usize>(size));
    usize read = std::fread(data.data(), 1, static_cast<usize>(size), f);
    (void)read;
    std::fclose(f);

    return LoadSvgMemory(data.data(), static_cast<usize>(size), out, target_width, target_height);
}

bool LoadSvgMemory(const char* data, usize length, SvgImage& out, u32 target_width,
                     u32 target_height) {
    svg::Document doc;
    if (!svg::ParseSvg(data, length, doc))
        return false;

    if (target_width == 0)
        target_width = static_cast<u32>(doc.width);
    if (target_height == 0)
        target_height = static_cast<u32>(doc.height);
    if (target_width == 0 || target_height == 0)
        return false;

    out.width = target_width;
    out.height = target_height;
    out.pixels.resize(target_width * target_height * 4, 0);

    svg::Rasterize(doc, out.pixels.data(), target_width, target_height);
    return true;
}

RHITextureHandle LoadSvgTexture(RHI* rhi, const char* path, u32 target_width,
                                  u32 target_height) {
    if (!rhi)
        return kInvalidTexture;

    SvgImage img;
    if (!LoadSvg(path, img, target_width, target_height))
        return kInvalidTexture;

    return rhi->CreateTexture(img.width, img.height, RHIFormat::kRgba8Unorm, img.pixels.data());
}

} // namespace ugui
