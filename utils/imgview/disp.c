//
// NekoInk Image Viewer
// Copyright 2021 Wenting Zhang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// File : disp.c
// Brief: Hardware display related functions
//
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include "config.h"
#include "disp.h"
#include "stb_image_resize.h"
#include "stb_image.h"

#if defined(BUILD_PC_SIM)
// Use SDL on PC SIM
#include <SDL.h>
#elif defined(BUILD_NEKOINK)
// Use FBDEV on NekoInk 1st gen
#endif

#if defined(BUILD_PC_SIM)
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
#elif defined(BUILD_NEKOINK)

#endif

static Canvas *screen;

static int disp_get_bpp(PixelFormat fmt) {
    switch(fmt) {
    case PIXFMT_Y1_PACKED:
        return 1;
    case PIXFMT_Y2_PACKED:
        return 2;
    case PIXFMT_Y4_PACKED:
        return 4;
    case PIXFMT_Y8:
    case PIXFMT_Y1_LSB:
    case PIXFMT_Y2_LSB:
    case PIXFMT_Y4_LSB:
    case PIXFMT_C1_LSB:
    case PIXFMT_C2_LSB:
    case PIXFMT_C4_LSB:
    case PIXFMT_C8:
        return 8;
    case PIXFMT_RGB565:
    case PIXFMT_RGB565_BE:
        return 16;
    case PIXFMT_RGB888:
        return 24;
    case PIXFMT_ARGB8888:
    case PIXFMT_ARGB8888_BE:
    case PIXFMT_RGBA8888:
    case PIXFMT_RGBA8888_BE:
        return 32;
    }
    return 0; // Unknown
}

static uint8_t disp_get_mask(PixelFormat fmt) {
    switch(fmt) {
    case PIXFMT_Y1_PACKED:
        return 0x01;
    case PIXFMT_Y2_PACKED:
        return 0x03;
    case PIXFMT_Y4_PACKED:
        return 0x0f;
    default:
        return 0x00;
    }
    return 0x00; // Unnecessary, but gcc gives a warning if I don't do so
}

// Framebuffer operation
Canvas *disp_create(int w, int h, PixelFormat fmt) {
    size_t size = w * h;
    int bpp = disp_get_bpp(fmt);
    if (bpp >= 8) {
        size *= (bpp / 8);
    }
    else {
        size /= (8 / bpp);
    }
    Canvas *canvas = malloc(sizeof(Canvas) + size);
    canvas->width = w;
    canvas->height = h;
    canvas->pixelFormat = fmt;
    return canvas;
}

void disp_free(Canvas *canvas) {
    free(canvas);
}

uint32_t disp_conv_pix(PixelFormat dst, PixelFormat src, uint32_t color) {
    int r = 0x00, g = 0x00, b = 0x00, a = 0xff;
    uint8_t y = (uint8_t)color;

    if (src == dst)
        return color;

    switch (src) {
    case PIXFMT_Y1_LSB:
        y = (color) ? 0xff : 0x00;
        r = g = b = y;
        break;
    case PIXFMT_Y2_LSB:
        y |= y << 2; __attribute__ ((fallthrough));
    case PIXFMT_Y4_LSB:
        y |= y << 4; __attribute__ ((fallthrough));
    case PIXFMT_Y8:
        r = g = b = y;
        break;
    case PIXFMT_RGB565:
        r = (color >> 8) & 0xf8;
        g = (color >> 3) & 0xfc;
        b = (color << 3) & 0xf8;
        r |= r >> 5;
        g |= g >> 6;
        b |= b >> 5;
        break;
    case PIXFMT_ARGB8888:
        a = (color >> 24) & 0xff; __attribute__ ((fallthrough));
    case PIXFMT_RGB888:
        r = (color >> 16) & 0xff;
        g = (color >> 8) & 0xff;
        b = (color) & 0xff;
        break;
    case PIXFMT_RGBA8888:
        r = (color >> 24) & 0xff;
        g = (color >> 16) & 0xff;
        b = (color >> 8) & 0xff;
        a = (color) & 0xff;
        break;
    case PIXFMT_ARGB8888_BE:
        b = (color >> 24) & 0xff;
        g = (color >> 16) & 0xff;
        r = (color >> 8) & 0xff;
        a = (color) & 0xff;
        break;
    case PIXFMT_RGBA8888_BE:
        a = (color >> 24) & 0xff;
        b = (color >> 16) & 0xff;
        g = (color >> 8) & 0xff;
        r = (color) & 0xff;
        break;
    default:
        assert(0);
    }

    y = (uint8_t)(r * 0.312f + g * 0.563f + b * 0.125f);
    uint32_t target = 0;
    switch (dst) {
    case PIXFMT_Y1_LSB:
        target = (y >> 7) & 0x1;
        break;
    case PIXFMT_Y2_LSB:
        target = (y >> 6) & 0x3;
        break;
    case PIXFMT_Y4_LSB:
        target = (y >> 4) & 0xf;
        break;
    case PIXFMT_Y8:
        target = y;
        break;
    case PIXFMT_RGB565:
        target = ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | ((b & 0xf8) >> 3);
        break;
    case PIXFMT_ARGB8888:
        target = (a << 24) | (r << 16) | (g << 8) | b;
        break;
    case PIXFMT_RGB888:
        target = (r << 16) | (g << 8) | b;
        break;
    case PIXFMT_RGBA8888:
        target = (r << 24) | (g << 16) | (b << 8) | a;
        break;
    default:
        assert(0);
    }
    return target;
}

// Caution: really slow
void disp_conv(Canvas *dst, Canvas *src) {
    uint8_t *src_raw8 = (uint8_t *)src->buf;
    uint16_t *src_raw16 = (uint16_t *)src->buf;
    uint32_t *src_raw32 = (uint32_t *)src->buf;
    uint8_t *dst_raw8 = (uint8_t *)dst->buf;
    uint16_t *dst_raw16 = (uint16_t *)dst->buf;
    uint32_t *dst_raw32 = (uint32_t *)dst->buf;

    for (int i = 0; i < (src->width * src->height); i++) {
        int bpp = disp_get_bpp(src->pixelFormat);
        uint32_t color;
        if (bpp == 8) {
            color = *src_raw8++;
        }
        else if (bpp == 16) {
            color = *src_raw16++;
        }
        else if (bpp == 24) {
            color = *src_raw8++;
            color <<= 8;
            color |= *src_raw8++;
            color <<= 8;
            color |= *src_raw8++;
        }
        else if (bpp == 32) {
            color = *src_raw32++;
        }
        else {
            assert(0);
        }
        color = disp_conv_pix(dst->pixelFormat, src->pixelFormat, color);
        bpp = disp_get_bpp(dst->pixelFormat);
        if (bpp == 8) {
            *dst_raw8++ = color;
        }
        else if (bpp == 16) {
            *dst_raw16++ = color;
        }
        else if (bpp == 24) {
            *dst_raw8++ = (color >> 16) & 0xff;
            *dst_raw8++ = (color >> 8) & 0xff;
            *dst_raw8++ = (color) & 0xff;
        }
        else if (bpp == 32) {
            *dst_raw32++ = color;
        }
        else {
            assert(0);
        }
    }
}

void disp_scale_image_fit(Canvas *src, Canvas *dst) {
    float scalex, scaley;
    scalex = (float)dst->width / (float)src->width;
    scaley = (float)dst->height / (float)src->height;
    int channels = disp_get_bpp(src->pixelFormat);
    // Source and dest should have the same pixel format
    assert(channels == disp_get_bpp(dst->pixelFormat));
    // The pixel format should not be packed
    assert(channels >= 8);
    channels /= 8;
    // Assume byte per pixel is equal to channel count
    // Fit the image into destination size keeping aspect ratio
    int outh, outw, outstride, outoffset;
    if (scalex > scaley) {
        outh = dst->height;
        outw = src->width * scaley;
        outoffset = ((dst->width - outw) / 2) * channels;
        outstride = dst->width * channels;
    }
    else {
        outw = dst->width;
        outh = src->height * scalex;
        outoffset = ((dst->height - outh) / 2) * channels * dst->width;
        outstride = 0;
    }
    stbir_resize_uint8(src->buf, src->width, src->height, 0,
            dst->buf + outoffset, outw, outh, outstride, channels);
}

// For a certain pixel, get its color component on the EPD screen,
// return in the RSH amount to get the component
static uint32_t get_panel_color_shift(int x, int y) {
    int c = (x + y) % 3;
    if (c == 0)
        return 16; // r
    else if (c == 1)
        return 0; // b
    else
        return 8; // g
}

static uint32_t get_panel_color_component(int x, int y) {
    int c = (x + y) % 3;
    if (c == 0)
        return 0; // r
    else if (c == 1)
        return 2; // b
    else
        return 1; // g
}

static float srgb_to_linear(uint8_t val) {
    float srgb = val / 255.0f;
    float linear = powf(srgb, DISP_GAMMA);
    //printf("srgb %f -> linear %f\n", srgb, linear);
    return linear;
}

static uint8_t linear_to_srgb(float val) {
    float srgb = powf(val, 1.0f / DISP_GAMMA);
    int32_t srgbi = srgb * 255.0f;
    if (srgbi > 255) srgbi = 255;
    if (srgbi < 0) srgbi = 0;
    return srgbi;
}

// Process image to be displayed on EPD
void disp_filtering_image(Canvas *src, Rect srcRect, Rect dstRect) {
    uint8_t *src_raw = (uint8_t *)src->buf;
#if defined(BUILD_PC_SIM)
    uint32_t *dst_raw = (uint32_t *)screen->buf;
#elif defined(BUILD_NEKOINK)
    uint8_t *dst_raw = screen->buf;
#endif
    uint32_t dst_w = screen->width;
    uint32_t src_x = srcRect.x;
    uint32_t src_y = srcRect.y;
    uint32_t w = srcRect.w;
    uint32_t h = srcRect.h;
    uint32_t dst_x = dstRect.x;
    uint32_t dst_y = dstRect.y;
    if ((w == 0) && (h == 0)) {
        w = src->width;
        h = src->height;
    }

#ifdef ENABLE_COLOR
    assert(src->pixelFormat == PIXFMT_RGB888);
#else
    assert(src->pixelFormat == PIXFMT_Y8);
#endif

#ifdef ENABLE_DITHERING
    #ifdef DITHERING_GAMMA_AWARE
    float *err_buf = malloc(w * DITHERING_ERRBUF_LINES * sizeof(float));
    #else
    int32_t *err_buf = malloc(w * DITHERING_ERRBUF_LINES * sizeof(int32_t));
    #endif
    assert(err_buf);
#endif

#define SRC_PIX(x, y, comp) src_raw[((src_y + y) * w + src_x + x) * 3 + comp]
#define DST_PIX(x, y) dst_raw[(dst_y + y) * dst_w + dst_x + x]

    // Convert to 8bpp in target buffer
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t pix;
#ifdef ENABLE_COLOR
            uint32_t comp = get_panel_color_component(dst_x + x, dst_y + y);
            pix = SRC_PIX(x, y, comp);
    #ifdef ENABLE_LPF
            // Low pass filtering to reduce the color/ jagged egdes
            uint32_t pix_u = (y == 0) ? pix : SRC_PIX(x, y - 1, comp);
            uint32_t pix_d = (y == (h - 1)) ? pix : SRC_PIX(x, y + 1, comp);
            uint32_t pix_l = (x == 0) ? pix : SRC_PIX(x - 1, y, comp);
            uint32_t pix_r = (x == (w - 1)) ? pix : SRC_PIX(x + 1, y, comp);
            pix = pix >> 1; // /2
            pix_u = pix_u >> 3; // /8
            pix_d = pix_d >> 3;
            pix_l = pix_l >> 3;
            pix_r = pix_r >> 3;
            pix = pix + pix_u + pix_d + pix_l + pix_r;
    #endif
#else
            pix = src_raw[(src_y + y) * w + src_x + x];
#endif
            DST_PIX(x, y) = pix;
        }
    }

#ifdef ENABLE_DITHERING
    memset(err_buf, 0, w * DITHERING_ERRBUF_LINES * sizeof(*err_buf));
#endif

    // Quantize color into requested bit depth and do optional dithering
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int32_t pix = (int32_t)DST_PIX(x, y);
    
#ifdef ENABLE_DITHERING
            // Add in error term
    #ifdef DITHERING_GAMMA_AWARE
            float pix_linear = srgb_to_linear(pix);
            pix_linear += err_buf[(y % DITHERING_ERRBUF_LINES) * w + x];
            pix = linear_to_srgb(pix_linear);
    #else
            pix += err_buf[(y % DITHERING_ERRBUF_LINES) * w + x];
    #endif
#endif

            // Quantize the pixel down to the bpp required
            int32_t new_pix;
#ifdef DEPTH_1BPP
            new_pix = (pix & 0x80) ? 0xff : 0x00;
#elif (defined(DEPTH_2BPP))
            new_pix = pix & 0xc0;
            new_pix |= new_pix >> 2;
            new_pix |= new_pix >> 4;
#elif (defined(DEPTH_4BPP))
            new_pix = pix & 0xf0;
            new_pix |= new_pix >> 4;
#elif (defined(DEPTH_8BPP))
            new_pix = pix;
#endif

#ifdef ENABLE_DITHERING
            // Apply clipping
            if (pix < 0)
                new_pix = 0;
            else if (pix > 255)
                new_pix = 255;

            // Use error-diffusion dithering.
    #ifdef DITHERING_GAMMA_AWARE
            float quant_error = pix_linear - srgb_to_linear(new_pix);
    #else
            int32_t quant_error = (int32_t)pix - (int32_t)new_pix;
    #endif

            pix = new_pix;

    #define DIFFUSE_ERROR(x, y, w, h, error, factor, sum)\
            if (((y) > 0) && ((x) > 0) && ((y) < h) && ((x) < w)) \
                err_buf[((y) % DITHERING_ERRBUF_LINES) * w + x] += error * factor / sum;
    
    #ifdef ENABLE_COLOR
            // . . * . . 1
            // . 2 . . 3 .
            // 4 . . 5 . .
            // . . 6 . . .
            // Star is the pixel in question, the error is pushed to the pixels
            // labeled 1-6 (neighboring pixels in the same color).
            DIFFUSE_ERROR(x + 3, y + 0, w, h, quant_error, 2, 16); // D=3
            DIFFUSE_ERROR(x - 1, y + 1, w, h, quant_error, 5, 16); // D=1.4
            DIFFUSE_ERROR(x + 2, y + 1, w, h, quant_error, 3, 16); // D=1.7
            DIFFUSE_ERROR(x - 2, y + 2, w, h, quant_error, 2, 16); // D=2.8
            DIFFUSE_ERROR(x + 1, y + 2, w, h, quant_error, 3, 16); // D=1.7
            DIFFUSE_ERROR(x    , y + 3, w, h, quant_error, 1, 16); // D=3
    #else
            #if 0
            // Floyd-Steinberg
            // . * 1
            // 2 3 4
            // Star is the pixel in question, the error is pushed to the pixels
            // labeled 1-4
            DIFFUSE_ERROR(x + 1, y + 0, w, h, quant_error, 7, 16);
            DIFFUSE_ERROR(x - 1, y + 1, w, h, quant_error, 3, 16);
            DIFFUSE_ERROR(x    , y + 1, w, h, quant_error, 5, 16);
            DIFFUSE_ERROR(x + 1, y + 1, w, h, quant_error, 1, 16);
            #else
            // Two-Row Sierra
            DIFFUSE_ERROR(x + 1, y + 0, w, h, quant_error, 4, 16);
            DIFFUSE_ERROR(x + 2, y + 0, w, h, quant_error, 3, 16);
            DIFFUSE_ERROR(x - 2, y + 1, w, h, quant_error, 1, 16);
            DIFFUSE_ERROR(x - 1, y + 1, w, h, quant_error, 2, 16);
            DIFFUSE_ERROR(x,     y + 1, w, h, quant_error, 3, 16);
            DIFFUSE_ERROR(x + 1, y + 1, w, h, quant_error, 2, 16);
            DIFFUSE_ERROR(x + 2, y + 1, w, h, quant_error, 1, 16);
            #endif
    #endif
#else
            // Does not apply dithering
            pix = new_pix;
#endif
            DST_PIX(x, y) = pix;
        }
#ifdef ENABLE_DITHERING
        // Clear errbuf of current line
        memset(&err_buf[(y % DITHERING_ERRBUF_LINES) * w], 0, w * sizeof(*err_buf));
#endif
    }

#ifdef ENABLE_DITHERING
    free(err_buf);
#endif

#if defined(BUILD_PC_SIM)
    // Reformat for ARGB8888 buffer
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t pix = DST_PIX(x, y);
    #ifdef ENABLE_COLOR
            uint32_t shift = get_panel_color_shift(dst_x + x, dst_y + y);
            pix <<= shift;
            //pix |= (pix << 16) | (pix << 8);
    #else
            pix |= (pix << 16) | (pix << 8);
    #endif
            pix |= 0xff000000;
            DST_PIX(x, y) = pix;
        }
    }

    #ifdef ENABLE_BRIGHTEN
    // Brighten image, not recommended
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < (w - 1); x++) {
            uint32_t pix = DST_PIX(x, y);
            uint32_t shift = get_panel_color_shift(x, y);
            uint32_t cm = 0xff << shift;
            pix &= cm;
            DST_PIX(x + 1, y) |= pix;
            if (y < (h - 1))
                DST_PIX(x + 1, y + 1) |= pix;
        }
    }
    #endif

    uint32_t *texture_pixels;
    int texture_pitch;
    SDL_LockTexture(texture, NULL, (void **)&texture_pixels, &texture_pitch);
    assert(texture_pitch == (screen->width * 4));
    memcpy(texture_pixels, screen->buf, screen->height * texture_pitch);
    SDL_UnlockTexture(texture);
#elif defined(BUILD_NEKOINK)

#endif

}

void disp_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "SDL initialization error %s\n", SDL_GetError());
        exit(1);
    }

    window = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED, DISP_WIDTH, DISP_HEIGHT,
            SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    
    int w, h;
    SDL_GL_GetDrawableSize(window, &w, &h);
    // Detect 2X HiDPI screen, if high dpi, set to 1X size
    if (w == DISP_WIDTH * 2) {
        SDL_SetWindowSize(window, DISP_WIDTH / 2, DISP_HEIGHT / 2);
        SDL_GL_GetDrawableSize(window, &w, &h);
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, w, h);
    
    screen = disp_create(w, h, PIXFMT_ARGB8888);
}

void disp_deinit(void) {
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void disp_present(void) {
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

Canvas *disp_load_image(char *filename) {
    int x, y, n;
    unsigned char *data = stbi_load(filename, &x, &y, &n, 0);
    PixelFormat fmt;
    if (n == 1)
        fmt = PIXFMT_Y8;
    else if (n == 3)
        fmt = PIXFMT_RGB888;
    else if (n == 4)
        fmt = PIXFMT_RGBA8888_BE;
    else
        return NULL; // YA88 not supported
    Canvas *canvas = disp_create(x, y, fmt);
    memcpy(canvas->buf, data, x * y * n);
    return canvas;
}
