#pragma once

// Choose build target
// PC simulator uses SDL for input and output
// Target uses fbdev and evdev directly
// Should be defined in Makefile
//#define BUILD_PC_SIM
//#define BUILD_NEKOINK

// Target resolution
#if defined(BUILD_PC_SIM)
#define TITLE "IMGVIEW"
#define TARGET_FPS (30)
#define DISP_WIDTH (1448)
#define DISP_HEIGHT (1072)
#elif defined(BUILD_NEKOINK)
#define DISP_WIDTH (2232)
#define DISP_HEIGHT (1680)
#endif

#define DISP_GAMMA (2.2f)

#define ENABLE_COLOR

#ifdef ENABLE_COLOR
// Options only applies if COLOR is enabled
//#define ENABLE_LPF // Enable LPF to avoid jagged edges
//#define ENABLE_BRIGHTEN // Copy component to neighbor pixels, only valid on SIM
#endif

#define DEPTH_1BPP // monochrome
//#define DEPTH_2BPP // 4 grey / 64 color
//#define DEPTH_4BPP // 16 grey / 4096 color
//#define DEPTH_8BPP

// Define either one of them to enable dithering
#define DITHERING_ERROR_DIFFUSION
//#define DITHERING_ORDERED
//#define DITHERING_BLUE_NOISE

// Dithering options
#ifdef ENABLE_COLOR
    #define DITHERING_ERRBUF_LINES (4)
#else
    #define DITHERING_ERRBUF_LINES (2)
#endif
#define DITHERING_GAMMA_AWARE
