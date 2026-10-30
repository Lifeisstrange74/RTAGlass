// PNG and DDS writers for 32-bit RGBA images (byte order R,G,B,A per pixel, alpha 0-255).
// PNG uses a small built-in deflate (LZ77 with lazy matching, dynamic Huffman codes), so there are no library
// dependencies.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

bool WritePng(const char* path, const uint32_t* rgba, int w, int h);

// Uncompressed A8R8G8B8 DDS (D3DFMT_A8R8G8B8), the runtime format planned in docs/graphics-plan.md.
bool WriteDds(const char* path, const uint32_t* rgba, int w, int h);

// The PNG writer's deflate as a raw RFC 1951 stream (no zlib header): tools/rpak uses it.
std::vector<uint8_t> DeflateRaw(const uint8_t* data, size_t size);
