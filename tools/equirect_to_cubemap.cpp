#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

struct Float3
{
    float x, y, z;
};

static Float3 Normalize(Float3 v)
{
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len <= 0.000001f) return { 0.0f, 1.0f, 0.0f };
    return { v.x / len, v.y / len, v.z / len };
}

static Float3 FaceDirection(int face, float u, float v)
{
    switch (face)
    {
    case 0: return Normalize({  1.0f, -v,    -u    }); // px
    case 1: return Normalize({ -1.0f, -v,     u    }); // nx
    case 2: return Normalize({  u,     1.0f,  v    }); // py
    case 3: return Normalize({  u,    -1.0f, -v    }); // ny
    case 4: return Normalize({  u,    -v,     1.0f }); // pz
    default:return Normalize({ -u,    -v,    -1.0f }); // nz
    }
}

static Float3 SampleEquirect(const float* pixels, int width, int height, Float3 dir)
{
    constexpr float pi = 3.14159265358979323846f;
    const float longitude = std::atan2(dir.z, dir.x);
    const float latitude = std::asin(std::clamp(dir.y, -1.0f, 1.0f));

    float x = (longitude / (2.0f * pi) + 0.5f) * static_cast<float>(width);
    float y = (0.5f - latitude / pi) * static_cast<float>(height);

    x = std::fmod(x, static_cast<float>(width));
    if (x < 0.0f) x += static_cast<float>(width);
    y = std::clamp(y, 0.0f, static_cast<float>(height - 1));

    const int x0 = static_cast<int>(std::floor(x)) % width;
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = (x0 + 1) % width;
    const int y1 = std::min(y0 + 1, height - 1);
    const float tx = x - std::floor(x);
    const float ty = y - std::floor(y);

    auto at = [&](int px, int py) -> Float3
        {
            const float* p = pixels + (py * width + px) * 3;
            return { p[0], p[1], p[2] };
        };

    const Float3 c00 = at(x0, y0);
    const Float3 c10 = at(x1, y0);
    const Float3 c01 = at(x0, y1);
    const Float3 c11 = at(x1, y1);

    const Float3 a = {
        c00.x + (c10.x - c00.x) * tx,
        c00.y + (c10.y - c00.y) * tx,
        c00.z + (c10.z - c00.z) * tx
    };
    const Float3 b = {
        c01.x + (c11.x - c01.x) * tx,
        c01.y + (c11.y - c01.y) * tx,
        c01.z + (c11.z - c01.z) * tx
    };

    return {
        a.x + (b.x - a.x) * ty,
        a.y + (b.y - a.y) * ty,
        a.z + (b.z - a.z) * ty
    };
}

static uint8_t TonemapToByte(float v)
{
    v = std::max(v, 0.0f);
    v = (v * (2.51f * v + 0.03f)) / (v * (2.43f * v + 0.59f) + 0.14f);
    v = std::pow(std::clamp(v, 0.0f, 1.0f), 1.0f / 2.2f);
    return static_cast<uint8_t>(std::clamp(v * 255.0f + 0.5f, 0.0f, 255.0f));
}

static bool WriteTga(const std::string& path, const std::vector<uint8_t>& rgba, int size)
{
    FILE* file = nullptr;
    if (fopen_s(&file, path.c_str(), "wb") != 0 || !file) return false;

    uint8_t header[18] = {};
    header[2] = 2;
    header[12] = static_cast<uint8_t>(size & 255);
    header[13] = static_cast<uint8_t>((size >> 8) & 255);
    header[14] = static_cast<uint8_t>(size & 255);
    header[15] = static_cast<uint8_t>((size >> 8) & 255);
    header[16] = 32;
    header[17] = 0x28;
    fwrite(header, 1, sizeof(header), file);

    for (int i = 0; i < size * size; ++i)
    {
        const uint8_t out[4] = {
            rgba[i * 4 + 2],
            rgba[i * 4 + 1],
            rgba[i * 4 + 0],
            rgba[i * 4 + 3]
        };
        fwrite(out, 1, 4, file);
    }

    fclose(file);
    return true;
}

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::fprintf(stderr, "Usage: equirect_to_cubemap <input.hdr> <output_dir> <face_size>\n");
        return 1;
    }

    int width = 0, height = 0, channels = 0;
    float* hdr = stbi_loadf(argv[1], &width, &height, &channels, 3);
    if (!hdr)
    {
        std::fprintf(stderr, "Failed to load %s\n", argv[1]);
        return 1;
    }

    const int faceSize = std::max(16, std::atoi(argv[3]));
    const char* names[6] = { "px", "nx", "py", "ny", "pz", "nz" };

    for (int face = 0; face < 6; ++face)
    {
        std::vector<uint8_t> rgba(faceSize * faceSize * 4);
        for (int y = 0; y < faceSize; ++y)
        {
            for (int x = 0; x < faceSize; ++x)
            {
                const float u = (2.0f * (x + 0.5f) / faceSize) - 1.0f;
                const float v = (2.0f * (y + 0.5f) / faceSize) - 1.0f;
                const Float3 color = SampleEquirect(hdr, width, height, FaceDirection(face, u, v));
                const int dst = (y * faceSize + x) * 4;
                rgba[dst + 0] = TonemapToByte(color.x);
                rgba[dst + 1] = TonemapToByte(color.y);
                rgba[dst + 2] = TonemapToByte(color.z);
                rgba[dst + 3] = 255;
            }
        }

        const std::string outPath = std::string(argv[2]) + "/" + names[face] + ".tga";
        if (!WriteTga(outPath, rgba, faceSize))
        {
            std::fprintf(stderr, "Failed to write %s\n", outPath.c_str());
            stbi_image_free(hdr);
            return 1;
        }
        std::printf("Wrote %s\n", outPath.c_str());
    }

    stbi_image_free(hdr);
    return 0;
}
