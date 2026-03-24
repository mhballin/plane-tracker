/**
 * @file lv_conf.h
 * LVGL Configuration for ESP32-S3 with Elecrow 5" Display (800x480)
 * Optimized for PSRAM and RGB parallel display
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/* Color depth: 16 (RGB565), 24 (RGB888), or 32 (XRGB8888) */
#define LV_COLOR_DEPTH 16

/* Swap the 2 bytes of RGB565 color. Useful if the display has an 8-bit interface (e.g. SPI) */
#define LV_COLOR_16_SWAP 0

/*=========================
   MEMORY SETTINGS
 *=========================*/

/* Size of the memory available for `lv_malloc()` in bytes (>= 2kB) */
#define LV_MEM_CUSTOM 0
#if LV_MEM_CUSTOM == 0
    /* Use heap_caps_malloc with SPIRAM for LVGL's internal allocations */
    #define LV_MEM_SIZE (128U * 1024U)  /* 128KB - plenty for UI widgets */
    #define LV_MEM_ADR 0     /* 0: unused */
    #define LV_MEM_POOL_INCLUDE <stdlib.h>
    #define LV_MEM_POOL_ALLOC   malloc
#else
    /* Use PSRAM for LVGL memory */
    #define LV_MEM_CUSTOM_INCLUDE <esp_heap_caps.h>
    #define LV_MEM_CUSTOM_ALLOC(size) heap_caps_malloc((size), MALLOC_CAP_SPIRAM)
    #define LV_MEM_CUSTOM_FREE heap_caps_free
    #define LV_MEM_CUSTOM_REALLOC(ptr, new_size) heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM)
#endif

/* Number of the intermediate memory buffer used during rendering */
#define LV_DRAW_BUF_STRIDE_ALIGN 1
#define LV_DRAW_BUF_ALIGN 4

/*====================
   HAL SETTINGS
 *====================*/

/* Default display refresh period in ms */
#define LV_DEF_REFR_PERIOD 30

/* Input device read period in milliseconds */
#define LV_INDEV_DEF_READ_PERIOD 30

/* Disable Helium assembly (for ARM Cortex-M only, not ESP32-S3) */
#define LV_USE_DRAW_SW_ASM 0

/*=================
   FONT USAGE
 *=================*/

/*Enable the Montserrat font family*/
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_34 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_38 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_MONTSERRAT_42 1
#define LV_FONT_MONTSERRAT_44 1
#define LV_FONT_MONTSERRAT_46 1
#define LV_FONT_MONTSERRAT_48 1

/* Demonstrate special features */
#define LV_FONT_MONTSERRAT_12_SUBPX      0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0  /* bpp = 3 */
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0  /* Hebrew, Arabic, Persian */
#define LV_FONT_SIMSUN_16_CJK            0  /* 1000 most common CJK radicals */

/* Pixel perfect monospace fonts */
#define LV_FONT_UNSCII_8  0
#define LV_FONT_UNSCII_16 0

/* Optionally declare custom fonts here */
#define LV_FONT_CUSTOM_DECLARE

/* Enable it if you have fonts with many characters.
 * The limit depends on the font size, font face and bpp. */
#define LV_FONT_FMT_TXT_LARGE 1

/* Enables/disables support for compressed fonts */
#define LV_USE_FONT_COMPRESSED 1

/* Enable drawing placeholders when glyphs are not found */
#define LV_USE_FONT_PLACEHOLDER 1

/* Set the default font */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*===================
   TEXT SETTINGS
 *===================*/

/* Select a character encoding for strings */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/* Can break (wrap) texts on these chars */
#define LV_TXT_BREAK_CHARS " ,.;:-_)]}"

/* If a word is at least this long, will break wherever "prettiest"
 * To disable, set to a value <= 0 */
#define LV_TXT_LINE_BREAK_LONG_LEN 0

/* Minimum number of characters in a long word to put on a line before a break.
 * Depends on LV_TXT_LINE_BREAK_LONG_LEN. */
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3

/* Minimum number of characters in a long word to put on a line after a break.
 * Depends on LV_TXT_LINE_BREAK_LONG_LEN. */
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3

/* Support bidirectional texts. Allows mixing Left-to-Right and Right-to-Left texts.
 * The direction will be processed according to the Unicode Bidirectional Algorithm:
 * https://www.w3.org/International/articles/inline-bidi-markup/uba-basics*/
#define LV_USE_BIDI 0
#if LV_USE_BIDI
    /* Set the default direction. Supported values:
     * `LV_BASE_DIR_LTR` Left-to-Right
     * `LV_BASE_DIR_RTL` Right-to-Left
     * `LV_BASE_DIR_AUTO` detect texts base direction */
    #define LV_BIDI_BASE_DIR_DEF LV_BASE_DIR_LTR
#endif

/* Enable Arabic/Persian processing
 * In these languages characters should be replaced with an other form based on their position in the text */
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*====================
   WIDGETS
 *====================*/

/* Documentation of the widgets: https://docs.lvgl.io/latest/en/html/widgets/index.html */

#define LV_USE_ANIMIMG        1
#define LV_USE_ARC            1
#define LV_USE_BAR            1
#define LV_USE_BUTTON         1
#define LV_USE_BUTTONMATRIX   1
#define LV_USE_CALENDAR       1
#define LV_USE_CANVAS         1
#define LV_USE_CHART          1
#define LV_USE_CHECKBOX       1
#define LV_USE_DROPDOWN       1
#define LV_USE_IMAGE          1
#define LV_USE_IMAGEBUTTON    1
#define LV_USE_KEYBOARD       1
#define LV_USE_LABEL          1
#define LV_USE_LED            1
#define LV_USE_LINE           1
#define LV_USE_LIST           1
#define LV_USE_MENU           1
#define LV_USE_METER          1
#define LV_USE_MSGBOX         1
#define LV_USE_ROLLER         1
#define LV_USE_SCALE          1
#define LV_USE_SLIDER         1
#define LV_USE_SPAN           1
#define LV_USE_SPINBOX        1
#define LV_USE_SPINNER        1
#define LV_USE_SWITCH         1
#define LV_USE_TEXTAREA       1
#define LV_USE_TABLE          1
#define LV_USE_TABVIEW        1
#define LV_USE_TILEVIEW       1
#define LV_USE_WIN            1

/*==================
 * THEMES
 *==================*/

/* A simple, impressive and very complete theme */
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
    /* 0: Light mode; 1: Dark mode */
    #define LV_THEME_DEFAULT_DARK 1

    /* 1: Enable grow on press */
    #define LV_THEME_DEFAULT_GROW 1

    /* Default transition time in [ms] */
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif

/* A very simple theme that is a good starting point for a custom theme */
#define LV_USE_THEME_SIMPLE 1

/* A theme designed for monochrome displays */
#define LV_USE_THEME_MONO 1

/*==================
 * LAYOUTS
 *==================*/

/* A layout similar to Flexbox in CSS */
#define LV_USE_FLEX 1

/* A layout similar to Grid in CSS */
#define LV_USE_GRID 1

/*=====================
 * COMPILER SETTINGS
 *====================*/

/* For big endian systems set to 1 */
#define LV_BIG_ENDIAN_SYSTEM 0

/* Define a custom attribute to `lv_tick_inc` function */
#define LV_ATTRIBUTE_TICK_INC

/* Define a custom attribute to `lv_timer_handler` function */
#define LV_ATTRIBUTE_TIMER_HANDLER

/* Define a custom attribute to `lv_disp_flush_ready` function */
#define LV_ATTRIBUTE_FLUSH_READY

/* Required alignment size for buffers */
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE 1

/* With size optimization (-Os) the compiler might not align data to
 * 4 or 8 byte boundary. Some HW may need even 32 or 64 bytes.
 * This alignment will be explicitly applied where needed.
 * LV_ATTRIBUTE_MEM_ALIGN_SIZE should be used to specify required align size.
 * E.g. __attribute__((aligned(LV_ATTRIBUTE_MEM_ALIGN_SIZE))) */
#define LV_ATTRIBUTE_MEM_ALIGN

/* Attribute to mark large constant arrays for example font's bitmaps */
#define LV_ATTRIBUTE_LARGE_CONST

/* Compiler prefix for a big array declaration in RAM */
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY

/* Place performance critical functions into a faster memory (e.g RAM) */
#define LV_ATTRIBUTE_FAST_MEM

/* Export integer constant to binding. This macro is used with constants in the form of LV_<CONST> that
 * should also appear on LVGL binding API such as Micropython. */
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning

/* Prefix all global extern data with this */
#define LV_ATTRIBUTE_EXTERN_DATA

/* Use `float` as `lv_value_precise_t` */
#define LV_USE_FLOAT 1

/*==================
 *   EXAMPLES
 *==================*/

/* Enable the examples to be built with the library */
#define LV_BUILD_EXAMPLES 0

/*===================
 *  DEMO USAGE
 *==================*/

/* Show some widget. It might be required to increase `LV_MEM_SIZE` */
#define LV_USE_DEMO_WIDGETS 0

/* Demonstrate the usage of encoder and keyboard */
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0

/* Benchmark your system */
#define LV_USE_DEMO_BENCHMARK 0

/* Stress test for LVGL */
#define LV_USE_DEMO_STRESS 0

/* Music player demo */
#define LV_USE_DEMO_MUSIC 0

/* Flex layout demo */
#define LV_USE_DEMO_FLEX_LAYOUT 0

/* Smart thermostat demo */
#define LV_USE_DEMO_MULTILANG 0

/* Widget transformation demo */
#define LV_USE_DEMO_TRANSFORM 0

/* Scroll animation demo */
#define LV_USE_DEMO_SCROLL 0

/* Rendering benchmark */
#define LV_USE_DEMO_RENDER 0

/*--END OF LV_CONF_H--*/

#endif /*LV_CONF_H*/
