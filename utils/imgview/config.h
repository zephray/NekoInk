#pragma once

// Choose build target
// PC simulator uses SDL for input and output
// Target uses fbdev and evdev directly
#define BUILD_PC_SIM
#undef BUILD_TARGET

// Target resolution
#if defined(BUILD_PC_SIM)
#define TITLE "IMGVIEW"
#define TARGET_FPS (30)
#define DISP_WIDTH (2232)
#define DISP_HEIGHT (1680)
#elif defined(BUILD_TARGET)
#define DISP_WIDTH (2232)
#define DISP_HEIGHT (1680)
#endif

#define ENABLE_COLOR

#ifdef ENABLE_COLOR
// Options only applies if COLOR is enabled
#define ENABLE_LPF // Enable LPF to avoid jagged edges
//#define ENABLE_BRIGHTEN // Copy component to neighbor pixels, only valid on SIM
#endif

//#define DEPTH_1BPP // monochrome
//#define DEPTH_2BPP // 4 grey / 64 color
#define DEPTH_4BPP // 16 grey / 4096 color

#define ENABLE_DITHERING
