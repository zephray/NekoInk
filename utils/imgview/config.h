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
#define DISP_WIDTH (1024)
#define DISP_HEIGHT (768)
#elif defined(BUILD_TARGET)
#define DISP_WIDTH (1872)
#define DISP_HEIGHT (1404)
#endif

#define ENABLE_LPF
