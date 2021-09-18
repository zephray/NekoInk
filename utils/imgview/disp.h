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
// File : disp.h
// Brief: Hardware display related functions
//
#pragma once

typedef enum {
    // Greyscale packed formats
    PIXFMT_Y1_PACKED, // 8 pixels per byte
    PIXFMT_Y2_PACKED, // 4 pixels per byte
    PIXFMT_Y4_PACKED, // 2 pixels per byte
    PIXFMT_Y8, // 1 pixels per byte
    // Greyscale formats in LSB
    PIXFMT_Y1_LSB,
    PIXFMT_Y2_LSB,
    PIXFMT_Y4_LSB,
    // Color formats
    PIXFMT_RGB888, // Always BE
    // Color formats, assuming little endian (preferred)
    PIXFMT_RGB565,
    PIXFMT_ARGB8888, // Actually BGRA8888
    PIXFMT_RGBA8888, // Actually ABGR8888
    // Color formats, using big endian
    PIXFMT_RGB565_BE,
    PIXFMT_ARGB8888_BE,
    PIXFMT_RGBA8888_BE,
    // Color formats for non-standard CFA screen (dot vs pixel)
    PIXFMT_C1_LSB,
    PIXFMT_C2_LSB,
    PIXFMT_C4_LSB,
    PIXFMT_C8
} PixelFormat;

typedef enum {
    WVMD_INIT = 0,
    WVMD_DU = 1,
    WVMD_GC16 = 2,
    WVMD_GC4 = 3,
    WVMD_A2 = 4
} WaveformMode;

typedef struct {
    int width;
    int height;
    PixelFormat pixelFormat;
    uint8_t buf[];
} Canvas;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} Rect;

Canvas *disp_create(int w, int h, PixelFormat fmt);
void disp_free(Canvas *canvas);
void disp_conv(Canvas *dst, Canvas *src);
void disp_scale_image_fit(Canvas *src, Canvas *dst);
void disp_filtering_image(Canvas *src, Rect src_rect, Rect dst_rect);
void disp_init(void);
void disp_deinit(void);
void disp_present(Rect dest_rect, WaveformMode mode, bool wait);
Canvas *disp_load_image(char *filename);