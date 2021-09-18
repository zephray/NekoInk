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
#include "config.h"
#include "disp.h"


#define PROFILE(x) { \
    clock_t t = clock();\
    x;\
    t = clock() - t;\
    printf("%.2f ms\n", (double)t / CLOCKS_PER_SEC * 1000);\
}

void dump_hex(uint8_t *buf, int count) {
    for (int i = 0; i < count / 16; i++) {
        for (int j = 0; j < 16; j++) {
            printf("%02x ", *buf++);
        }
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: imgview <path_to_image>\n");
        return 1;
    }

#ifdef ENABLE_COLOR
    Canvas *target = disp_create(DISP_WIDTH, DISP_HEIGHT, PIXFMT_RGB888);
#else
    Canvas *target = disp_create(DISP_WIDTH, DISP_HEIGHT, PIXFMT_Y8);
#endif

    disp_init();

    Canvas *image;
    
    printf("Loading image: ");
    PROFILE(image = disp_load_image(argv[1]));

    if (image->pixelFormat != target->pixelFormat) {
        Canvas *image_new = disp_create(image->width, image->height, target->pixelFormat);

        printf("Converting image: ");
        PROFILE(disp_conv(image_new, image));
        disp_free(image);
        image = image_new;
    }

    Rect zero_rect = {0};

    printf("Scaling image: ");
    PROFILE(disp_scale_image_fit(image, target));

    printf("Filtering image: ");
    PROFILE(disp_filtering_image(target, zero_rect, zero_rect));
    disp_present();

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

        // Wait for next frame
        int time_to_wait = time_delta - (SDL_GetTicks() - last_ticks);
        if (time_to_wait > 0)
            SDL_Delay(time_to_wait);
    }

    disp_deinit();

    return 0;
}