// SPDX-License-Identifier: GPL-3.0-or-later
//
// Headless unit tests for the pure presentation logic (src/video/PixelConversion):
// pixel formats, texture UVs, readback row order and the framebuffer policy. No GPU,
// no window, no SDL.
//
// These are the parts of the video backend that were previously only checked by hand,
// because the backend itself needs a GL context.

#include "video/PixelConversion.h"
#include "test_common.h"

#include <cstdio>
#include <cstring>
#include <vector>

using tests::Check;
using tests::CheckEqual;
using tests::CheckNear;
using video::TextureUV;

namespace {

void TestPixelFormats() {
    std::printf("[1] formatos de píxel\n");

    // Two pixels per row, two rows, with a padded pitch: the core can send more bytes per
    // row than the row itself, so row 1 starts at byte 12 and not at byte 8. The padding
    // slots must be skipped, and the buffer must cover the padded rows.
    const uint32_t frame[6] = {0x00102030u, 0x00F0E0D0u, 0x00000000u,
                               0x00010203u, 0x00A0B0C0u, 0x00000000u};
    std::vector<uint8_t> dst;

    Check(video::ConvertFrameToRGBA8(dst, frame, 2, 2, 12, RETRO_PIXEL_FORMAT_XRGB8888),
          "XRGB8888 con pitch acolchado");
    CheckEqual(static_cast<long long>(dst.size()), 2 * 2 * 4, "tamaño del buffer RGBA8");
    CheckEqual(dst[0], 0x10, "XRGB8888: canal R en memoria");
    CheckEqual(dst[1], 0x20, "XRGB8888: canal G en memoria");
    CheckEqual(dst[2], 0x30, "XRGB8888: canal B en memoria");
    CheckEqual(dst[3], 0xFF, "XRGB8888: alfa opaca");
    CheckEqual(dst[4], 0xF0, "XRGB8888: segundo pixel de la fila");
    CheckEqual(dst[8], 0x01, "XRGB8888: primera columna de la segunda fila (pitch respetado)");
    CheckEqual(dst[9], 0x02, "XRGB8888: segunda fila, canal G");
    CheckEqual(dst[10], 0x03, "XRGB8888: segunda fila, canal B");
    CheckEqual(dst[12], 0xA0, "XRGB8888: segunda fila, segunda columna");

    // The 5/6 bit components must be expanded by replicating the top bits, so the full
    // range maps to 0..255.
    CheckEqual(static_cast<long long>(video::Expand5(0)), 0, "Expand5(0)");
    CheckEqual(static_cast<long long>(video::Expand5(31)), 255, "Expand5(31)");
    CheckEqual(static_cast<long long>(video::Expand6(0)), 0, "Expand6(0)");
    CheckEqual(static_cast<long long>(video::Expand6(63)), 255, "Expand6(63)");

    const uint16_t rgb565[4] = {0xFFFF, 0x0800, 0x0020, 0x001F};
    Check(video::ConvertFrameToRGBA8(dst, rgb565, 2, 2, 0, RETRO_PIXEL_FORMAT_RGB565),
          "RGB565");
    CheckEqual(dst[0], 255, "RGB565: blanco");
    CheckEqual(dst[1], 255, "RGB565: blanco (G)");
    CheckEqual(dst[4], 8, "RGB565: rojo mínimo (1 bit extendido a 8)");
    CheckEqual(dst[5], 0, "RGB565: verde nulo");
    CheckEqual(dst[9], 4, "RGB565: verde mínimo (1 bit extendido a 6)");
    CheckEqual(dst[14], 255, "RGB565: azul máximo");

    const uint16_t rgb1555[4] = {0x7FFF, 0x0400, 0x0020, 0x001F};
    Check(video::ConvertFrameToRGBA8(dst, rgb1555, 2, 2, 0, RETRO_PIXEL_FORMAT_0RGB1555),
          "0RGB1555");
    CheckEqual(dst[0], 255, "0RGB1555: blanco");
    CheckEqual(dst[4], 8, "0RGB1555: rojo mínimo");
    CheckEqual(dst[9], 8, "0RGB1555: verde mínimo (1 bit extendido a 5)");
    CheckEqual(dst[14], 255, "0RGB1555: azul máximo");

    // A pitch smaller than a row is malformed: it must be rejected instead of reading
    // past the buffer the core provided.
    std::vector<uint8_t> sentinel(64, 0x5A);
    Check(!video::ConvertFrameToRGBA8(sentinel, frame, 2, 2, 4, RETRO_PIXEL_FORMAT_XRGB8888),
          "un pitch menor que una fila debe rechazarse");
    CheckEqual(static_cast<long long>(sentinel.size()), 64, "el buffer no se toca al rechazar");

    // An unknown format must be rejected too.
    Check(!video::ConvertFrameToRGBA8(dst, frame, 2, 2, 0,
                                      static_cast<retro_pixel_format>(1000)),
          "un formato desconocido debe rechazarse");
    Check(!video::ConvertFrameToRGBA8(dst, nullptr, 2, 2, 0, RETRO_PIXEL_FORMAT_XRGB8888),
          "un frame vacío debe rechazarse");
}

void TestTextureUVs() {
    std::printf("[2] UV y bottom_left_origin\n");

    // Software frames are uploaded top-down, so no vertical flip is needed.
    const TextureUV software = video::ComputeTextureUVs(2048, 2048, 400, 240, false, true);
    CheckNear(software.u0, 0.0, 1e-6, "software: u0");
    CheckNear(software.v0, 0.0, 1e-6, "software: v0 se mantiene arriba");
    CheckNear(software.u1, 400.0 / 2048.0, 1e-6, "software: recorte horizontal");
    CheckNear(software.v1, 240.0 / 2048.0, 1e-6, "software: recorte vertical");

    // The core rendered bottom-up: the top of the image is the last texture row, so the
    // quad is sampled flipped.
    const TextureUV bottom_left = video::ComputeTextureUVs(2048, 2048, 400, 240, true, true);
    CheckNear(bottom_left.v0, 240.0 / 2048.0, 1e-6, "hardware bottom-left: v0 abajo");
    CheckNear(bottom_left.v1, 0.0, 1e-6, "hardware bottom-left: v1 arriba");
    CheckNear(bottom_left.u1, 400.0 / 2048.0, 1e-6, "hardware bottom-left: u1");

    const TextureUV top_left = video::ComputeTextureUVs(2048, 2048, 400, 240, true, false);
    CheckNear(top_left.v0, 0.0, 1e-6, "hardware top-left: v0 no se invierte");
    CheckNear(top_left.v1, 240.0 / 2048.0, 1e-6, "hardware top-left: v1");

    // Nothing drawn yet -> the whole texture, which the UI must not sample.
    const TextureUV empty = video::ComputeTextureUVs(2048, 2048, 0, 0, true, true);
    CheckNear(empty.u1, 1.0, 1e-6, "sin frame: uv completa (u1)");
    CheckNear(empty.v1, 1.0, 1e-6, "sin frame: uv completa (v1)");

    // Frame exactly the size of the framebuffer: no crop.
    const TextureUV full = video::ComputeTextureUVs(400, 240, 400, 240, true, false);
    CheckNear(full.u1, 1.0, 1e-6, "frame igual al FBO: sin recorte");
    CheckNear(full.v1, 1.0, 1e-6, "frame igual al FBO: sin recorte vertical");
}

void TestReadbackRows() {
    std::printf("[3] filas del readback (bottom-up -> top-down)\n");

    // A 2x2 readback: row 0 is the bottom of the image in GL.
    std::vector<uint8_t> raw = {
            0x01, 0x02, 0x03, 0xFF, 0x04, 0x05, 0x06, 0xFF, // bottom row
            0x07, 0x08, 0x09, 0xFF, 0x0A, 0x0B, 0x0C, 0xFF, // top row
    };

    std::vector<uint8_t> dst;
    video::FlipRowsToTopDown(dst, raw, 2, 2);
    CheckEqual(static_cast<long long>(dst.size()), static_cast<long long>(raw.size()),
               "el readback voltea filas, no píxeles");
    CheckEqual(dst[0], 0x07, "primera fila del archivo = última fila del readback");
    CheckEqual(dst[8], 0x01, "última fila del archivo = primera fila del readback");

    // A readback shorter than the frame must not read past its end, and must not be
    // reported as a frame: dst stays empty so the capture is rejected.
    std::vector<uint8_t> short_src(8, 0x7F); // one row instead of two
    std::vector<uint8_t> short_dst;
    Check(!video::FlipRowsToTopDown(short_dst, short_src, 2, 2),
          "readback incompleto: se rechaza, no se rellena");
    CheckEqual(static_cast<long long>(short_dst.size()), 0,
               "readback incompleto: dst queda vacío");
}

void TestFramebufferPolicy() {
    std::printf("[4] política del FBO (grow-only)");

    Check(video::FramebufferNeedsReallocation(0, 0, 400, 240),
          "sin FBO hay que crearlo");
    Check(!video::FramebufferNeedsReallocation(2048, 2048, 400, 240),
          "un FBO mayor no se recrece: se recorta con UV");
    Check(!video::FramebufferNeedsReallocation(400, 240, 400, 240),
          "el mismo tamaño no recrea el FBO");
    Check(video::FramebufferNeedsReallocation(400, 240, 800, 240),
          "solo el ancho crece");
    Check(video::FramebufferNeedsReallocation(400, 240, 400, 480),
          "solo el alto crece");
    std::printf("\n");
}

} // namespace

int main() {
    std::printf("AlmenEMU video tests (lógica pura del backend)\n");

    TestPixelFormats();
    TestTextureUVs();
    TestReadbackRows();
    TestFramebufferPolicy();

    std::printf("\n%d comprobaciones, %d fallos\n", tests::checks, tests::failures);
    return tests::failures == 0 ? 0 : 1;
}
