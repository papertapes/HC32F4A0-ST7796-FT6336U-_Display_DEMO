/**
 *******************************************************************************
 * @file  lcd_config.h
 * @brief Single place for all hardware-dependent settings of the 4.0" 320x480 LCD project.
 * @note  Every pin / clock / timing / IC choice is here. Do not scatter magic numbers in C files.
 *        EXMC pin assignment is identical to the official EV_HC32F4A0_LQFP176 LCD example,
 *        see RM_HC32F4A0 Rev1.50 chapter 40 (EXMC), tables 40-3 / 40-6.
 *******************************************************************************
 * Copyright (C) 2022-2025, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 * This software component is licensed by XHSC under BSD 3-Clause license.
 *******************************************************************************
 */
#ifndef __LCD_CONFIG_H__
#define __LCD_CONFIG_H__

#include "hc32_ll.h"

/* =============================================================================================
 * 0. WIRING QUICK REFERENCE - 34PIN adapter board (TFT_LCD2), capacitive panel
 *    Touch IC: FocalTech FT6336U @ I2C 0x38. This is the FINAL hardware option.
 *    The adapter only re-numbers signals; every MCU-side GPIO below is unchanged,
 *    so NO firmware change is needed. See the wiring quick table below.
 *
 *    Adapter 34PIN -> MCU / net:
 *      1 CS  -> PG12 (CE3)      2 RS  -> PG2  (A12)     3 WR  -> PC0  (WE)
 *      4 RD  -> PF11 (OE)       5 RST -> PI1             6..21 DB0..15 -> EXMC DATA0..15
 *     22 GND -> GND            23 BL  -> PI0 (active HIGH, on-board driver)
 *     24,25 VDD3.3 -> +3.3V    26,27 GND -> GND
 *     28 VCC5/BL_VDD -> +5V  *** separate 5V supply, do NOT connect to any MCU IO ***
 *     29 NC (capacitive)       30 SDA -> PF10 (I2C1_SDA, func48)
 *     31 INT -> PI4 (optional) 32 NC (capacitive)       33 RST -> PI2 (touch reset)
 *     34 SCL -> PD3  (I2C1_SCL, func49)
 *    NOTE: pins 29~34 are labelled as resistive SPI on the adapter symbol; for the
 *          capacitive panel they mean NC/SDA/INT/NC/RST/SCL per the CT definition table.
 * ========================================================================================== */

/* =============================================================================================
 * 1. Panel selection
 *    The panel fitted is a ST7796 (320x480), so the project is fixed to it: no ID probing
 *    at power-on (faster, deterministic, immune to /RD readback / timing issues).
 *
 *    LCD_PANEL_AUTO = 0  +  LCD_PANEL_FORCE_ST7796  = 1   -> fixed ST7796   (CURRENT)
 *    LCD_PANEL_AUTO = 1  +  both FORCE macros = 0           -> auto-detect ST7796 / ILI9488
 *    LCD_PANEL_AUTO = 0  +  LCD_PANEL_FORCE_ILI9488 = 1     -> fixed ILI9488
 *    Only ONE of the two FORCE macros may be 1.
 *    The ILI9488 init table and auto-detect code remain in lcd_panel.c for future modules.
 * ========================================================================================== */
#define LCD_PANEL_AUTO                              (0U)
#define LCD_PANEL_FORCE_ST7796                      (1U)
#define LCD_PANEL_FORCE_ILI9488                     (0U)

/* =============================================================================================
 * 2. i8080 bus width
 *    1 = 16-bit (DB0~DB15), matches the official example and RM 40.3.1 (SMC 16/32-bit only).
 *    0 = 8-bit  (DB0~DB7 only); SMC stays 16-bit, a RGB565 pixel is split into 2 writes.
 * ========================================================================================== */
#define LCD_BUS_16BIT                               (1U)

/* Native (portrait) resolution of the module, from the module datasheet */
#define LCD_NATIVE_WIDTH                            (320U)
#define LCD_NATIVE_HEIGHT                           (480U)
#define LCD_DEFAULT_ORIENTATION                     (0U)   /* 0 = vertical, 1 = horizontal */

/* Color inversion is handled by the panel vendor's init table: it sends
 * INVON (0x21) before DISPON, so standard RGB565 values display correctly.
 * ST7796 powers up in inversion-off state; this BOE IPS module needs inversion
 * ON. (The earlier software workaround is kept only as a fallback for other
 * modules: set 1 to bitwise-invert pixels if a different panel shows inverted
 * colors.  Only BSP_LCD_WriteColor() is affected; commands are untouched.)
 *   0 = pass RGB565 through unchanged (THIS module, vendor INVON table) - default
 *   1 = software bitwise inversion fallback (other inverted modules) */
#define LCD_COLOR_INVERT                            (0U)

/* =============================================================================================
 * 3. EXMC SMC configuration (identical to ev_hc32f4a0_lqfp176_nt35510.c)
 *    EXCLK = HCLK/8 = 30 MHz @ PLLH 240 MHz.
 *    Timing values are the official ones for 30 MHz EXCLK; do not speed up the READ
 *    timing (RC/CEOE) or the ID read (0x04 / 0xD3) returns 0x0000.
 * ========================================================================================== */
#define LCD_SMC_CHIP                                (EXMC_SMC_CHIP3)
#define LCD_SMC_MATCH_ADDR                          (0x60UL)
#define LCD_SMC_ADDR_MASK                           (EXMC_SMC_ADDR_MASK_16MB)
#define LCD_SMC_MEMORY_WIDTH                        (EXMC_SMC_MEMORY_WIDTH_16BIT)
#define LCD_SMC_READ_BURST                          (EXMC_SMC_READ_BURST_4BEAT)
#define LCD_SMC_WRITE_BURST                         (EXMC_SMC_WRITE_BURST_4BEAT)
#define LCD_SMC_READ_MODE                           (EXMC_SMC_READ_SYNC)
#define LCD_SMC_WRITE_MODE                          (EXMC_SMC_WRITE_SYNC)
#define LCD_SMC_BAA                                 (EXMC_SMC_BAA_PORT_DISABLE)
#define LCD_SMC_ADV                                 (EXMC_SMC_ADV_PORT_DISABLE)
#define LCD_SMC_BLS                                 (EXMC_SMC_BLS_SYNC_CS)

#define LCD_SMC_TIMING_RC                           (12U)  /* tRCFM >= 400 ns */
#define LCD_SMC_TIMING_WC                           (2U)   /* tWC   >= 33 ns  */
#define LCD_SMC_TIMING_CEOE                         (7U)
#define LCD_SMC_TIMING_WP                           (1U)   /* tWRL  >= 15 ns  */
#define LCD_SMC_TIMING_PC                           (4U)
#define LCD_SMC_TIMING_TR                           (1U)
#define LCD_SMC_INIT_TIMEOUT                        (0x100000UL)
#define LCD_EXCLK_DIV                               (CLK_EXCLK_DIV8)

/* A12 is the RS (D/CX) signal.
 * With a 16-bit memory width CPU A[1] maps to EXMC_ADD[0], so EXMC_ADD[12]
 * is toggled by CPU A[13] = 0x2000. REG is at 0x...1FFE (A13=0) and RAM at
 * 0x...2000 (A13=1); the 2-byte struct offset flips RS. */
#define LCD_RS_ADDR_LINE                            (12U)
#define LCD_SMC_BASE                                (0x60000000UL)
#define LCD_SMC_REG_ADDR                            (0x60001FFEUL)
#define LCD_SMC_RAM_ADDR                            (0x60002000UL)
#define LCD_SMC_GPIO_FUNC                           (GPIO_FUNC_12)

/* =============================================================================================
 * 4. EXMC pin assignment (unchanged from the official EV_HC32F4A0_LQFP176 example)
 * ========================================================================================== */
#define LCD_CS_PORT                                 (GPIO_PORT_G)   /* PG12 - EXMC_CE3 (DS Rev1.60 pin table) */
#define LCD_CS_PIN                                  (GPIO_PIN_12)
#define LCD_WE_PORT                                 (GPIO_PORT_C)   /* PC00 - EXMC_WE  */
#define LCD_WE_PIN                                  (GPIO_PIN_00)
#define LCD_OE_PORT                                 (GPIO_PORT_F)   /* PF11 - EXMC_OE  */
#define LCD_OE_PIN                                  (GPIO_PIN_11)
#define LCD_RS_PORT                                 (GPIO_PORT_G)   /* PG02 - EXMC_ADD12 */
#define LCD_RS_PIN                                  (GPIO_PIN_02)

#define LCD_D0_PORT                                 (GPIO_PORT_D)
#define LCD_D0_PIN                                  (GPIO_PIN_14)
#define LCD_D1_PORT                                 (GPIO_PORT_D)
#define LCD_D1_PIN                                  (GPIO_PIN_15)
#define LCD_D2_PORT                                 (GPIO_PORT_D)
#define LCD_D2_PIN                                  (GPIO_PIN_00)
#define LCD_D3_PORT                                 (GPIO_PORT_D)
#define LCD_D3_PIN                                  (GPIO_PIN_01)
#define LCD_D4_PORT                                 (GPIO_PORT_E)
#define LCD_D4_PIN                                  (GPIO_PIN_07)
#define LCD_D5_PORT                                 (GPIO_PORT_E)
#define LCD_D5_PIN                                  (GPIO_PIN_08)
#define LCD_D6_PORT                                 (GPIO_PORT_E)
#define LCD_D6_PIN                                  (GPIO_PIN_09)
#define LCD_D7_PORT                                 (GPIO_PORT_E)
#define LCD_D7_PIN                                  (GPIO_PIN_10)
#define LCD_D8_PORT                                 (GPIO_PORT_E)
#define LCD_D8_PIN                                  (GPIO_PIN_11)
#define LCD_D9_PORT                                 (GPIO_PORT_E)
#define LCD_D9_PIN                                  (GPIO_PIN_12)
#define LCD_D10_PORT                                (GPIO_PORT_E)
#define LCD_D10_PIN                                 (GPIO_PIN_13)
#define LCD_D11_PORT                                (GPIO_PORT_E)
#define LCD_D11_PIN                                 (GPIO_PIN_14)
#define LCD_D12_PORT                                (GPIO_PORT_E)
#define LCD_D12_PIN                                 (GPIO_PIN_15)
#define LCD_D13_PORT                                (GPIO_PORT_D)
#define LCD_D13_PIN                                 (GPIO_PIN_08)
#define LCD_D14_PORT                                (GPIO_PORT_D)
#define LCD_D14_PIN                                 (GPIO_PIN_09)
#define LCD_D15_PORT                                (GPIO_PORT_D)
#define LCD_D15_PIN                                 (GPIO_PIN_10)

/* =============================================================================================
 * 5. Panel control
 *    /RESET is active low, BL is active high (drives an external transistor, NOT the LED directly).
 *
 *    On the official EV_HC32F4A0_LQFP176 board, J30 pin 5 (net LCD_RST) is driven by
 *    TCA9539 IO7, NOT by PI1. Set LCD_RST_VIA_TCA9539 = 1 for that board (default).
 *    For a self-drawn board with the panel RST wired directly to PI1, set it to 0.
 * ========================================================================================== */
#define LCD_RST_PORT                                (GPIO_PORT_I)
#define LCD_RST_PIN                                 (GPIO_PIN_01)
#define LCD_BKL_PORT                                (GPIO_PORT_I)   /* PI0, same as EV board */
#define LCD_BKL_PIN                                 (GPIO_PIN_00)

/* Minimal TCA9539 backend (EV board IO expander, shares I2C1 with the FT6336U).
 * TCA9539 7-bit addr = 0x74; LCD /RST = PORT0 IO7; reset input = PC13. */
#define LCD_TCA9539_I2C_UNIT                        (CM_I2C1)
#define LCD_TCA9539_I2C_FCG                         (FCG1_PERIPH_I2C1)
#define LCD_TCA9539_DEV_ADDR                        (0x74U)
#define LCD_TCA9539_REG_OUTPUT0                     (0x02U)
#define LCD_TCA9539_REG_CONFIG0                     (0x06U)
#define LCD_TCA9539_LCD_RST_PIN                     (0x80U)   /* IO7 */
#define LCD_TCA9539_RST_PORT                        (GPIO_PORT_C)
#define LCD_TCA9539_RST_PIN                         (GPIO_PIN_13)

/* =============================================================================================
 * 6. FT6336U capacitive touch (6-pin FPC)
 * ========================================================================================== */
#define TOUCH_I2C_UNIT                              (CM_I2C1)
#define TOUCH_I2C_FCG                               (FCG1_PERIPH_I2C1)
#define TOUCH_I2C_SCL_PORT                          (GPIO_PORT_D)
#define TOUCH_I2C_SCL_PIN                           (GPIO_PIN_03)
#define TOUCH_I2C_SCL_FUNC                          (GPIO_FUNC_49)
#define TOUCH_I2C_SDA_PORT                          (GPIO_PORT_F)
#define TOUCH_I2C_SDA_PIN                           (GPIO_PIN_10)
#define TOUCH_I2C_SDA_FUNC                          (GPIO_FUNC_48)
#define TOUCH_I2C_ADDR                              (0x38U)     /* 7-bit FT6336U address */
#define TOUCH_I2C_BAUDRATE                          (100000UL)
#define TOUCH_I2C_TIMEOUT                           (0x40000UL)

#define TOUCH_RST_PORT                              (GPIO_PORT_I)
#define TOUCH_RST_PIN                               (GPIO_PIN_02)
#define TOUCH_INT_PORT                              (GPIO_PORT_I)
#define TOUCH_INT_PIN                               (GPIO_PIN_04)
#define TOUCH_USE_INT                               (0U)        /* 0 = polling (default) */

/* Raw coordinate range reported by the touch IC; map linearly to the LCD pixels.
 * Adjust these after a real calibration if the touch and LCD axes differ. */
#define TOUCH_RAW_X_MAX                             (320U)
#define TOUCH_RAW_Y_MAX                             (480U)
#define TOUCH_SWAP_XY                               (0U)
#define TOUCH_INVERT_X                              (0U)
#define TOUCH_INVERT_Y                              (0U)

/* Palm / multi-touch rejection.
 * 0 = use P1 whenever TD_STATUS reports at least one point (default).
 * 1 = require EXACTLY one touch point; any multi-point frame is treated as
 *     "no touch". Enable only if a resting palm still produces stray dots. */
#define TOUCH_REJECT_MULTI                          (0U)

/* Out-of-range raw coordinate rejection.
 * An idle / invalid FT6336U frame can report a full-scale raw value (e.g.
 * 0x0FFF), which the linear mapping clamps to the panel corner - a corner that
 * sits inside the button bar. Rejecting raw values outside the configured range
 * removes that whole class of phantom points.
 * Set to 0 only if your panel's real raw range is LARGER than TOUCH_RAW_*_MAX. */
#define TOUCH_REJECT_OUT_OF_RANGE                   (1U)

/* FT6336U register map (public FT6x36 register map) */
#define FT6336U_REG_DEVICE_MODE                     (0x00U)
#define FT6336U_REG_GESTURE_ID                      (0x01U)
#define FT6336U_REG_TD_STATUS                       (0x02U)
#define FT6336U_REG_P1_XH                           (0x03U)
#define FT6336U_REG_P1_XL                           (0x04U)
#define FT6336U_REG_P1_YH                           (0x05U)
#define FT6336U_REG_P1_YL                           (0x06U)
#define FT6336U_REG_ID_G_MODE                       (0xA4U)
#define FT6336U_REG_THRESHOLD                       (0x80U)
#define FT6336U_TD_STATUS_MASK                      (0x0FU)
#define FT6336U_P1_XH_MASK                          (0x0FU)

/* =============================================================================================
 * 7. Bring-up diagnostics (no serial cable needed - the backlight is the indicator)
 *    0 = normal demo: touch + 3 green boxes + red circle + RGB cycle.
 *    1 = BACKLIGHT SANITY: right after clock init, force PI0 (BL) high, then loop forever.
 *        Nothing else runs (no SMC / no LCD init / no touch / no I2C).
 *        -> If the backlight stays dark in this mode, the fault is purely HARDWARE:
 *           adapter pin 28 VCC5 must have +5V, pin 23 BL must reach PI0, and 3V3 must be present.
 *    2 = LCD-ONLY RGB CYCLE: skip the FT6336U I2C probe, run SMC + ST7796 init and cycle
 *        full-screen red/green/blue.
 *        -> If colors appear here but not in mode 0, the I2C touch probe was blocking startup.
 *
 *    Change only this macro, rebuild and download. Remember to set it back to 0 afterwards.
 * ========================================================================================== */
#define LCD_DIAG_MODE                               (0U)

/* Print the REAL RDDID/ID4 bytes read back from the panel at every startup.
 * 0x00/0xFF means the panel is not answering (reset held, /RD or data wiring fault);
 * 0x85/0x52/0x77 0x96 means the ST7796 bus is alive. Set 0 to silence the console. */
#define LCD_DEBUG_PRINT_RAW_ID                      (1U)

/* LCD /RESET drive source:
 *    0 = PI1 direct GPIO (self-drawn board with RST wired to PI1).
 *    1 = TCA9539 IO7 (official EV board J30 pin 5, net "LCD_RST"). DEFAULT for EV board.
 *  The TCA9539 backend is now implemented in bsp_lcd.c (lightweight, shares I2C1 with FT6336U).
 *  Change to 0 only if your board really wires the panel /RST to PI1. */
#define LCD_RST_VIA_TCA9539                         (1U)

/* =============================================================================================
 * 8. Touch drawing board UI (应用层布局，不影响任何硬件配置)
 *    竖屏 320x480: 上方画布 y=[0, DRAW_CANVAS_BOTTOM], 下方 5 色按钮栏 y=[DRAW_CANVAS_BOTTOM+1, 479]
 *    5 个按钮等宽 320/5 = 64 px: 白 / 红 / 绿 / 橙 / 蓝, 画笔固定黑色。
 * ========================================================================================== */
#define DRAW_BAR_HEIGHT                             (70U)
#define DRAW_CANVAS_BOTTOM                          (LCD_NATIVE_HEIGHT - DRAW_BAR_HEIGHT - 1U) /* 409 */
#define DRAW_BUTTON_COUNT                           (5U)
#define DRAW_BUTTON_WIDTH                           (LCD_NATIVE_WIDTH / DRAW_BUTTON_COUNT)     /* 64 */

/* Pen / stroke tuning */
#define DRAW_PEN_SIZE                               (6U)    /* pen tip size in pixels (square)       */
/* One sample is now taken per 10 ms scan frame, so a fast circle can easily
 * travel ~100 px between two samples. Out-of-family samples are DROPPED (never
 * re-anchored), therefore a larger threshold only affects what we ignore. */
#define DRAW_STROKE_MAX_STEP                        (100U)  /* max plausible pixel move per sample  */
/* NOTE: the old "N consecutive jump samples" counter is gone - the poll loop is
 * much faster than the touch scan rate, so it counted the same frame twice.
 * Mid-stroke jumps are now confirmed by time (DRAW_GHOST_HOLD_MS below). */

/* Stroke smoothing
 * FT6336U reports +-2..4 px of coordinate noise while the finger moves fast.
 * The old code connected consecutive samples with straight capsules, so that
 * noise turned straight into a visible wave. The stroke is now rendered as a
 * chain of quadratic Bezier arcs through the MIDPOINTS of the samples, with a
 * 1-2-1 smoothed control point: a symmetric (zero phase lag) filter that fully
 * cancels alternating noise, while keeping the pen glued to the finger.
 *   DRAW_CURVE_ENABLE : 1 = smooth curve, 0 = plain polyline (A/B compare)
 *   DRAW_CURVE_SEG_PX : max chord length when rasterising the curve (px)
 *   DRAW_MIN_STEP_PX  : samples closer than this to the last point are dropped
 *                       (the polling loop reads the same frame many times) */
#define DRAW_CURVE_ENABLE                           (1U)
#define DRAW_CURVE_SEG_PX                           (2U)
#define DRAW_MIN_STEP_PX                            (1U)

/* Touch robustness
 * The main loop polls TOUCH_GetXY() far faster than the FT6336U scans (~100 Hz),
 * so "N consecutive samples" used to mean "the same scan frame read N times".
 * Every confirmation below is therefore TIME based (1 ms SysTick).
 *   DRAW_PRESS_VOTES     : samples needed before a press is accepted
 *   DRAW_PRESS_HOLD_MS   : the touch must be continuously present this long
 *                          (a ghost burst / release transient never lasts this long)
 *   DRAW_PENUP_QUIET_MS  : ignore new presses this long after a pen-up
 *   DRAW_GHOST_HOLD_MS   : a far mid-stroke point must persist this long to be real
 *   DRAW_BUTTON_VOTES    : consecutive samples on one button while sliding the bar
 */
#define DRAW_PRESS_VOTES                            (3U)
#define DRAW_PRESS_HOLD_MS                          (30U)
#define DRAW_PENUP_QUIET_MS                         (80U)
/* DEPRECATED: the old "far point must persist 30 ms,
 * then re-anchor here" branch was the direct cause of BOTH ghost dashes and
 * broken circles. Mid-stroke out-of-family samples are now simply dropped.
 * Kept only so existing config snippets do not break the build. */
#define DRAW_GHOST_HOLD_MS                          (30U)
#define DRAW_BUTTON_VOTES                           (2U)

/* Touch tracking policy.
 * FT6336U reports an event flag per point: 00 put-down / 01 put-up / 10 contact.
 *   1 = during a gesture ignore any "put-down" point (a second finger / ghost)
 *       and follow the CONTACT point nearest to the last accepted position.
 *   0 = position-only tracking (fallback if a panel firmware never reports
 *       "contact" after the first frame). */
#define DRAW_TRACK_EVENT_FILTER                     (1U)

/* Touch sampling period. The FT6336U scans the panel at ~100 Hz (10 ms/frame);
 * reading it faster only re-reads the SAME frame, which makes "3 votes" a
 * meaningless quantity. Polling once per scan frame makes one sample == one
 * new frame, so every vote counted by the press gate is a real frame.
 * Set to 0 to disable the throttle (only for A/B testing). */
#define TOUCH_POLL_INTERVAL_MS                      (10U)

/* Deferred commit ("prove yourself before you may paint").
 * A press is buffered and paints NOTHING until the pen has moved
 * DRAW_PENDING_MIN_MOVE pixels away from the touch-down point. A ghost contact
 * (0..3 px, 1..3 frames) therefore leaves no mark at all, whatever produced it.
 * Trade-off: a deliberate "tap without moving" does not dot the canvas.
 *   DRAW_PENDING_MAX      : samples buffered while unproven (anchor + latest kept)
 *   DRAW_PENDING_MIN_MOVE : displacement from the anchor that proves a real stroke
 */
#define DRAW_PENDING_ENABLE                         (1U)
#define DRAW_PENDING_MAX                            (8U)
#define DRAW_PENDING_MIN_MOVE                       (10U)

/* Console trace of every stroke / dropped ghost (see 09_*). Handy to verify the
 * 1 ms time base against a stopwatch; set 0 to silence it. */
#define DRAW_TOUCH_LOG                              (1U)

#endif /* __LCD_CONFIG_H__ */
