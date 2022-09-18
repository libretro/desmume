#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <retro_inline.h>

#ifndef HAVE_NO_LANGEXTRA
#include "libretro_core_options_intl.h"
#endif

/*
 ********************************
 * VERSION: 2.0
 ********************************
 *
 * - 2.0: Add support for core options v2 interface
 * - 1.3: Move translations to libretro_core_options_intl.h
 *        - libretro_core_options_intl.h includes BOM and utf-8
 *          fix for MSVC 2010-2013
 *        - Added HAVE_NO_LANGEXTRA flag to disable translations
 *          on platforms/compilers without BOM support
 * - 1.2: Use core options v1 interface when
 *        RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION is >= 1
 *        (previously required RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION == 1)
 * - 1.1: Support generation of core options v0 retro_core_option_value
 *        arrays containing options with a single value
 * - 1.0: First commit
*/

#ifdef __cplusplus
extern "C" {
#endif

/*
 ********************************
 * Core Option Definitions
 ********************************
*/

/* RETRO_LANGUAGE_ENGLISH */

/* Default language:
 * - All other languages must include the same keys and values
 * - Will be used as a fallback in the event that frontend language
 *   is not available
 * - Will be used as a fallback for any missing entries in
 *   frontend language definition */

struct retro_core_option_v2_category option_cats_us[] = {
   {
      "system",
      "System",
      "Configure system options."
   },
   {
      "video",
      "Graphics",
      "Configure graphics options."
   },
   {
      "screen",
      "Screen",
      "Configure screen options."
   },
   {
      "input",
      "Input",
      "Configure input options."
   },
   { NULL, NULL, NULL },
};

struct retro_core_option_v2_definition option_defs_us[] = {
    {
        "desmume_firmware_language",
        "Firmware Language",
        NULL,
        "Set language to use for firmware.",
        NULL,
        "system",
        {
            { "Auto",      NULL },
            { "Japanese",  NULL },
            { "English",   NULL },
            { "French",    NULL },
            { "German",    NULL },
            { "Italian",   NULL },
            { "Spanish",   NULL },
        },
        "Auto"
    },
    {
        "desmume_use_external_bios",
        "Use External BIOS/Firmware (restart)",
        NULL,
        "Use official BIOS. Core needs to be restarted for changes to apply.",
        NULL,
        "system",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "desmume_boot_into_bios",
        "Boot Into BIOS (interpreter and external bios only)",
        NULL,
        "Boot directly into the BIOS.",
        NULL,
        "system",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "desmume_load_to_memory",
        "Load Game Into Memory (restart)",
        NULL,
        "Load the game into memory to reduce IO lag.",
        NULL,
        "system",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "desmume_num_cores",
        "CPU Cores",
        NULL,
        "Set the amount of CPU cores to use.",
        NULL,
        "system",
        {
            { "1",  NULL },
            { "2",  NULL },
            { "3",  NULL },
            { "4",  NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "desmume_cpu_mode",
#if defined(IOS) || defined(ANDROID)
        "CPU Mode",
#else
        "CPU Mode (restart)",
#endif
        NULL,
        "Enable or disable the use of the dynamic recompiler.",
        NULL,
        "system",
        {
            { "interpreter",  NULL },
#ifdef HAVE_JIT
            { "jit",   NULL },
#endif
            { NULL, NULL },
        },
        "interpreter"
    },
#ifdef HAVE_JIT
    {
        "desmume_jit_block_size",
        "JIT Block Size",
        NULL,
        "Set the block size of the JIT dynamic recompiler.",
        NULL,
        "system",
        {
            { "0",   NULL },
            { "1",   NULL },
            { "2",   NULL },
            { "3",   NULL },
            { "4",   NULL },
            { "5",   NULL },
            { "6",   NULL },
            { "7",   NULL },
            { "8",   NULL },
            { "9",   NULL },
            { "10",  NULL },
            { "11",  NULL },
            { "12",  NULL },
            { "13",  NULL },
            { "14",  NULL },
            { "15",  NULL },
            { "16",  NULL },
            { "17",  NULL },
            { "18",  NULL },
            { "19",  NULL },
            { "20",  NULL },
            { "21",  NULL },
            { "22",  NULL },
            { "23",  NULL },
            { "24",  NULL },
            { "25",  NULL },
            { "26",  NULL },
            { "27",  NULL },
            { "28",  NULL },
            { "29",  NULL },
            { "30",  NULL },
            { "31",  NULL },
            { "32",  NULL },
            { "33",  NULL },
            { "34",  NULL },
            { "35",  NULL },
            { "36",  NULL },
            { "37",  NULL },
            { "38",  NULL },
            { "39",  NULL },
            { "40",  NULL },
            { "41",  NULL },
            { "42",  NULL },
            { "43",  NULL },
            { "44",  NULL },
            { "45",  NULL },
            { "46",  NULL },
            { "47",  NULL },
            { "48",  NULL },
            { "49",  NULL },
            { "50",  NULL },
            { "51",  NULL },
            { "52",  NULL },
            { "53",  NULL },
            { "54",  NULL },
            { "55",  NULL },
            { "56",  NULL },
            { "57",  NULL },
            { "58",  NULL },
            { "59",  NULL },
            { "60",  NULL },
            { "61",  NULL },
            { "62",  NULL },
            { "63",  NULL },
            { "64",  NULL },
            { "65",  NULL },
            { "66",  NULL },
            { "67",  NULL },
            { "68",  NULL },
            { "69",  NULL },
            { "70",  NULL },
            { "71",  NULL },
            { "72",  NULL },
            { "73",  NULL },
            { "74",  NULL },
            { "75",  NULL },
            { "76",  NULL },
            { "77",  NULL },
            { "78",  NULL },
            { "79",  NULL },
            { "80",  NULL },
            { "81",  NULL },
            { "82",  NULL },
            { "83",  NULL },
            { "84",  NULL },
            { "85",  NULL },
            { "86",  NULL },
            { "87",  NULL },
            { "88",  NULL },
            { "89",  NULL },
            { "90",  NULL },
            { "91",  NULL },
            { "92",  NULL },
            { "93",  NULL },
            { "94",  NULL },
            { "95",  NULL },
            { "96",  NULL },
            { "97",  NULL },
            { "98",  NULL },
            { "99",  NULL },
            { "100", NULL },
            { NULL, NULL },
        },
        "12"
    },
#endif
    {
        "desmume_advanced_timing",
        "Enable Advanced Bus-Level Timing",
        NULL,
        "Enables the use of Advanced Bus-Level Timing.",
        NULL,
        "system",
        {
            { "enabled",   NULL },
            { "disabled",  NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "desmume_frameskip",
        "Frameskip",
        NULL,
        "Sets the amount of frames to skip.",
        NULL,
        "video",
        {
            { "0",  NULL },
            { "1",  NULL },
            { "2",  NULL },
            { "3",  NULL },
            { "4",  NULL },
            { "5",  NULL },
            { "6",  NULL },
            { "7",  NULL },
            { "8",  NULL },
            { "9",  NULL },
            { NULL, NULL },
        },
        "0"
    },
    {
        "desmume_internal_resolution",
        "Internal Resolution",
        NULL,
        "Sets the internal resolution. Higher resolutions can provide better clarity for 3D scenes & objects, but will increase the performance requirements",
        NULL,
        "video",
        {
            { "256x192",    "1x (256x192)" },
            { "512x384",    "2x (512x384)" },
            { "768x576",    "3x (768x576)" },
            { "1024x768",   "4x (1024x768)" },
            { "1280x960",   "5x (1280x960)" },
            { "1536x1152",  "6x (1536x1152)" },
            { "1792x1344",  "7x (1792x1344)" },
            { "2048x1536",  "8x (2048x1536)" },
            { "2304x1728",  "9x (2304x1728)" },
            { "2560x1920",  "10x (2560x1920)" },
            { NULL, NULL },
        },
        "256x192"
    },
#ifdef HAVE_OPENGL
    {
        "desmume_opengl_mode",
        "OpenGL Rasterizer (restart)",
        NULL,
        "Enable the OpenGL Rasterizer for 3D.",
        NULL,
        "video",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "desmume_color_depth",
        "OpenGL: Color Depth (restart)",
        NULL,
        "Sets the Color Depth to either 16-bit or 32-bit colors. Requires the OpenGL Rasterizer.",
        NULL,
        "video",
        {
            { "16-bit",  NULL },
            { "32-bit",   NULL },
            { NULL, NULL },
        },
        "16-bit"
    },
    {
        "desmume_gfx_multisampling",
        "OpenGL: Multisampling AA",
        NULL,
        "Set the multiplier for Multisampling Anti Aliasing. Requires the OpenGL Rasterizer.",
        NULL,
        "video",
        {
            { "disabled",  NULL },
            { "2",         NULL },
            { "4",         NULL },
            { "8",         NULL },
            { "16",        NULL },
            { "32",        NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "desmume_gfx_texture_smoothing",
        "OpenGL: Texture Smoothing",
        NULL,
        "Enables Texture Smoothing. Requires the OpenGL Rasterizer.",
        NULL,
        "video",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "desmume_opengl_shadow_polygon",
        "OpenGL: Shadow Polygons",
        NULL,
        "Enables Shadow Polygons. Requires the OpenGL Rasterizer.",
        NULL,
        "video",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "desmume_opengl_special_zero_alpha",
        "OpenGL: Special 0 Alpha",
        NULL,
        "Enables Special 0 Alpha. Requires the OpenGL Rasterizer.",
        NULL,
        "video",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "desmume_opengl_nds_depth_calculation",
        "OpenGL: NDS Depth Calculation",
        NULL,
        "Enables NDS Depth Calculation. Requires the OpenGL Rasterizer.",
        NULL,
        "video",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "desmume_opengl_depth_lequal_polygon_facing",
        "OpenGL: Depth-LEqual Polygon Facing",
        NULL,
        "Enables Depth-LEqual Polygon Facing. Requires the OpenGL Rasterizer.",
        NULL,
        "video",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
#endif
    {
        "desmume_gfx_highres_interpolate_color",
        "Soft3D: High-res Color Interpolation",
        NULL,
        "Enables High-res Color Inperpolation.",
        NULL,
        "video",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "desmume_gfx_linehack",
        "Soft3D: Line Hack",
        NULL,
        "Enables Line Hack.",
        NULL,
        "video",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "desmume_gfx_txthack",
        "Soft3D: Texture Hack",
        NULL,
        "Enables Texture Hack.",
        NULL,
        "video",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "desmume_gfx_edgemark",
        "Edge Marking",
        NULL,
        "Enables Edge Marking.",
        NULL,
        "video",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "desmume_gfx_texture_scaling",
        "Texture Scaling (xBrz)",
        NULL,
        "Sets the multiplier for xBrz texture scaling to use.",
        NULL,
        "video",
        {
            { "1",  NULL },
            { "2",  NULL },
            { "4",  NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "desmume_gfx_texture_deposterize",
        "Texture Deposterization",
        NULL,
        "Enables Texture Deposterization.",
        NULL,
        "video",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "desmume_screens_layout",
        "Screen Layout",
        NULL,
        "Select the screen layout to use.",
        NULL,
        "screen",
        {
            { "top/bottom",    "Top/Bottom" },
            { "bottom/top",    "Bottom/Top" },
            { "left/right",    "Left/Right" },
            { "right/left",    "Right/Left" },
            { "top only",      "Top only" },
            { "bottom only",   "Bottom only" },
            { "hybrid/top",    "Hybrid/Top" },
            { "hybrid/bottom", "Hybrid/Bottom" },
            { NULL, NULL },
        },
        "top/bottom"
    },
    {
        "desmume_screens_gap",
        "Screen Gap",
        NULL,
        "Sets the amout of space between the top and bottom screens.",
        NULL,
        "screen",
        {
            { "0",   NULL },
            { "1",   NULL },
            { "2",   NULL },
            { "3",   NULL },
            { "4",   NULL },
            { "5",   NULL },
            { "6",   NULL },
            { "7",   NULL },
            { "8",   NULL },
            { "9",   NULL },
            { "10",  NULL },
            { "11",  NULL },
            { "12",  NULL },
            { "13",  NULL },
            { "14",  NULL },
            { "15",  NULL },
            { "16",  NULL },
            { "17",  NULL },
            { "18",  NULL },
            { "19",  NULL },
            { "20",  NULL },
            { "21",  NULL },
            { "22",  NULL },
            { "23",  NULL },
            { "24",  NULL },
            { "25",  NULL },
            { "26",  NULL },
            { "27",  NULL },
            { "28",  NULL },
            { "29",  NULL },
            { "30",  NULL },
            { "31",  NULL },
            { "32",  NULL },
            { "33",  NULL },
            { "34",  NULL },
            { "35",  NULL },
            { "36",  NULL },
            { "37",  NULL },
            { "38",  NULL },
            { "39",  NULL },
            { "40",  NULL },
            { "41",  NULL },
            { "42",  NULL },
            { "43",  NULL },
            { "44",  NULL },
            { "45",  NULL },
            { "46",  NULL },
            { "47",  NULL },
            { "48",  NULL },
            { "49",  NULL },
            { "50",  NULL },
            { "51",  NULL },
            { "52",  NULL },
            { "53",  NULL },
            { "54",  NULL },
            { "55",  NULL },
            { "56",  NULL },
            { "57",  NULL },
            { "58",  NULL },
            { "59",  NULL },
            { "60",  NULL },
            { "61",  NULL },
            { "62",  NULL },
            { "63",  NULL },
            { "64",  NULL },
            { "65",  NULL },
            { "66",  NULL },
            { "67",  NULL },
            { "68",  NULL },
            { "69",  NULL },
            { "70",  NULL },
            { "71",  NULL },
            { "72",  NULL },
            { "73",  NULL },
            { "74",  NULL },
            { "75",  NULL },
            { "76",  NULL },
            { "77",  NULL },
            { "78",  NULL },
            { "79",  NULL },
            { "80",  NULL },
            { "81",  NULL },
            { "82",  NULL },
            { "83",  NULL },
            { "84",  NULL },
            { "85",  NULL },
            { "86",  NULL },
            { "87",  NULL },
            { "88",  NULL },
            { "89",  NULL },
            { "90",  NULL },
            { "91",  NULL },
            { "92",  NULL },
            { "93",  NULL },
            { "94",  NULL },
            { "95",  NULL },
            { "96",  NULL },
            { "97",  NULL },
            { "98",  NULL },
            { "99",  NULL },
            { "100", NULL },
            { NULL, NULL },
        },
        "0"
    },
    {
        "desmume_hybrid_layout_ratio",
        "Hybrid Layout: Ratio",
        NULL,
        "Sets the screen ratio for the hybrid screen(s) to use.",
        NULL,
        "screen",
        {
            { "3:1",  NULL },
            { "2:1",   NULL },
            { NULL, NULL },
        },
        "3:1"
    },
    {
        "desmume_hybrid_layout_scale",
        "Hybrid Layout: Scale So Small Screen is 1:1px",
        NULL,
        "Force the small screen to render at 1:1px.",
        NULL,
        "screen",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "desmume_hybrid_showboth_screens",
        "Hybrid Layout: Show Both Screens",
        NULL,
        "Show both top & bottom small screens.",
        NULL,
        "screen",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "desmume_hybrid_cursor_always_smallscreen",
        "Hybrid Layout: Cursor Always on Small Screen",
        NULL,
        "Only display the cursor on the small bottom screen.",
        NULL,
        "screen",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "desmume_pointer_mouse",
        "Mouse/Pointer",
        NULL,
        "Enables the NDS pointer.",
        NULL,
        "input",
        {
            { "disabled",  NULL },
            { "enabled",   NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "desmume_pointer_type",
        "Pointer Type",
        NULL,
        "Select what type of input to use for the pointer.",
        NULL,
        "input",
        {
            { "mouse",  "Mouse" },
            { "touch",  "Touchscreen" },
            { NULL, NULL },
        },
        "mouse"
    },
    {
        "desmume_mouse_speed",
        "Mouse Speed",
        NULL,
        "Sets the speed multiplier to use for the mouse pointer.",
        NULL,
        "input",
        {
            { "1.0",   NULL },
            { "1.5",   NULL },
            { "2.0",   NULL },
            { "0.01",  NULL },
            { "0.02",  NULL },
            { "0.03",  NULL },
            { "0.04",  NULL },
            { "0.05",  NULL },
            { "0.125", NULL },
            { "0.25",  NULL },
            { "0.5",   NULL },
            { NULL, NULL },
        },
        "1.0"
    },
    {
        "desmume_input_rotation",
        "Pointer Rotation",
        NULL,
        "Sets the rotation of the pointer.",
        NULL,
        "input",
        {
            { "0",   NULL },
            { "90",  NULL },
            { "180", NULL },
            { "270", NULL },
            { NULL, NULL },
        },
        "0"
    },
    {
        "desmume_pointer_device_l",
        "Pointer Mode for Left Analog",
        NULL,
        "Sets the pointer mode for the left analog stick.",
        NULL,
        "input",
        {
            { "none",     "None" },
            { "emulated", "Emulated" },
            { "absolute", "Absolute" },
            { "pressed",  "Pressed" },
            { NULL, NULL },
        },
        "none"
    },
    {
        "desmume_pointer_device_r",
        "Pointer Mode for Right Analog",
        NULL,
        "Sets the pointer mode for the right analog stick.",
        NULL,
        "input",
        {
            { "none",     "None" },
            { "emulated", "Emulated" },
            { "absolute", "Absolute" },
            { "pressed",  "Pressed" },
            { NULL, NULL },
        },
        "none"
    },
    {
        "desmume_pointer_device_deadzone",
        "Emulated Pointer Deadzone Percent",
        NULL,
        "Sets the stick deadzone for the emulated pointer.",
        NULL,
        "input",
        {
            { "0",  NULL },
            { "5",  NULL },
            { "10", NULL },
            { "15", NULL },
            { "20", NULL },
            { "25", NULL },
            { "30", NULL },
            { "35", NULL },
            { NULL, NULL },
        },
        "15"
    },
    {
        "desmume_pointer_device_acceleration_mod",
        "Emulated Pointer Acceleration Modifier Percent",
        NULL,
        "Sets the percent for the Pointer Acceleration Modifier.",
        NULL,
        "input",
        {
            { "0",   NULL },
            { "1",   NULL },
            { "2",   NULL },
            { "3",   NULL },
            { "4",   NULL },
            { "5",   NULL },
            { "6",   NULL },
            { "7",   NULL },
            { "8",   NULL },
            { "9",   NULL },
            { "10",  NULL },
            { "11",  NULL },
            { "12",  NULL },
            { "13",  NULL },
            { "14",  NULL },
            { "15",  NULL },
            { "16",  NULL },
            { "17",  NULL },
            { "18",  NULL },
            { "19",  NULL },
            { "20",  NULL },
            { "21",  NULL },
            { "22",  NULL },
            { "23",  NULL },
            { "24",  NULL },
            { "25",  NULL },
            { "26",  NULL },
            { "27",  NULL },
            { "28",  NULL },
            { "29",  NULL },
            { "30",  NULL },
            { "31",  NULL },
            { "32",  NULL },
            { "33",  NULL },
            { "34",  NULL },
            { "35",  NULL },
            { "36",  NULL },
            { "37",  NULL },
            { "38",  NULL },
            { "39",  NULL },
            { "40",  NULL },
            { "41",  NULL },
            { "42",  NULL },
            { "43",  NULL },
            { "44",  NULL },
            { "45",  NULL },
            { "46",  NULL },
            { "47",  NULL },
            { "48",  NULL },
            { "49",  NULL },
            { "50",  NULL },
            { "51",  NULL },
            { "52",  NULL },
            { "53",  NULL },
            { "54",  NULL },
            { "55",  NULL },
            { "56",  NULL },
            { "57",  NULL },
            { "58",  NULL },
            { "59",  NULL },
            { "60",  NULL },
            { "61",  NULL },
            { "62",  NULL },
            { "63",  NULL },
            { "64",  NULL },
            { "65",  NULL },
            { "66",  NULL },
            { "67",  NULL },
            { "68",  NULL },
            { "69",  NULL },
            { "70",  NULL },
            { "71",  NULL },
            { "72",  NULL },
            { "73",  NULL },
            { "74",  NULL },
            { "75",  NULL },
            { "76",  NULL },
            { "77",  NULL },
            { "78",  NULL },
            { "79",  NULL },
            { "80",  NULL },
            { "81",  NULL },
            { "82",  NULL },
            { "83",  NULL },
            { "84",  NULL },
            { "85",  NULL },
            { "86",  NULL },
            { "87",  NULL },
            { "88",  NULL },
            { "89",  NULL },
            { "90",  NULL },
            { "91",  NULL },
            { "92",  NULL },
            { "93",  NULL },
            { "94",  NULL },
            { "95",  NULL },
            { "96",  NULL },
            { "97",  NULL },
            { "98",  NULL },
            { "99",  NULL },
            { "100", NULL },
            { NULL, NULL },
        },
        "0"
    },
    {
        "desmume_pointer_stylus_pressure",
        "Emulated Stylus Pressure Modifier Percent",
        NULL,
        "Sets the percent for the Emulated Stylus Pressure Modifier.",
        NULL,
        "input",
        {
            { "0",   NULL },
            { "1",   NULL },
            { "2",   NULL },
            { "3",   NULL },
            { "4",   NULL },
            { "5",   NULL },
            { "6",   NULL },
            { "7",   NULL },
            { "8",   NULL },
            { "9",   NULL },
            { "10",  NULL },
            { "11",  NULL },
            { "12",  NULL },
            { "13",  NULL },
            { "14",  NULL },
            { "15",  NULL },
            { "16",  NULL },
            { "17",  NULL },
            { "18",  NULL },
            { "19",  NULL },
            { "20",  NULL },
            { "21",  NULL },
            { "22",  NULL },
            { "23",  NULL },
            { "24",  NULL },
            { "25",  NULL },
            { "26",  NULL },
            { "27",  NULL },
            { "28",  NULL },
            { "29",  NULL },
            { "30",  NULL },
            { "31",  NULL },
            { "32",  NULL },
            { "33",  NULL },
            { "34",  NULL },
            { "35",  NULL },
            { "36",  NULL },
            { "37",  NULL },
            { "38",  NULL },
            { "39",  NULL },
            { "40",  NULL },
            { "41",  NULL },
            { "42",  NULL },
            { "43",  NULL },
            { "44",  NULL },
            { "45",  NULL },
            { "46",  NULL },
            { "47",  NULL },
            { "48",  NULL },
            { "49",  NULL },
            { "50",  NULL },
            { "51",  NULL },
            { "52",  NULL },
            { "53",  NULL },
            { "54",  NULL },
            { "55",  NULL },
            { "56",  NULL },
            { "57",  NULL },
            { "58",  NULL },
            { "59",  NULL },
            { "60",  NULL },
            { "61",  NULL },
            { "62",  NULL },
            { "63",  NULL },
            { "64",  NULL },
            { "65",  NULL },
            { "66",  NULL },
            { "67",  NULL },
            { "68",  NULL },
            { "69",  NULL },
            { "70",  NULL },
            { "71",  NULL },
            { "72",  NULL },
            { "73",  NULL },
            { "74",  NULL },
            { "75",  NULL },
            { "76",  NULL },
            { "77",  NULL },
            { "78",  NULL },
            { "79",  NULL },
            { "80",  NULL },
            { "81",  NULL },
            { "82",  NULL },
            { "83",  NULL },
            { "84",  NULL },
            { "85",  NULL },
            { "86",  NULL },
            { "87",  NULL },
            { "88",  NULL },
            { "89",  NULL },
            { "90",  NULL },
            { "91",  NULL },
            { "92",  NULL },
            { "93",  NULL },
            { "94",  NULL },
            { "95",  NULL },
            { "96",  NULL },
            { "97",  NULL },
            { "98",  NULL },
            { "99",  NULL },
            { "100", NULL },
            { NULL, NULL },
        },
        "50"
    },
    {
        "desmume_pointer_colour",
        "Pointer Colour",
        NULL,
        "Sets what color to use for the pointer.",
        NULL,
        "input",
        {
            { "white",  "White" },
            { "black",  "Black" },
            { "red",    "Red" },
            { "blue",   "Blue" },
            { "yellow", "Yellow" },
            { NULL, NULL },
        },
        "white"
    },
    {
        "desmume_mic_mode",
        "Microphone Button Noise Type",
        NULL,
        "Sets what type of noise to use when pressing the microfone button.",
        NULL,
        NULL,
        {
            { "pattern",  "Pattern" },
            { "random",   "Random" },
            { NULL, NULL },
        },
        "pattern"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};

struct retro_core_options_v2 options_us = {
   option_cats_us,
   option_defs_us
};

/*
 ********************************
 * Language Mapping
 ********************************
*/

#ifndef HAVE_NO_LANGEXTRA
struct retro_core_options_v2 *options_intl[RETRO_LANGUAGE_LAST] = {
   &options_us,    /* RETRO_LANGUAGE_ENGLISH */
   NULL,           /* RETRO_LANGUAGE_JAPANESE */
   NULL,           /* RETRO_LANGUAGE_FRENCH */
   NULL,           /* RETRO_LANGUAGE_SPANISH */
   NULL,           /* RETRO_LANGUAGE_GERMAN */
   NULL,           /* RETRO_LANGUAGE_ITALIAN */
   NULL,           /* RETRO_LANGUAGE_DUTCH */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */
   NULL,           /* RETRO_LANGUAGE_RUSSIAN */
   NULL,           /* RETRO_LANGUAGE_KOREAN */
   NULL,           /* RETRO_LANGUAGE_CHINESE_TRADITIONAL */
   NULL,           /* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */
   NULL,           /* RETRO_LANGUAGE_ESPERANTO */
   NULL,           /* RETRO_LANGUAGE_POLISH */
   NULL,           /* RETRO_LANGUAGE_VIETNAMESE */
   NULL,           /* RETRO_LANGUAGE_ARABIC */
   NULL,           /* RETRO_LANGUAGE_GREEK */
   NULL,           /* RETRO_LANGUAGE_TURKISH */
};
#endif

/*
 ********************************
 * Functions
 ********************************
*/

/* Handles configuration/setting of core options.
 * Should be called as early as possible - ideally inside
 * retro_set_environment(), and no later than retro_load_game()
 * > We place the function body in the header to avoid the
 *   necessity of adding more .c files (i.e. want this to
 *   be as painless as possible for core devs)
 */

static INLINE void libretro_set_core_options(retro_environment_t environ_cb,
      bool *categories_supported)
{
   unsigned version  = 0;
#ifndef HAVE_NO_LANGEXTRA
   unsigned language = 0;
#endif

   if (!environ_cb || !categories_supported)
      return;

   *categories_supported = false;

   if (!environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version))
      version = 0;

   if (version >= 2)
   {
#ifndef HAVE_NO_LANGEXTRA
      struct retro_core_options_v2_intl core_options_intl;

      core_options_intl.us    = &options_us;
      core_options_intl.local = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))
         core_options_intl.local = options_intl[language];

      *categories_supported = environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL,
            &core_options_intl);
#else
      *categories_supported = environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2,
            &options_us);
#endif
   }
   else
   {
      size_t i, j;
      size_t option_index              = 0;
      size_t num_options               = 0;
      struct retro_core_option_definition
            *option_v1_defs_us         = NULL;
#ifndef HAVE_NO_LANGEXTRA
      size_t num_options_intl          = 0;
      struct retro_core_option_v2_definition
            *option_defs_intl          = NULL;
      struct retro_core_option_definition
            *option_v1_defs_intl       = NULL;
      struct retro_core_options_intl
            core_options_v1_intl;
#endif
      struct retro_variable *variables = NULL;
      char **values_buf                = NULL;

      /* Determine total number of options */
      while (true)
      {
         if (option_defs_us[num_options].key)
            num_options++;
         else
            break;
      }

      if (version >= 1)
      {
         /* Allocate US array */
         option_v1_defs_us = (struct retro_core_option_definition *)
               calloc(num_options + 1, sizeof(struct retro_core_option_definition));

         /* Copy parameters from option_defs_us array */
         for (i = 0; i < num_options; i++)
         {
            struct retro_core_option_v2_definition *option_def_us = &option_defs_us[i];
            struct retro_core_option_value *option_values         = option_def_us->values;
            struct retro_core_option_definition *option_v1_def_us = &option_v1_defs_us[i];
            struct retro_core_option_value *option_v1_values      = option_v1_def_us->values;

            option_v1_def_us->key           = option_def_us->key;
            option_v1_def_us->desc          = option_def_us->desc;
            option_v1_def_us->info          = option_def_us->info;
            option_v1_def_us->default_value = option_def_us->default_value;

            /* Values must be copied individually... */
            while (option_values->value)
            {
               option_v1_values->value = option_values->value;
               option_v1_values->label = option_values->label;

               option_values++;
               option_v1_values++;
            }
         }

#ifndef HAVE_NO_LANGEXTRA
         if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
             (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH) &&
             options_intl[language])
            option_defs_intl = options_intl[language]->definitions;

         if (option_defs_intl)
         {
            /* Determine number of intl options */
            while (true)
            {
               if (option_defs_intl[num_options_intl].key)
                  num_options_intl++;
               else
                  break;
            }

            /* Allocate intl array */
            option_v1_defs_intl = (struct retro_core_option_definition *)
                  calloc(num_options_intl + 1, sizeof(struct retro_core_option_definition));

            /* Copy parameters from option_defs_intl array */
            for (i = 0; i < num_options_intl; i++)
            {
               struct retro_core_option_v2_definition *option_def_intl = &option_defs_intl[i];
               struct retro_core_option_value *option_values           = option_def_intl->values;
               struct retro_core_option_definition *option_v1_def_intl = &option_v1_defs_intl[i];
               struct retro_core_option_value *option_v1_values        = option_v1_def_intl->values;

               option_v1_def_intl->key           = option_def_intl->key;
               option_v1_def_intl->desc          = option_def_intl->desc;
               option_v1_def_intl->info          = option_def_intl->info;
               option_v1_def_intl->default_value = option_def_intl->default_value;

               /* Values must be copied individually... */
               while (option_values->value)
               {
                  option_v1_values->value = option_values->value;
                  option_v1_values->label = option_values->label;

                  option_values++;
                  option_v1_values++;
               }
            }
         }

         core_options_v1_intl.us    = option_v1_defs_us;
         core_options_v1_intl.local = option_v1_defs_intl;

         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_v1_intl);
#else
         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, option_v1_defs_us);
#endif
      }
      else
      {
         /* Allocate arrays */
         variables  = (struct retro_variable *)calloc(num_options + 1,
               sizeof(struct retro_variable));
         values_buf = (char **)calloc(num_options, sizeof(char *));

         if (!variables || !values_buf)
            goto error;

         /* Copy parameters from option_defs_us array */
         for (i = 0; i < num_options; i++)
         {
            const char *key                        = option_defs_us[i].key;
            const char *desc                       = option_defs_us[i].desc;
            const char *default_value              = option_defs_us[i].default_value;
            struct retro_core_option_value *values = option_defs_us[i].values;
            size_t buf_len                         = 3;
            size_t default_index                   = 0;

            values_buf[i] = NULL;

            if (desc)
            {
               size_t num_values = 0;

               /* Determine number of values */
               while (true)
               {
                  if (values[num_values].value)
                  {
                     /* Check if this is the default value */
                     if (default_value)
                        if (strcmp(values[num_values].value, default_value) == 0)
                           default_index = num_values;

                     buf_len += strlen(values[num_values].value);
                     num_values++;
                  }
                  else
                     break;
               }

               /* Build values string */
               if (num_values > 0)
               {
                  buf_len += num_values - 1;
                  buf_len += strlen(desc);

                  values_buf[i] = (char *)calloc(buf_len, sizeof(char));
                  if (!values_buf[i])
                     goto error;

                  strcpy(values_buf[i], desc);
                  strcat(values_buf[i], "; ");

                  /* Default value goes first */
                  strcat(values_buf[i], values[default_index].value);

                  /* Add remaining values */
                  for (j = 0; j < num_values; j++)
                  {
                     if (j != default_index)
                     {
                        strcat(values_buf[i], "|");
                        strcat(values_buf[i], values[j].value);
                     }
                  }
               }
            }

            variables[option_index].key   = key;
            variables[option_index].value = values_buf[i];
            option_index++;
         }

         /* Set variables */
         environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
      }

error:
      /* Clean up */

      if (option_v1_defs_us)
      {
         free(option_v1_defs_us);
         option_v1_defs_us = NULL;
      }

#ifndef HAVE_NO_LANGEXTRA
      if (option_v1_defs_intl)
      {
         free(option_v1_defs_intl);
         option_v1_defs_intl = NULL;
      }
#endif

      if (values_buf)
      {
         for (i = 0; i < num_options; i++)
         {
            if (values_buf[i])
            {
               free(values_buf[i]);
               values_buf[i] = NULL;
            }
         }

         free(values_buf);
         values_buf = NULL;
      }

      if (variables)
      {
         free(variables);
         variables = NULL;
      }
   }
}

#ifdef __cplusplus
}
#endif

#endif
