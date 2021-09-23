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
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include "config.h"
#include "disp.h"
#include "stb_image_resize.h"
#include "stb_image.h"

#if defined(BUILD_PC_SIM)
// Use SDL on PC SIM
#include <SDL.h>
#elif defined(BUILD_NEKOINK)
// Use FBDEV on NekoInk 1st gen
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/mxcfb.h>
#include <sys/mman.h>
#endif

#if defined(BUILD_PC_SIM)
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
#elif defined(BUILD_NEKOINK)
uint32_t marker_value = 0;
int fd_fbdev;
int fb_virtual_x;
size_t fb_size;
struct fb_var_screeninfo var_screeninfo;
uint8_t *fbdev_fb;
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
    int c = (x + (DISP_HEIGHT - y)) % 3;
    if (c == 0)
        return 16; // r
    else if (c == 1)
        return 0; // b
    else
        return 8; // g
}

static uint32_t get_panel_color_component(int x, int y) {
    int c = (x + (DISP_HEIGHT - y)) % 3;
    if (c == 0)
        return 0; // r
    else if (c == 1)
        return 2; // b
    else
        return 1; // g
}

static float degamma_table[256];
static uint8_t gamma_table[256];

static void build_gamma_table(void) {
    for (int i = 0; i < 256; i++) {
        degamma_table[i] = powf(i / 255.0f, DISP_GAMMA);
        gamma_table[i] = powf(i / 255.0f, 1.0f / DISP_GAMMA) * 255.0f;
    }
}

static float srgb_to_linear(uint8_t val) {
    return degamma_table[val];
}

#if 0
// Accurate version
static uint8_t linear_to_srgb(float val) {
    int low = 0, high = 255, mid;
    while ((high - low) > 1) {
        mid = (low + high) / 2;
        if (val > gamma_table[mid])
            low = mid;
        else
            high = mid;
    }
    float difflow = fabsf(val - gamma_table[low]);
    float diffhigh = fabsf(val - gamma_table[high]);
    int srgb = (difflow < diffhigh) ? low : high;
    return srgb;
}
#else
// Not accurate, good enough for 4bpp
static uint8_t linear_to_srgb(float val) {
    size_t index = (size_t)(val * 255.0f);
    return gamma_table[index];
}
#endif

// Process image to be displayed on EPD
void disp_filtering_image(Canvas *src, Rect src_rect, Rect dst_rect) {
    uint8_t *src_raw = (uint8_t *)src->buf;
#if defined(BUILD_PC_SIM)
    uint32_t *dst_raw = (uint32_t *)screen->buf;
#elif defined(BUILD_NEKOINK)
    uint8_t *dst_raw = (uint8_t *)screen->buf;
#endif
    uint32_t dst_w = screen->width;
    uint32_t src_x = src_rect.x;
    uint32_t src_y = src_rect.y;
    uint32_t w = src_rect.w;
    uint32_t h = src_rect.h;
    uint32_t dst_x = dst_rect.x;
    uint32_t dst_y = dst_rect.y;
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
    // TODO: Directly write into FB?
    uint8_t *wrptr = fbdev_fb;
    uint8_t *rdptr = dst_raw;
    for (int i = 0; i < screen->height; i++) {
        memcpy(wrptr, rdptr, screen->width);
        wrptr += fb_virtual_x;
        rdptr += screen->width;
    }
#endif

}

void disp_init(void) {

#if (defined(ENABLE_DITHERING) && defined(DITHERING_GAMMA_AWARE))
    build_gamma_table();
#endif

#if defined(BUILD_PC_SIM)
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
#elif defined(BUILD_NEKOINK)
    char devname[] = "/dev/fbxx";
    char epdcid[] = "mxc_epdc_fb";
    struct fb_fix_screeninfo fix_screeninfo;
    bool found = false;
    // Try to detect epdc fb device
    for (int i = 0; i < 3; i++) {
        sprintf(devname, "/dev/fb%d", i);
        fd_fbdev = open(devname, O_RDWR, 0);
        if (fd_fbdev < 0) {
            fprintf(stderr, "Failed to open fbdev %s\n", devname);
            exit(1);
        }
        if (ioctl(fd_fbdev, FBIOGET_FSCREENINFO, &fix_screeninfo) < 0) {
            fprintf(stderr, "Failed to get fixed screeninfo for %s\n", devname);
            exit(1);
        }
        if (!strcmp(fix_screeninfo.id, epdcid)) {
            printf("Opened EPDC device %s\n", devname);
            found = true;
            break;
        }
    }
    if (!found) {
        fprintf(stderr, "Failed to find and open EPDC device.\n");
        exit(1);
    }

    if (ioctl(fd_fbdev, FBIOGET_VSCREENINFO, &var_screeninfo) < 0) {
        fprintf(stderr, "Failed to get variable screeninfo\n");
        exit(1);
    }

    var_screeninfo.rotate = FB_ROTATE_UR;
    var_screeninfo.bits_per_pixel = 8;
    var_screeninfo.grayscale = GRAYSCALE_8BIT;
    var_screeninfo.yoffset = 0;
    var_screeninfo.activate = FB_ACTIVATE_FORCE;
    if (ioctl(fd_fbdev, FBIOPUT_VSCREENINFO, &var_screeninfo) < 0) {
        fprintf(stderr, "Failed to set screen mode\n");
        exit(1);
    }

    int w, h;
    w = var_screeninfo.xres_virtual;
    h = var_screeninfo.yres_virtual;
    fb_size = w * h * var_screeninfo.bits_per_pixel / 8;
    fb_virtual_x = w;
    printf("Virtual screen size: %d x %d\n", w, h);

    w = var_screeninfo.xres;
    h = var_screeninfo.yres;
    printf("Actual screen size: %d x %d\n", w, h);

    fbdev_fb = (uint8_t *)mmap(0, fb_size,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd_fbdev, 0);
    if ((int32_t)fbdev_fb <= 0) {
        fprintf(stderr, "Failed to set screen mode\n");
        exit(1);
    }

    // Disable auto update mode (region mode)
    uint32_t auto_update_mode = AUTO_UPDATE_MODE_REGION_MODE;
    if (ioctl(fd_fbdev, MXCFB_SET_AUTO_UPDATE_MODE, &auto_update_mode) < 0) {
        fprintf(stderr, "Failed to set auto update mode\n");
        exit(1);
    }

    // Setup waveform mode for auto wave mode (not used)
    struct mxcfb_waveform_modes waveform_modes;
    waveform_modes.mode_init = (uint32_t)WVMD_INIT;
    waveform_modes.mode_du   = (uint32_t)WVMD_DU;
    waveform_modes.mode_gc4  = (uint32_t)WVMD_GC4;
    waveform_modes.mode_gc8  = (uint32_t)WVMD_GC16;
    waveform_modes.mode_gc16 = (uint32_t)WVMD_GC16;
    waveform_modes.mode_gc32 = (uint32_t)WVMD_GC16;
    if (ioctl(fd_fbdev, MXCFB_SET_WAVEFORM_MODES, &waveform_modes) < 0) {
        fprintf(stderr, "Failed to set waveform mode\n");
        exit(1);
    }

    // Set update scheme
    uint32_t scheme = UPDATE_SCHEME_QUEUE_AND_MERGE;
    if (ioctl(fd_fbdev, MXCFB_SET_UPDATE_SCHEME, &scheme) < 0) {
        fprintf(stderr, "Failed to set update scheme\n");
        exit(1);
    }

    // Set power down delay
    uint32_t powerdown_delay = 0;
    if (ioctl(fd_fbdev, MXCFB_SET_PWRDOWN_DELAY, &powerdown_delay) < 0) {
        fprintf(stderr, "Failed to set power down delay\n");
    }

    screen = disp_create(w, h, PIXFMT_Y8);

    // Clear screen
    Rect zero_rect = {0};
    memset(fbdev_fb, 0xff, fb_size);
    disp_present(zero_rect, WVMD_INIT, false, true);
    //memset(fbdev_fb, 0x00, fb_size);
    //disp_present(zero_rect, WVMD_GC16, true, true);
#endif
}

void disp_deinit(void) {
#if defined(BUILD_PC_SIM)
    SDL_DestroyWindow(window);
    SDL_Quit();
#elif defined(BUILD_NEKOINK)
    munmap(fbdev_fb, fb_size);
    close(fd_fbdev);
#endif
}

void disp_present(Rect dest_rect, WaveformMode mode, bool partial, bool wait) {
#if defined(BUILD_PC_SIM)
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
#elif defined(BUILD_NEKOINK)
    if ((dest_rect.w == 0) && (dest_rect.h == 0)) {
        dest_rect.w = screen->width;
        dest_rect.h = screen->height;
    }
    struct mxcfb_update_data update_data;
    struct mxcfb_update_marker_data update_marker_data;

    update_data.update_mode = partial ? UPDATE_MODE_PARTIAL : UPDATE_MODE_FULL;
    update_data.waveform_mode = mode;
    update_data.update_region.left = dest_rect.x;
    update_data.update_region.top = dest_rect.y;
    update_data.update_region.width = dest_rect.w;
    update_data.update_region.height = dest_rect.h;
    update_data.temp = TEMP_USE_AMBIENT;
    update_data.flags = 0;

    if (wait)
        update_data.update_marker = ++marker_value;
    else
        update_data.update_marker = 0;

    if (ioctl(fd_fbdev, MXCFB_SEND_UPDATE, &update_data) < 0) {
        fprintf(stderr, "Failed sending udpdate\n");
        return;
    }

    if (wait) {
        update_marker_data.update_marker = marker_value;
        if (ioctl(fd_fbdev, MXCFB_WAIT_FOR_UPDATE_COMPLETE,
                &update_marker_data) < 0) {
            fprintf(stderr, "Failed waiting for update complete\n");
        }
    }
#endif
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
