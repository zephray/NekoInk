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

    SDL_BlitScaled(src, NULL, dst, &dst_rect);
}

// For a certain pixel, get its color component on the EPD screen,
// return in mask
static uint32_t get_panel_color_mask(int x, int y) {
    int c = (x + y) % 3;
    if (c == 0)
        return 0x00ff0000;
    else if (c == 1)
        return 0x000000ff;
    else
        return 0x0000ff00;
}

// Process image to be displayed on EPD
static void filtering_image(SDL_Surface *src, SDL_Surface *dst) {
    uint32_t *src_raw = src->pixels;
    uint32_t *dst_raw = dst->pixels;
    uint32_t w = src->w;
    uint32_t h = src->h;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t cm = get_panel_color_mask(x, y);
            uint32_t pix = src_raw[y * w + x];
#ifdef ENABLE_LPF
            uint32_t pix_u = (y == 0) ? pix : src_raw[(y - 1) * w + x];
            uint32_t pix_d = (y == (h - 1)) ? pix : src_raw[(y + 1) * w + x];
            uint32_t pix_l = (x == 0) ? pix : src_raw[y * w + x - 1];
            uint32_t pix_r = (x == (w - 1)) ? pix : src_raw[y * w + x + 1];
            pix = ((pix & cm) >> 1) & cm; // /2
            pix_u = ((pix_u & cm) >> 3) & cm; // /8
            pix_d = ((pix_d & cm) >> 3) & cm;
            pix_l = ((pix_l & cm) >> 3) & cm;
            pix_r = ((pix_r & cm) >> 3) & cm;
            pix = pix + pix_u + pix_d + pix_l + pix_r;
#endif
            pix &= cm;
            dst_raw[y * w + x] = pix;
        }
    }
}

int main(int argc, char *argv[]) {
    SDL_Window *window;
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
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "SDL image initialization error %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED, DISP_WIDTH, DISP_HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI);
    screen = SDL_GetWindowSurface(window);
    memset(screen->pixels, 0x00, screen->pitch * screen->h);

    target = SDL_CreateRGBSurfaceWithFormat(0, DISP_WIDTH,
            DISP_HEIGHT, 32, SDL_PIXELFORMAT_ARGB8888);

    SDL_Surface *image = IMG_Load(argv[1]);
    if (!image) {
        fprintf(stderr, "Failed to load image %s\n", argv[1]);
        return 1;
    }

    scale_image_fit(image, target);
    filtering_image(target, screen);

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
            SDL_UpdateWindowSurface(window);
        }
        else
            printf("Skipping one frame\n");

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