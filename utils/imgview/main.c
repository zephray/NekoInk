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
// File : main.c
// Brief: Main entry point of the application
//
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL.h>
#include <SDL_image.h>
#include "config.h"

#define PROFILE(x) { \
    clock_t t = clock();\
    x;\
    t = clock() - t;\
    printf("Time took: %.2f ms\n", (double)t / CLOCKS_PER_SEC * 1000);\
}

static void scale_image_fit(SDL_Surface *src, SDL_Surface *dst) {
    float scalex, scaley;
    scalex = (float)dst->w / (float)src->w;
    scaley = (float)dst->h / (float)src->h;
    // Use the smaller scale
    SDL_Rect dst_rect;
    if (scalex > scaley) {
        dst_rect.h = dst->h;
        dst_rect.w = src->w * scaley;
        dst_rect.x = (dst->w - dst_rect.w) / 2;
        dst_rect.y = 0;
    }
    else {
        dst_rect.w = dst->w;
        dst_rect.h = src->h * scalex;
        dst_rect.y = (dst->h - dst_rect.h) / 2;
        dst_rect.x = 0;
    }
    SDL_FillRect(dst, NULL, 0xff000000);
    SDL_BlitScaled(src, NULL, dst, &dst_rect);
}

// For a certain pixel, get its color component on the EPD screen,
// return in the RSH amount to get the component
static uint32_t get_panel_color_shift(int x, int y) {
    int c = (x + y) % 3;
    if (c == 0)
        return 16;
    else if (c == 1)
        return 0;
    else
        return 8;
}

static uint32_t get_panel_color_shift2(int c) {
    if (c == 0)
        return 16;
    else if (c == 1)
        return 0;
    else
        return 8;
}

// Process image to be displayed on EPD
static void filtering_image(SDL_Surface *src, SDL_Surface *dst) {
    uint32_t *src_raw = src->pixels;
    uint32_t *dst_raw = dst->pixels;
    uint32_t w = src->w;
    uint32_t h = src->h;
    //uint32_t c = 0;

    // Convert to 8bpp in target buffer
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t pix = src_raw[y * w + x];
#ifdef ENABLE_COLOR
            uint32_t shift = get_panel_color_shift(x, y);
            /*uint32_t shift = get_panel_color_shift2(c);
            c++;
            if (c == 3) c = 0;*/
            pix = (pix >> shift) & 0xff;
#else
            uint32_t r = (pix >> 16) & 0xff;
            uint32_t g = (pix >> 8) & 0xff;
            uint32_t b = (pix) & 0xff;
            pix = (uint32_t)(r * 0.312f + g * 0.563f + b * 0.125f);
            if (pix > 255) pix = 255;
#endif
            dst_raw[y * w + x] = pix;
        }
    }

#ifdef ENABLE_LPF
    // Low pass filtering is necessary to reduce the color/ jagged egdes as
    // applying color masking to the screen is essentially down-sampling.
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t pix = dst_raw[y * w + x];
            uint32_t pix_u = (y == 0) ? pix : dst_raw[(y - 1) * w + x];
            uint32_t pix_d = (y == (h - 1)) ? pix : dst_raw[(y + 1) * w + x];
            uint32_t pix_l = (x == 0) ? pix : dst_raw[y * w + x - 1];
            uint32_t pix_r = (x == (w - 1)) ? pix : dst_raw[y * w + x + 1];
            pix = pix >> 1; // /2
            pix_u = pix_u >> 3; // /8
            pix_d = pix_d >> 3;
            pix_l = pix_l >> 3;
            pix_r = pix_r >> 3;
            pix = pix + pix_u + pix_d + pix_l + pix_r;
            dst_raw[y * w + x] = pix;
        }
    }
#endif

    // Quantize color into requested bit depth and do optional dithering
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t pix = dst_raw[y * w + x];
            // Quantize the pixel down to the bpp required
            uint32_t new_pix;
#ifdef DEPTH_1BPP
            new_pix = (pix & 0x80) ? 0xff : 0x00;
#elif (defined(DEPTH_2BPP))
            new_pix = pix & 0xc0;
            new_pix |= new_pix >> 2;
            new_pix |= new_pix >> 4;
#elif (defined(DEPTH_4BPP))
            new_pix = pix & 0xf0;
            new_pix |= new_pix >> 4;
#endif

#ifdef ENABLE_DITHERING
            // Apply clipping
            if ((int32_t)pix < 0)
                new_pix = 0;
            else if ((int32_t)pix > 255)
                new_pix = 255;

            // Use error-diffusion dithering.
            // Un-quantized pixels would be int32_t to allow negative numbers
            int32_t quant_error = (int32_t)pix - (int32_t)new_pix;

            //printf("Old %02x, New %02x　, Err %d\n", pix, new_pix, quant_error);

            pix = new_pix;

#define DIFFUSE_ERROR(buf, x, y, w, h, error, factor, sum)\
        { \
            uint32_t addr = (y) * w + x; \
            if (((y) > 0) && ((x) > 0) && ((y) < h) && ((x) < w)) \
                buf[addr] = ((int32_t)buf[addr] + error * factor / sum); \
        }
        

    #ifdef ENABLE_COLOR
            // . . * . . 1
            // . 2 . . 3 .
            // 4 . . 5 . .
            // . . 6 . . .
            // Star is the pixel in question, the error is pushed to the pixels
            // labeled 1-6 (neighboring pixels in the same color).
            DIFFUSE_ERROR(dst_raw, x + 3, y, w, h, quant_error, 2, 16); // D=3
            DIFFUSE_ERROR(dst_raw, x - 1, y + 1, w, h, quant_error, 5, 16); // D=1.4
            DIFFUSE_ERROR(dst_raw, x + 2, y + 1, w, h, quant_error, 3, 16); // D=1.7
            DIFFUSE_ERROR(dst_raw, x - 2, y + 2, w, h, quant_error, 2, 16); // D=2.8
            DIFFUSE_ERROR(dst_raw, x + 1, y + 2, w, h, quant_error, 3, 16); // D=1.7
            DIFFUSE_ERROR(dst_raw, x, y + 3, w, h, quant_error, 1, 16); // D=3
    #else
            #if 1
            // Floyd-Steinberg
            // . * 1
            // 2 3 4
            // Star is the pixel in question, the error is pushed to the pixels
            // labeled 1-4
            DIFFUSE_ERROR(dst_raw, x + 1, y, w, h, quant_error, 7, 16);
            DIFFUSE_ERROR(dst_raw, x - 1, y + 1, w, h, quant_error, 3, 16);
            DIFFUSE_ERROR(dst_raw, x, y + 1, w, h, quant_error, 5, 16);
            DIFFUSE_ERROR(dst_raw, x + 1, y + 1, w, h, quant_error, 1, 16);
            #else
            // Two-Row Sierra
            DIFFUSE_ERROR(dst_raw, x + 1, y,     w, h, quant_error, 4, 16);
            DIFFUSE_ERROR(dst_raw, x + 2, y,     w, h, quant_error, 3, 16);
            DIFFUSE_ERROR(dst_raw, x - 2, y + 1, w, h, quant_error, 1, 16);
            DIFFUSE_ERROR(dst_raw, x - 1, y + 1, w, h, quant_error, 2, 16);
            DIFFUSE_ERROR(dst_raw, x,     y + 1, w, h, quant_error, 3, 16);
            DIFFUSE_ERROR(dst_raw, x + 1, y + 1, w, h, quant_error, 2, 16);
            DIFFUSE_ERROR(dst_raw, x + 2, y + 1, w, h, quant_error, 1, 16);
            #endif
    #endif
#else
            // Does not apply dithering
            pix = new_pix;
#endif
            dst_raw[y * w + x] = pix;
        }
    }

    // Reformat for ARGB8888 buffer
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t pix = dst_raw[y * w + x];
#ifdef ENABLE_COLOR
            uint32_t shift = get_panel_color_shift(x, y);
            pix <<= shift;
#else
            pix |= (pix << 16) | (pix << 8);
#endif
            pix |= 0xff000000;
            dst_raw[y * w + x] = pix;
        }
    }

#ifdef ENABLE_BRIGHTEN
    // Brighten image, not recommended
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < (w - 1); x++) {
            uint32_t pix = dst_raw[y * w + x];
            uint32_t shift = get_panel_color_shift(x, y);
            uint32_t cm = 0xff << shift;
            pix &= cm;
            dst_raw[y * w + x + 1] |= pix;
            if (y < (h - 1))
                dst_raw[(y + 1) * w + x + 1] |= pix;
        }
    }
#endif
}

int main(int argc, char *argv[]) {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Surface *screen;
    SDL_Surface *target;

    if (argc < 2) {
        fprintf(stderr, "Usage: imgview <path_to_image>\n");
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "SDL initialization error %s\n", SDL_GetError());
        return 1;
    }
    if (!IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG)) {
        fprintf(stderr, "SDL image initialization error %s\n", SDL_GetError());
        return 1;
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

    target = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    screen = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);

    SDL_Surface *image = IMG_Load(argv[1]);
    if (!image) {
        fprintf(stderr, "Failed to load image %s\n", argv[1]);
        return 1;
    }

    PROFILE(scale_image_fit(image, target));
    PROFILE(filtering_image(target, screen));

    texture = SDL_CreateTextureFromSurface(renderer, screen);

    SDL_Event event;
    float time_delta = 0.0f;
    int last_ticks = SDL_GetTicks();
    bool running = true;

    while (running) {
        int cur_ticks = SDL_GetTicks();
        time_delta -= cur_ticks - last_ticks; // Actual ticks passed since last iteration
        time_delta += 1000.0f / (float)TARGET_FPS; // Time allocated for this iteration
        last_ticks = cur_ticks;

        // Process SDL events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            else {
                // handle events
            }
        }

        // Render objects
        if (time_delta > 0) {
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }

        // Wait for next frame
        int time_to_wait = time_delta - (SDL_GetTicks() - last_ticks);
        if (time_to_wait > 0)
            SDL_Delay(time_to_wait);
    }

    SDL_FreeSurface(target);

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}