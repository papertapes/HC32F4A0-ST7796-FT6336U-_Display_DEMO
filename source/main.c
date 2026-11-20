/**
 *******************************************************************************
 * @file  main.c
 * @brief Touch drawing board for a 4.0" 320x480 i8080 LCD (ST7796 + FT6336U).
 *
 *        Layout (portrait):
 *          - upper canvas area  : y = 0 .. DRAW_CANVAS_BOTTOM (default white)
 *          - bottom color bar   : 5 equal buttons (WHITE/RED/GREEN/ORANGE/BLUE)
 *        The pen is always BLACK. Touching a color button fills the canvas with
 *        that background color (which also erases the drawing).
 *
 * @note  Hardware bring-up is identical to the validated LCD_4P0_8080_Keil demo.
 *******************************************************************************
 * Copyright (C) 2022-2025, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 * This software component is licensed by XHSC under BSD 3-Clause license.
 *******************************************************************************
 */
#include "main.h"

/**
 * @addtogroup HC32F4A0_LCD_4P0_Examples
 * @{
 */

/*******************************************************************************
 * Local type definitions
 ******************************************************************************/

typedef struct {
    uint16_t u16Color;      /* RGB565 background color */
    const char *pu8Name;    /* console label */
} stc_draw_color_t;

#if (DRAW_PENDING_ENABLE != 0U)
/* One buffered touch sample of an unproven press */
typedef struct {
    uint16_t u16X;
    uint16_t u16Y;
} stc_draw_point_t;
#endif

/*******************************************************************************
 * Local definitions
 ******************************************************************************/

/* The ST7796 ignores the first RAMWR burst issued immediately after DISPON.
 * The old demo hid this because its first full-screen fill was BLACK (the
 * power-up GRAM content is black anyway). Here the first fill is WHITE, so
 * that lost burst leaves the canvas black. A settling delay + one discarded
 * fill before the real UI fixes it deterministically. */
#define DRAW_PANEL_SETTLE_MS                        (250UL)

/* Interaction model.
 *
 * The main loop polls the touch IC much faster than the IC scans (~100 Hz), so
 * "N consecutive samples" really means "the same scan frame read N times".
 * Every delay/confirmation below is therefore measured in REAL TIME (1 ms
 * SysTick), not in poll iterations.
 *
 * A gesture is bound to the area it was STARTED in and can never change:
 *   DRAW_GESTURE_CANVAS - samples in the button bar are dropped, so drawing
 *                         can never switch the background colour.
 *   DRAW_GESTURE_BUTTON - samples on the canvas are dropped, so a tap in the
 *                         bar can never paint the canvas.
 */
#define DRAW_GESTURE_NONE                           (0U)
#define DRAW_GESTURE_CANVAS                         (1U)
#define DRAW_GESTURE_BUTTON                         (2U)

/* Mid-stroke jump filter threshold (a real finger cannot move this far in one
 * scan frame, but a ghost point often sits on the opposite side of the panel). */
#define DRAW_STROKE_MAX_STEP_SQ                     ((uint32_t)DRAW_STROKE_MAX_STEP * \
                                                     (uint32_t)DRAW_STROKE_MAX_STEP)

/*******************************************************************************
 * Local variables
 ******************************************************************************/

/* Bottom bar buttons, left to right. Index matches x / DRAW_BUTTON_WIDTH. */
static const stc_draw_color_t m_astcColors[DRAW_BUTTON_COUNT] = {
    {LCD_COLOR_WHITE,  "WHITE"},
    {LCD_COLOR_RED,    "RED"},
    {LCD_COLOR_GREEN,  "GREEN"},
    {LCD_COLOR_ORANGE, "ORANGE"},
    {LCD_COLOR_BLUE,   "BLUE"},
};

/* Title text shown at the top of the canvas. */
#define DRAW_TITLE_L1   "SuperFly"
#define DRAW_TITLE_L2   "HC32F4A0 +ST7796 +FT6336U _Display_DEMO_V1.5"

/* 5x7 dot-matrix font, column-major: bit[row] = lit pixel. */
typedef struct {
    char    ch;
    uint8_t au8Col[5U];
} stc_font5x7_t;

static const stc_font5x7_t m_astcFont[] = {
    { ' ', { 0x00U, 0x00U, 0x00U, 0x00U, 0x00U } },
    { '+', { 0x08U, 0x08U, 0x3EU, 0x08U, 0x08U } },
    { '.', { 0x00U, 0x00U, 0x60U, 0x60U, 0x00U } },
    { '0', { 0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU } },
    { '1', { 0x00U, 0x42U, 0x7FU, 0x40U, 0x00U } },
    { '2', { 0x42U, 0x61U, 0x51U, 0x49U, 0x46U } },
    { '3', { 0x41U, 0x49U, 0x49U, 0x49U, 0x36U } },
    { '4', { 0x18U, 0x14U, 0x12U, 0x7FU, 0x10U } },
    { '5', { 0x27U, 0x45U, 0x45U, 0x45U, 0x39U } },
    { '6', { 0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U } },
    { '7', { 0x01U, 0x71U, 0x09U, 0x05U, 0x03U } },
    { '9', { 0x06U, 0x49U, 0x49U, 0x49U, 0x3EU } },
    { 'A', { 0x7EU, 0x09U, 0x09U, 0x09U, 0x7EU } },
    { 'C', { 0x3EU, 0x41U, 0x41U, 0x41U, 0x41U } },
    { 'D', { 0x7FU, 0x41U, 0x41U, 0x41U, 0x3EU } },
    { 'E', { 0x7FU, 0x49U, 0x49U, 0x49U, 0x41U } },
    { 'F', { 0x7FU, 0x09U, 0x09U, 0x09U, 0x01U } },
    { 'H', { 0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU } },
    { 'M', { 0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU } },
    { 'O', { 0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU } },
    { 'S', { 0x46U, 0x49U, 0x49U, 0x49U, 0x31U } },
    { 'T', { 0x01U, 0x01U, 0x7FU, 0x01U, 0x01U } },
    { 'U', { 0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU } },
    { 'V', { 0x1FU, 0x20U, 0x40U, 0x20U, 0x1FU } },
    { '_', { 0x40U, 0x40U, 0x40U, 0x40U, 0x40U } },
    { 'a', { 0x20U, 0x54U, 0x54U, 0x54U, 0x78U } },
    { 'e', { 0x38U, 0x54U, 0x54U, 0x54U, 0x18U } },
    { 'i', { 0x00U, 0x44U, 0x7DU, 0x40U, 0x00U } },
    { 'l', { 0x00U, 0x41U, 0x7FU, 0x40U, 0x00U } },
    { 'p', { 0x7CU, 0x24U, 0x24U, 0x24U, 0x18U } },
    { 'r', { 0x7CU, 0x08U, 0x04U, 0x04U, 0x08U } },
    { 's', { 0x48U, 0x54U, 0x54U, 0x54U, 0x24U } },
    { 'u', { 0x3CU, 0x40U, 0x40U, 0x20U, 0x7CU } },
    { 'y', { 0x0EU, 0x50U, 0x50U, 0x50U, 0x3EU } },
};

/* Canvas ink-coverage mask: one byte per canvas pixel (320 x 410 = 131200 B).
 *     0 = bare background, 255 = solid pen.
 * A new segment stores max(old, new) instead of simply overwriting, so an
 * overlapping segment can only DARKEN a pixel, never lighten it. That makes
 * the accumulated mask identical to the distance field of the WHOLE polyline
 * and removes the "string of sausages" artefact: with the old code every
 * segment re-blended its feathered edge with the BACKGROUND colour, which
 * bleached the previous segment at every joint.
 * Fits easily in .bss - RW_IRAM2 is 512 KB and the previous ZI was ~17 KB. */
#define DRAW_CANVAS_PIXELS                          (LCD_NATIVE_WIDTH * (DRAW_CANVAS_BOTTOM + 1U))
static uint8_t  m_au8Ink[DRAW_CANVAS_PIXELS];

static uint8_t  m_u8Gesture      = DRAW_GESTURE_NONE; /* owner of the current touch */
static uint16_t m_u16LastX       = 0U;   /* last accepted pen position (canvas) */
static uint16_t m_u16LastY       = 0U;
static uint8_t  m_u8LastButton   = 0xFFU;/* previous button index, 0xFF = none */

/* Pen-down confirmation. The anchor is the FIRST sample of a continuous touch
 * sequence: it fixes both the gesture owner (canvas / bar) and the point where
 * the stroke starts. m_u16Prev* only serves the continuity test. */
static uint8_t  m_u8HaveAnchor   = 0U;
static uint8_t  m_u8PressVotes   = 0U;   /* samples in the current sequence */
static uint16_t m_u16AnchorX     = 0U;
static uint16_t m_u16AnchorY     = 0U;
static uint16_t m_u16PrevX       = 0U;   /* previous sample, continuity check */
static uint16_t m_u16PrevY       = 0U;
static uint32_t m_u32AnchorTick  = 0UL;  /* sequence start (ms) */
static uint32_t m_u32PenUpTick   = 0UL;  /* last pen-up (ms), for the quiet window */

/* Mid-stroke samples are never re-anchored:
 * out-of-family points are simply DROPPED and counted for diagnostics.
 * m_u16LastStep remembers the previous real step so a fast stroke is allowed
 * to accelerate instead of being rejected forever by a fixed threshold. */
static uint16_t m_u16DropCount   = 0U;
static uint16_t m_u16LastStep    = 0U;

/* Button gesture: sliding along the bar needs its own short vote */
static uint8_t  m_u8BtnCandidate = 0xFFU;
static uint8_t  m_u8BtnVotes     = 0U;

/* Gesture bookkeeping for the console trace:
 * the pen-down tick lets a stroke report its real duration, which is also the
 * cheapest way for a human to verify that the 1 ms time base is really 1 ms. */
static uint32_t m_u32GestureTick = 0UL;
static uint16_t m_u16StrokeFrames = 0U;  /* accepted samples in the current stroke */

/* Result of DRAW_SelectTouch(): the point that belongs to the tracked gesture. */
static uint16_t m_u16SampleX      = 0U;
static uint16_t m_u16SampleY      = 0U;

#if (DRAW_PENDING_ENABLE != 0U)
/* Unproven press buffer: nothing is painted until the pen has moved far enough
 * away from the touch-down point, so a ghost contact can never leave a mark.
 * m_u16PendMove = max displacement from the anchor seen so far. */
static stc_draw_point_t m_astcPend[DRAW_PENDING_MAX];
static uint8_t  m_u8PendCount    = 0U;
static uint16_t m_u16PendMove    = 0U;
#endif

/* 1 = the 1 ms SysTick really advances. If the time base failed we must fail
 * OPEN (behave like the plain N-sample vote) instead of comparing against a
 * frozen counter, which would make the panel completely unresponsive. */
static uint8_t  m_u8TickOk      = 0U;

/* Tick of the last touch read: one read per FT6336U scan frame (10 ms). */
static uint32_t m_u32PollTick   = 0UL;

/* Smooth-stroke geometry state.
 * Q1 = last accepted sample, Q2 = the one before it; the incoming sample is
 * the function argument. u8PtCount is 0 = idle, 1 = one sample, 2 = two or more. */
static uint16_t m_u16Q1X        = 0U;
static uint16_t m_u16Q1Y        = 0U;
static uint16_t m_u16Q2X        = 0U;
static uint16_t m_u16Q2Y        = 0U;
static uint8_t  m_u8PtCount     = 0U;

/*******************************************************************************
 * Local functions
 ******************************************************************************/

/**
 * @brief  Fill a rectangular area with one RGB565 color.
 * @note   Uses one CASET/RASET window + one RAMWR command, then streams pixels;
 *         much faster than thousands of individual BSP_LCD_WritePixel() calls.
 */
static void DRAW_FillRect(uint16_t u16X1, uint16_t u16Y1,
                          uint16_t u16X2, uint16_t u16Y2, uint16_t u16Color)
{
    uint32_t i;
    uint32_t u32Total;

    if ((u16X2 < u16X1) || (u16Y2 < u16Y1)) {
        return;
    }

    BSP_LCD_SetWindow(u16X1, u16Y1, u16X2, u16Y2);
    BSP_LCD_PrepareWriteRAM();

    u32Total = ((uint32_t)u16X2 - (uint32_t)u16X1 + 1UL) * \
               ((uint32_t)u16Y2 - (uint32_t)u16Y1 + 1UL);
    for (i = 0UL; i < u32Total; i++) {
        BSP_LCD_WriteColor(u16Color);
    }
}

/**
 * @brief  Draw the bottom color selection bar (5 filled boxes + dividers).
 */
static void DRAW_DrawButtonBar(void)
{
    uint8_t i;
    uint16_t u16X1;
    uint16_t u16X2;

    for (i = 0U; i < DRAW_BUTTON_COUNT; i++) {
        u16X1 = (uint16_t)(i * DRAW_BUTTON_WIDTH);
        u16X2 = (uint16_t)(u16X1 + DRAW_BUTTON_WIDTH - 1U);
        DRAW_FillRect(u16X1, (uint16_t)(DRAW_CANVAS_BOTTOM + 1U),
                      u16X2, (LCD_NATIVE_HEIGHT - 1U), m_astcColors[i].u16Color);

        /* Black separator between buttons (skip the left edge of the first) */
        if (0U != i) {
            BSP_LCD_DrawLine(u16X1, (uint16_t)(DRAW_CANVAS_BOTTOM + 1U),
                             u16X1, (LCD_NATIVE_HEIGHT - 1U), LCD_COLOR_BLACK);
        }
    }

    /* Top border of the button bar */
    BSP_LCD_DrawLine(0U, (uint16_t)(DRAW_CANVAS_BOTTOM + 1U),
                     (LCD_NATIVE_WIDTH - 1U), (uint16_t)(DRAW_CANVAS_BOTTOM + 1U),
                     LCD_COLOR_BLACK);
}

/**
 * @brief  Draw one line of bold text with a scalable 5x7 font.
 * @param  pu8Text  NUL-terminated string
 * @param  u16X     left edge
 * @param  u16Y     top edge
 * @param  u8ScaleX horizontal scale of each font pixel
 * @param  u8ScaleY vertical scale of each font pixel
 * @param  u8Gap    blank columns inserted after each glyph
 * @note   Bold is done by horizontally dilating the glyph (a lit column also
 *         lights its neighbour) before scaling.
 */
static void DRAW_DrawTextScaled(const char *pu8Text, uint16_t u16X, uint16_t u16Y,
                                uint8_t u8ScaleX, uint8_t u8ScaleY, uint8_t u8Gap)
{
    const char *p = pu8Text;

    while ('\0' != *p) {
        const stc_font5x7_t *pf = NULL;
        uint8_t i;

        for (i = 0U; i < (uint8_t)(sizeof(m_astcFont) / sizeof(m_astcFont[0])); i++) {
            if (m_astcFont[i].ch == *p) {
                pf = &m_astcFont[i];
                break;
            }
        }
        if (NULL != pf) {
            uint8_t col;
            for (col = 0U; col < 5U; col++) {
                uint8_t u8Bits = pf->au8Col[col];
                uint8_t row;
                if (0U != col) {
                    u8Bits |= pf->au8Col[col - 1U];   /* horizontal dilation = bold */
                }
                for (row = 0U; row < 7U; row++) {
                    if (0U != (u8Bits & (uint8_t)(1U << row))) {
                        DRAW_FillRect((uint16_t)(u16X + (uint16_t)col * u8ScaleX),
                                      (uint16_t)(u16Y + (uint16_t)row * u8ScaleY),
                                      (uint16_t)(u16X + (uint16_t)(col + 1U) * u8ScaleX - 1U),
                                      (uint16_t)(u16Y + (uint16_t)(row + 1U) * u8ScaleY - 1U),
                                      LCD_COLOR_BLACK);
                    }
                }
            }
        }
        u16X = (uint16_t)(u16X + 5U * u8ScaleX + u8Gap);
        p++;
    }
}

/**
 * @brief  Draw the two-line bold black title at the top of the canvas.
 * @note   Line 1 "SuperFly" is blown up 4x (both axes) as the headline.
 *         Line 2 has 44 characters and must fit in 320 px, so it is scaled
 *         1x horizontally but stretched 2x vertically for legibility.
 */
static void DRAW_DrawTitle(void)
{
    const uint16_t u16W1 = 8U * 5U * 4U + 7U * 3U;   /* 188 px */
    const uint16_t u16W2 = 44U * 5U + 43U * 2U;      /* 306 px */

    DRAW_DrawTextScaled(DRAW_TITLE_L1,
                        (uint16_t)((LCD_NATIVE_WIDTH - u16W1) / 2U), 4U,
                        4U, 4U, 3U);
    DRAW_DrawTextScaled(DRAW_TITLE_L2,
                        (uint16_t)((LCD_NATIVE_WIDTH - u16W2) / 2U), 38U,
                        1U, 2U, 2U);
}

/**
 * @brief  Redraw the whole UI: canvas in the selected color + the button bar.
 */
static void DRAW_RedrawUI(uint8_t u8Button)
{
    /* New canvas = bare background, so the ink mask must be wiped with it. */
    (void)memset(m_au8Ink, 0, sizeof(m_au8Ink));
    DRAW_FillRect(0U, 0U, (LCD_NATIVE_WIDTH - 1U), DRAW_CANVAS_BOTTOM,
                  m_astcColors[u8Button].u16Color);
    DRAW_DrawTitle();
    DRAW_DrawButtonBar();
}

/**
 * @brief  Convert a touch X coordinate into a button index (clamped 0..4).
 */
static uint8_t DRAW_GetButtonIndex(uint16_t u16X)
{
    uint8_t u8Idx;

    u8Idx = (uint8_t)(u16X / DRAW_BUTTON_WIDTH);
    if (u8Idx >= DRAW_BUTTON_COUNT) {
        u8Idx = (uint8_t)(DRAW_BUTTON_COUNT - 1U);
    }
    return u8Idx;
}

/**
 * @brief  Absolute difference of two pixel coordinates (no math.h needed).
 */
static uint16_t DRAW_AbsDiff(uint16_t u16A, uint16_t u16B)
{
    return (u16A >= u16B) ? (uint16_t)(u16A - u16B) : (uint16_t)(u16B - u16A);
}

/**
 * @brief  Integer square root (floor), no libm dependency.
 */
static uint16_t DRAW_ISqrt(uint32_t u32N)
{
    uint32_t u32Res = 0UL;
    uint32_t u32Bit = 0x40000000UL;

    while (u32Bit > u32N) {
        u32Bit >>= 2;
    }
    while (0UL != u32Bit) {
        if (u32N >= u32Res + u32Bit) {
            u32N -= u32Res + u32Bit;
            u32Res = (u32Res >> 1) + u32Bit;
        } else {
            u32Res >>= 1;
        }
        u32Bit >>= 2;
    }
    return (uint16_t)u32Res;
}

/**
 * @brief  Distance from a pixel centre to a line segment, in 1/16 px (Q4).
 * @note   Pixel centres are treated as integer coordinates; rounding is below
 *         half a pixel. Round caps come from clamping the projection to the
 *         segment ends, so a zero-length segment degenerates to a round dot.
 */
static uint16_t DRAW_DistQ4(uint16_t u16Px, uint16_t u16Py,
                            uint16_t u16X1, uint16_t u16Y1,
                            uint16_t u16X2, uint16_t u16Y2)
{
    int32_t  i32Dx;
    int32_t  i32Dy;
    int32_t  i32Ax;
    int32_t  i32Ay;
    int32_t  i32Bx;
    int32_t  i32By;
    int32_t  i32SegSq;
    int32_t  i32Dot;
    int32_t  i32Cross;
    uint32_t u32DistSq;

    i32Dx = (int32_t)u16X2 - (int32_t)u16X1;
    i32Dy = (int32_t)u16Y2 - (int32_t)u16Y1;
    i32Ax = (int32_t)u16Px - (int32_t)u16X1;
    i32Ay = (int32_t)u16Py - (int32_t)u16Y1;
    i32SegSq = (i32Dx * i32Dx) + (i32Dy * i32Dy);

    if (0 == i32SegSq) {
        /* Point: distance to the single endpoint */
        u32DistSq = (uint32_t)((i32Ax * i32Ax) + (i32Ay * i32Ay));
    } else {
        i32Dot = (i32Ax * i32Dx) + (i32Ay * i32Dy);
        if (i32Dot <= 0) {
            /* Before the start cap */
            u32DistSq = (uint32_t)((i32Ax * i32Ax) + (i32Ay * i32Ay));
        } else if (i32Dot >= i32SegSq) {
            /* Beyond the end cap */
            i32Bx = (int32_t)u16Px - (int32_t)u16X2;
            i32By = (int32_t)u16Py - (int32_t)u16Y2;
            u32DistSq = (uint32_t)((i32Bx * i32Bx) + (i32By * i32By));
        } else {
            /* Perpendicular distance = |cross| / |segment| */
            i32Cross = (i32Ax * i32Dy) - (i32Ay * i32Dx);
            if (i32Cross < 0) {
                i32Cross = -i32Cross;
            }
            u32DistSq = ((uint32_t)i32Cross * (uint32_t)i32Cross) / \
                        (uint32_t)i32SegSq;
        }
    }

    /* Q4: dist*16 = floor(sqrt(distSq * 256)) */
    return DRAW_ISqrt(u32DistSq << 8);
}

/**
 * @brief  Ink coverage (0..255) of the round pen at one pixel.
 * @note   Solid inside R-0.5, zero outside R+0.5, linear feather in between.
 *         8-bit resolution so the accumulated mask keeps a smooth edge even
 *         after many overlapping segments.
 */
static uint8_t DRAW_Coverage(uint16_t u16Px, uint16_t u16Py,
                             uint16_t u16X1, uint16_t u16Y1,
                             uint16_t u16X2, uint16_t u16Y2)
{
    const uint16_t u16Inner = (uint16_t)(((DRAW_PEN_SIZE / 2U) * 16U) - 8U); /* R-0.5 in Q4 */
    const uint16_t u16Outer = (uint16_t)(((DRAW_PEN_SIZE / 2U) * 16U) + 8U); /* R+0.5 in Q4 */
    uint16_t u16D = DRAW_DistQ4(u16Px, u16Py, u16X1, u16Y1, u16X2, u16Y2);

    if (u16D <= u16Inner) {
        return 255U;
    }
    if (u16D >= u16Outer) {
        return 0U;
    }
    /* (outer-d) is 1..15 -> scale to 17..255 */
    return (uint8_t)((uint16_t)(u16Outer - u16D) * 17U);
}

/**
 * @brief  Blend pen over background using an 8-bit ink level, per RGB565 channel.
 * @note   x/255 is approximated by (x+128)>>8; the error is below one LSB of
 *         a 5/6-bit channel, so it is invisible.
 */
static uint16_t DRAW_MixInk(uint16_t u16Pen, uint16_t u16Bg, uint8_t u8Ink)
{
    uint32_t u32PR;
    uint32_t u32PG;
    uint32_t u32PB;
    uint32_t u32BR;
    uint32_t u32BG;
    uint32_t u32BB;
    uint32_t u32Inv;
    uint32_t u32R;
    uint32_t u32G;
    uint32_t u32B;

    if (255U == u8Ink) {
        return u16Pen;
    }
    if (0U == u8Ink) {
        return u16Bg;
    }

    u32PR = ((uint32_t)u16Pen >> 11) & 0x1FUL;
    u32PG = ((uint32_t)u16Pen >> 5)  & 0x3FUL;
    u32PB =  (uint32_t)u16Pen        & 0x1FUL;
    u32BR = ((uint32_t)u16Bg  >> 11) & 0x1FUL;
    u32BG = ((uint32_t)u16Bg  >> 5)  & 0x3FUL;
    u32BB =  (uint32_t)u16Bg         & 0x1FUL;
    u32Inv = 255UL - (uint32_t)u8Ink;

    u32R = (((u32PR * (uint32_t)u8Ink) + (u32BR * u32Inv)) + 128UL) >> 8;
    u32G = (((u32PG * (uint32_t)u8Ink) + (u32BG * u32Inv)) + 128UL) >> 8;
    u32B = (((u32PB * (uint32_t)u8Ink) + (u32BB * u32Inv)) + 128UL) >> 8;

    return (uint16_t)((u32R << 11) | (u32G << 5) | u32B);
}

/**
 * @brief  Current canvas background color (selected by the bottom bar).
 */
static uint16_t DRAW_GetBg(void)
{
    if (m_u8LastButton < DRAW_BUTTON_COUNT) {
        return m_astcColors[m_u8LastButton].u16Color;
    }
    return LCD_COLOR_WHITE;
}

/**
 * @brief  Anti-aliased thick stroke using a signed-distance capsule.
 * @note   The segment's coverage is accumulated into m_au8Ink with max(), so
 *         consecutive segments of one stroke merge into the distance field of
 *         the whole polyline: joints can never be bleached by a later segment
 *         and the width stays constant (no "string of sausages").
 *         A scanline is re-sent only when it actually gained ink, and its
 *         whole covered run is rewritten in one RAMWR burst.
 */
static void DRAW_AAStroke(uint16_t u16X1, uint16_t u16Y1,
                          uint16_t u16X2, uint16_t u16Y2,
                          uint16_t u16Pen, uint16_t u16Bg)
{
    uint16_t u16Margin = (DRAW_PEN_SIZE / 2U) + 1U;
    uint16_t u16XMin;
    uint16_t u16XMax;
    uint16_t u16YMin;
    uint16_t u16YMax;
    uint16_t u16X;
    uint16_t u16Y;
    uint16_t u16RunStart;
    uint16_t u16RunEnd;
    uint8_t  u8Cov;
    uint8_t  u8RowChanged;
    uint32_t u32Idx;

    u16XMin = (u16X1 < u16X2) ? u16X1 : u16X2;
    u16XMax = (u16X1 > u16X2) ? u16X1 : u16X2;
    u16YMin = (u16Y1 < u16Y2) ? u16Y1 : u16Y2;
    u16YMax = (u16Y1 > u16Y2) ? u16Y1 : u16Y2;

    u16XMin = (u16XMin > u16Margin) ? (uint16_t)(u16XMin - u16Margin) : 0U;
    u16YMin = (u16YMin > u16Margin) ? (uint16_t)(u16YMin - u16Margin) : 0U;
    if (u16XMax > (LCD_NATIVE_WIDTH - 1U - u16Margin)) {
        u16XMax = LCD_NATIVE_WIDTH - 1U;
    } else {
        u16XMax = (uint16_t)(u16XMax + u16Margin);
    }
    if (u16YMax > (DRAW_CANVAS_BOTTOM - u16Margin)) {
        u16YMax = DRAW_CANVAS_BOTTOM;
    } else {
        u16YMax = (uint16_t)(u16YMax + u16Margin);
    }

    for (u16Y = u16YMin; u16Y <= u16YMax; u16Y++) {
        u16RunStart  = 0xFFFFU;
        u16RunEnd    = 0U;
        u8RowChanged = 0U;

        /* Pass 1: accumulate this segment into the ink mask (max only). */
        for (u16X = u16XMin; u16X <= u16XMax; u16X++) {
            u8Cov = DRAW_Coverage(u16X, u16Y, u16X1, u16Y1, u16X2, u16Y2);
            if (0U == u8Cov) {
                continue;
            }
            u32Idx = ((uint32_t)u16Y * LCD_NATIVE_WIDTH) + (uint32_t)u16X;
            if (u8Cov > m_au8Ink[u32Idx]) {
                m_au8Ink[u32Idx] = u8Cov;
                u8RowChanged = 1U;
            }
            if (u16RunStart > u16X) {
                u16RunStart = u16X;
            }
            u16RunEnd = u16X;
        }

        if (0U == u8RowChanged) {
            continue;
        }

        /* Pass 2: repaint the covered run from the accumulated mask. Pixels
         * that did not gain ink render to the same colour as before, so
         * rewriting the whole run is harmless and keeps the window contiguous. */
        BSP_LCD_SetWindow(u16RunStart, u16Y, u16RunEnd, u16Y);
        BSP_LCD_PrepareWriteRAM();
        for (u16X = u16RunStart; u16X <= u16RunEnd; u16X++) {
            u32Idx = ((uint32_t)u16Y * LCD_NATIVE_WIDTH) + (uint32_t)u16X;
            BSP_LCD_WriteColor(DRAW_MixInk(u16Pen, u16Bg, m_au8Ink[u32Idx]));
        }
    }
}

#if (DRAW_CURVE_ENABLE != 0U)
/**
 * @brief  Rasterise one quadratic Bezier arc as a chain of pen capsules.
 * @param  u16X0/u16Y0  arc start (midpoint of samples n-2 and n-1)
 * @param  u16Cx/u16Cy  control point (1-2-1 smoothed sample n-1)
 * @param  u16X1/u16Y1  arc end (midpoint of samples n-1 and n)
 * @note   The arc is split into chords of at most DRAW_CURVE_SEG_PX; each chord
 *         is a normal anti-aliased capsule, and because the ink mask keeps
 *         max(old, new) the chords merge without seams or bleaching.
 */
static void DRAW_QuadTo(uint16_t u16X0, uint16_t u16Y0,
                        uint16_t u16Cx, uint16_t u16Cy,
                        uint16_t u16X1, uint16_t u16Y1,
                        uint16_t u16Pen, uint16_t u16Bg)
{
    int32_t  i32Dx;
    int32_t  i32Dy;
    int32_t  i32Len;
    uint16_t u16Steps;
    uint16_t i;
    uint16_t u16Px = u16X0;
    uint16_t u16Py = u16Y0;
    uint16_t u16Nx;
    uint16_t u16Ny;

    i32Dx = (int32_t)u16X1 - (int32_t)u16X0;
    i32Dy = (int32_t)u16Y1 - (int32_t)u16Y0;
    if (i32Dx < 0) {
        i32Dx = -i32Dx;
    }
    if (i32Dy < 0) {
        i32Dy = -i32Dy;
    }
    /* Cheap upper bound on the arc length: the arc bulges, so use max+min/2. */
    i32Len = (i32Dx > i32Dy) ? (i32Dx + (i32Dy >> 1)) : (i32Dy + (i32Dx >> 1));
    u16Steps = (uint16_t)((i32Len / (int32_t)DRAW_CURVE_SEG_PX) + 1);
    if (u16Steps < 2U) {
        u16Steps = 2U;
    }
    if (u16Steps > 24U) {
        u16Steps = 24U;
    }

    for (i = 1U; i <= u16Steps; i++) {
        uint32_t u32T  = ((uint32_t)i << 8) / (uint32_t)u16Steps;  /* Q8: 0..256 */
        uint32_t u32U  = 256UL - u32T;
        uint32_t u32W0 = u32U * u32U;                  /* (1-t)^2 * 256^2        */
        uint32_t u32W1 = (u32T * u32U) * 2UL;          /* 2t(1-t) * 256^2        */
        uint32_t u32W2 = u32T * u32T;                  /* t^2 * 256^2            */

        u16Nx = (uint16_t)(((u32W0 * u16X0) + (u32W1 * u16Cx) + (u32W2 * u16X1)) >> 16);
        u16Ny = (uint16_t)(((u32W0 * u16Y0) + (u32W1 * u16Cy) + (u32W2 * u16Y1)) >> 16);

        DRAW_AAStroke(u16Px, u16Py, u16Nx, u16Ny, u16Pen, u16Bg);
        u16Px = u16Nx;
        u16Py = u16Ny;
    }
}
#endif /* DRAW_CURVE_ENABLE */

/**
 * @brief  Reset the smooth-stroke geometry state.
 */
static void DRAW_StrokeReset(void)
{
    m_u8PtCount = 0U;
    m_u16Q1X = 0U;
    m_u16Q1Y = 0U;
    m_u16Q2X = 0U;
    m_u16Q2Y = 0U;
}

/**
 * @brief  Start a new stroke at (x,y): reset the curve and stamp the first dot.
 */
static void DRAW_StrokeBegin(uint16_t u16X, uint16_t u16Y)
{
    DRAW_StrokeReset();
    m_u8PtCount = 1U;
    m_u16Q1X = u16X;
    m_u16Q1Y = u16Y;
    m_u16Q2X = u16X;
    m_u16Q2Y = u16Y;
    DRAW_AAStroke(u16X, u16Y, u16X, u16Y, LCD_COLOR_BLACK, DRAW_GetBg());
}

/**
 * @brief  Feed one accepted sample into the smooth stroke.
 * @note   Quadratic arcs through the SAMPLE MIDPOINTS keep the curve C1
 *         continuous (no corners), and the 1-2-1 control point is a symmetric
 *         (zero phase lag) filter that cancels alternating touch jitter.
 */
static void DRAW_StrokeAdd(uint16_t u16X, uint16_t u16Y)
{
    if (0U == m_u8PtCount) {
        DRAW_StrokeBegin(u16X, u16Y);
        return;
    }

#if (DRAW_CURVE_ENABLE == 0U)
    /* Plain polyline: connect the last sample directly (old behaviour). */
    DRAW_AAStroke(m_u16Q1X, m_u16Q1Y, u16X, u16Y, LCD_COLOR_BLACK, DRAW_GetBg());
#else
    {
        uint16_t u16M0x;
        uint16_t u16M0y;
        uint16_t u16M1x;
        uint16_t u16M1y;
        uint16_t u16Cx;
        uint16_t u16Cy;

        u16M1x = (uint16_t)(((uint32_t)m_u16Q1X + (uint32_t)u16X + 1UL) >> 1);
        u16M1y = (uint16_t)(((uint32_t)m_u16Q1Y + (uint32_t)u16Y + 1UL) >> 1);

        if (1U == m_u8PtCount) {
            /* Second sample: only one arc start exists, draw a straight head. */
            DRAW_AAStroke(m_u16Q1X, m_u16Q1Y, u16M1x, u16M1y,
                          LCD_COLOR_BLACK, DRAW_GetBg());
        } else {
            u16M0x = (uint16_t)(((uint32_t)m_u16Q2X + (uint32_t)m_u16Q1X + 1UL) >> 1);
            u16M0y = (uint16_t)(((uint32_t)m_u16Q2Y + (uint32_t)m_u16Q1Y + 1UL) >> 1);
            /* 1-2-1 smoothing of the middle sample, rounded: (Q2 + 2*Q1 + Qn)/4 */
            u16Cx  = (uint16_t)(((uint32_t)m_u16Q2X + (2UL * (uint32_t)m_u16Q1X) + \
                                 (uint32_t)u16X + 2UL) >> 2);
            u16Cy  = (uint16_t)(((uint32_t)m_u16Q2Y + (2UL * (uint32_t)m_u16Q1Y) + \
                                 (uint32_t)u16Y + 2UL) >> 2);

            DRAW_QuadTo(u16M0x, u16M0y, u16Cx, u16Cy, u16M1x, u16M1y,
                        LCD_COLOR_BLACK, DRAW_GetBg());
        }
    }
#endif

    /* Slide the history window by one sample. */
    m_u16Q2X = m_u16Q1X;
    m_u16Q2Y = m_u16Q1Y;
    m_u16Q1X = u16X;
    m_u16Q1Y = u16Y;
    if (m_u8PtCount < 2U) {
        m_u8PtCount = 2U;
    }
}

/**
 * @brief  Finish a stroke: draw the missing tail up to the last sample.
 * @note   The curve ends at mid(Q2,Q1), i.e. half a sample short of the finger.
 *         Without this flush every stroke would stop a few pixels early.
 */
static void DRAW_StrokeEnd(void)
{
    uint16_t u16Mx;
    uint16_t u16My;

    if (m_u8PtCount >= 2U) {
        u16Mx = (uint16_t)(((uint32_t)m_u16Q2X + (uint32_t)m_u16Q1X + 1UL) >> 1);
        u16My = (uint16_t)(((uint32_t)m_u16Q2Y + (uint32_t)m_u16Q1Y + 1UL) >> 1);
        DRAW_AAStroke(u16Mx, u16My, m_u16Q1X, m_u16Q1Y, LCD_COLOR_BLACK, DRAW_GetBg());
    }
    DRAW_StrokeReset();
}

/**
 * @brief  Squared distance between two points (no math.h / sqrt needed).
 */
static uint32_t DRAW_DistSq(uint16_t u16X1, uint16_t u16Y1,
                            uint16_t u16X2, uint16_t u16Y2)
{
    uint32_t u32Dx = (u16X1 >= u16X2) ? ((uint32_t)u16X1 - u16X2)
                                     : ((uint32_t)u16X2 - u16X1);
    uint32_t u32Dy = (u16Y1 >= u16Y2) ? ((uint32_t)u16Y1 - u16Y2)
                                     : ((uint32_t)u16Y2 - u16Y1);
    return (u32Dx * u32Dx) + (u32Dy * u32Dy);
}

/**
 * @brief  Plausible single-sample step, growing with the stroke's own speed.
 * @note   A fixed threshold that rejects a fast circle once will reject every
 *         following sample as well (the last accepted point never advances).
 *         Allow base + 2*previous_step, capped at 3*base.
 */
static uint16_t DRAW_StepLimit(void)
{
    uint32_t u32Limit = (uint32_t)DRAW_STROKE_MAX_STEP + (2UL * (uint32_t)m_u16LastStep);
    uint32_t u32Cap   = 3UL * (uint32_t)DRAW_STROKE_MAX_STEP;

    return (uint16_t)((u32Limit > u32Cap) ? u32Cap : u32Limit);
}

/**
 * @brief  Remember the step of an accepted sample (Chebyshev distance).
 */
static void DRAW_NoteStep(uint16_t u16X1, uint16_t u16Y1,
                          uint16_t u16X2, uint16_t u16Y2)
{
    uint16_t u16Dx = DRAW_AbsDiff(u16X1, u16X2);
    uint16_t u16Dy = DRAW_AbsDiff(u16Y1, u16Y2);

    m_u16LastStep = (u16Dx > u16Dy) ? u16Dx : u16Dy;
}

#if (DRAW_PENDING_ENABLE != 0U)
static uint16_t DRAW_PendingLastX(void);
static uint16_t DRAW_PendingLastY(void);
#endif

/**
 * @brief  Pick the touch point that belongs to the CURRENT gesture.
 * @retval 1 = m_u16SampleX/Y updated, 0 = no usable point in this frame
 * @note   FT6336U reports an event flag per point (put-down / put-up / contact)
 *         and may report two points in one frame. Blindly using P1 let a phantom
 *         "put-down" at an unrelated location start a stroke, while the real
 *         finger was reported as P2.
 *           - no gesture   : a fresh put-down wins; contact is also accepted;
 *                            put-up is ignored (it is a release, not a press).
 *           - active gesture: put-down is ignored (a second finger / ghost);
 *                            among the remaining points pick the one nearest
 *                            to the last accepted position, so P1/P2 swapping
 *                            cannot break the stroke.
 */
static uint8_t DRAW_SelectTouch(void)
{
    stc_touch_point_t astcPt[2U];
    uint8_t  u8N = TOUCH_GetPoints(astcPt, 2U);
    uint8_t  i;
    uint8_t  u8Best = 0xFFU;
    uint8_t  u8BestStrict = 0xFFU;   /* best point that is NOT a fresh put-down */
    uint32_t u32BestDist = 0xFFFFFFFFUL;
    uint32_t u32StrictDist = 0xFFFFFFFFUL;
    uint32_t u32Dist;
    uint16_t u16RefX;
    uint16_t u16RefY;

    if (0U == u8N) {
        return 0U;
    }

    if (DRAW_GESTURE_NONE == m_u8Gesture) {
        /* Looking for a new press. Prefer an explicit "put-down" frame. */
        for (i = 0U; i < u8N; i++) {
#if (DRAW_TRACK_EVENT_FILTER != 0U)
            if (TOUCH_EVT_PUT_UP == astcPt[i].u8Event) {
                continue;
            }
            if (TOUCH_EVT_PUT_DOWN == astcPt[i].u8Event) {
                u8Best = i;
                break;
            }
#endif
            if (0xFFU == u8Best) {
                u8Best = i;
            }
        }
    } else {
        /* Mid-gesture: only the tracked finger may contribute samples. */
#if (DRAW_PENDING_ENABLE != 0U)
        if (0U != m_u8PendCount) {
            u16RefX = DRAW_PendingLastX();
            u16RefY = DRAW_PendingLastY();
        } else {
            u16RefX = m_u16LastX;
            u16RefY = m_u16LastY;
        }
#else
        u16RefX = m_u16LastX;
        u16RefY = m_u16LastY;
#endif
        for (i = 0U; i < u8N; i++) {
            /* A button gesture lives in the bar, far from the last canvas
             * point; proximity selection does not apply there - take the first
             * eligible point, and let the button-state machine do the routing. */
            if (DRAW_GESTURE_BUTTON == m_u8Gesture) {
                u32Dist = (uint32_t)i;
            } else {
                u32Dist = DRAW_DistSq(u16RefX, u16RefY, astcPt[i].u16X, astcPt[i].u16Y);
            }
            if (u32Dist < u32BestDist) {
                u32BestDist = u32Dist;
                u8Best      = i;
            }
#if (DRAW_TRACK_EVENT_FILTER != 0U)
            if (TOUCH_EVT_PUT_DOWN != astcPt[i].u8Event) {
                if (u32Dist < u32StrictDist) {
                    u32StrictDist = u32Dist;
                    u8BestStrict  = i;
                }
            }
#endif
        }
#if (DRAW_TRACK_EVENT_FILTER != 0U)
        /* Reject a fresh put-down in the middle of a gesture - unless THIS
         * panel firmware reports put-down for every frame, in which case there
         * is no alternative and falling back keeps the board usable. */
        if (0xFFU != u8BestStrict) {
            u8Best = u8BestStrict;
        }
#endif
    }

    if (0xFFU == u8Best) {
        return 0U;
    }
    m_u16SampleX = astcPt[u8Best].u16X;
    m_u16SampleY = astcPt[u8Best].u16Y;
    return 1U;
}

#if (DRAW_PENDING_ENABLE != 0U)
/**
 * @brief  Forget the buffered samples of an unproven press (nothing was painted).
 */
static void DRAW_PendingReset(void)
{
    m_u8PendCount = 0U;
    m_u16PendMove = 0U;
}

/**
 * @brief  Last buffered sample of the unproven press.
 */
static uint16_t DRAW_PendingLastX(void)
{
    return m_astcPend[m_u8PendCount - 1U].u16X;
}

static uint16_t DRAW_PendingLastY(void)
{
    return m_astcPend[m_u8PendCount - 1U].u16Y;
}

/**
 * @brief  Buffer one sample of an unproven press and track its furthest excursion.
 * @note   When the buffer is full the anchor stays and only the newest sample is
 *         kept: while a press stays inside DRAW_PENDING_MIN_MOVE the samples are
 *         nearly identical, so dropping the middle of the run loses no geometry,
 *         and once the pen really moves the commit fires immediately anyway.
 */
static void DRAW_PendingPush(uint16_t u16X, uint16_t u16Y)
{
    uint16_t u16Move;

    if (0U == m_u8PendCount) {
        m_astcPend[0].u16X = u16X;
        m_astcPend[0].u16Y = u16Y;
        m_u8PendCount      = 1U;
        m_u16PendMove      = 0U;
        return;
    }

    u16Move = DRAW_ISqrt(DRAW_DistSq(m_astcPend[0].u16X, m_astcPend[0].u16Y, u16X, u16Y));
    if (u16Move > m_u16PendMove) {
        m_u16PendMove = u16Move;
    }

    if (m_u8PendCount < DRAW_PENDING_MAX) {
        m_astcPend[m_u8PendCount].u16X = u16X;
        m_astcPend[m_u8PendCount].u16Y = u16Y;
        m_u8PendCount++;
    } else {
        m_astcPend[DRAW_PENDING_MAX - 1U].u16X = u16X;
        m_astcPend[DRAW_PENDING_MAX - 1U].u16Y = u16Y;
    }
}

/**
 * @brief  1 = the press has proven itself (the pen really moved away).
 */
static uint8_t DRAW_PendingProven(void)
{
    return (m_u16PendMove >= DRAW_PENDING_MIN_MOVE) ? 1U : 0U;
}

/**
 * @brief  Replay the buffered samples into a real stroke, then free the buffer.
 */
static void DRAW_PendingCommit(void)
{
    uint8_t i;

    if (0U == m_u8PendCount) {
        return;
    }

    DRAW_StrokeBegin(m_astcPend[0].u16X, m_astcPend[0].u16Y);
    for (i = 1U; i < m_u8PendCount; i++) {
        DRAW_StrokeAdd(m_astcPend[i].u16X, m_astcPend[i].u16Y);
    }
    m_u16LastX = m_astcPend[m_u8PendCount - 1U].u16X;
    m_u16LastY = m_astcPend[m_u8PendCount - 1U].u16Y;
    DRAW_PendingReset();
}
#endif /* DRAW_PENDING_ENABLE */

/**
 * @brief  Finish a canvas gesture: commit + flush a real stroke, drop a ghost one.
 */
static void DRAW_CanvasPenUp(uint32_t u32Now)
{
#if (DRAW_PENDING_ENABLE != 0U)
    if (0U != m_u8PendCount) {
        uint16_t u16Move = m_u16PendMove;   /* commit clears the buffer */

        if (0U != DRAW_PendingProven()) {
            DRAW_PendingCommit();
            DRAW_StrokeEnd();
#if (DRAW_TOUCH_LOG != 0U)
            DDL_Printf("Stroke: %u ms, %u frames, %u drops (proven after %u px)\r\n",
                       (unsigned int)(u32Now - m_u32GestureTick),
                       (unsigned int)m_u16StrokeFrames,
                       (unsigned int)m_u16DropCount,
                       (unsigned int)u16Move);
#endif
        } else {
            /* Never painted anything, so there is nothing to erase. */
#if (DRAW_TOUCH_LOG != 0U)
            DDL_Printf("Ghost press dropped: moved %u px / %u ms\r\n",
                       (unsigned int)u16Move,
                       (unsigned int)(u32Now - m_u32GestureTick));
#endif
            DRAW_PendingReset();
        }
        return;
    }
#endif

    DRAW_StrokeEnd();
#if (DRAW_TOUCH_LOG != 0U)
    DDL_Printf("Stroke: %u ms, %u frames, %u drops\r\n",
               (unsigned int)(u32Now - m_u32GestureTick),
               (unsigned int)m_u16StrokeFrames,
               (unsigned int)m_u16DropCount);
#endif
}

/**
 * @brief  Clear the pen-down vote (pen lift / press consumed).
 */
static void DRAW_ResetPress(void)
{
    m_u8HaveAnchor = 0U;
    m_u8PressVotes = 0U;
}

/**
 * @brief  Has u32Ms elapsed since u32Since?
 * @note   Returns 1 when the 1 ms time base is unavailable, so a broken SysTick
 *         degrades the filters to "plain sample voting" instead of freezing the
 *         touch input completely.
 */
static uint8_t DRAW_Elapsed(uint32_t u32Now, uint32_t u32Since, uint32_t u32Ms)
{
    if (0U == m_u8TickOk) {
        return 1U;
    }
    return ((uint32_t)(u32Now - u32Since) >= u32Ms) ? 1U : 0U;
}

/**
 * @brief  Confirm a new press: continuous presence for DRAW_PRESS_HOLD_MS.
 * @param  u16X/u16Y  latest mapped touch point
 * @param  u32Now     SysTick milliseconds
 * @retval 1 = press accepted (m_u16AnchorX/m_u16AnchorY is the touch-down point)
 * @note   The test is CONTINUITY + TIME, not "N stable samples around a fixed
 *         point":
 *           - the fast poll loop reads one scan frame many times, so counting
 *             samples alone would confirm a single ghost frame instantly;
 *           - requiring a fixed anchor with a small tolerance would stall a
 *             FAST stroke start (the finger keeps walking out of the window and
 *             the vote keeps restarting), so movement is allowed as long as it
 *             is physically possible between two scan frames;
 *           - a teleport (ghost point, > DRAW_STROKE_MAX_STEP) restarts the
 *             sequence, so a ghost cannot hijack a press that began elsewhere.
 *         A release transient is left over from the previous gesture; the
 *         quiet window after a pen-up (DRAW_PENUP_QUIET_MS) covers that.
 */
static uint8_t DRAW_PollPress(uint16_t u16X, uint16_t u16Y, uint32_t u32Now)
{
    if (0U == m_u8HaveAnchor) {
        m_u8HaveAnchor  = 1U;
        m_u8PressVotes  = 1U;
        m_u16AnchorX    = u16X;   /* touch-down point = stroke start / owner */
        m_u16AnchorY    = u16Y;
        m_u16PrevX      = u16X;
        m_u16PrevY      = u16Y;
        m_u32AnchorTick = u32Now;
        return 0U;
    }

    if (DRAW_DistSq(m_u16PrevX, m_u16PrevY, u16X, u16Y) > DRAW_STROKE_MAX_STEP_SQ) {
        /* Discontinuity: this is not the same touch sequence, restart. */
        m_u8PressVotes  = 1U;
        m_u16AnchorX    = u16X;
        m_u16AnchorY    = u16Y;
        m_u16PrevX      = u16X;
        m_u16PrevY      = u16Y;
        m_u32AnchorTick = u32Now;
        return 0U;
    }

    m_u16PrevX = u16X;
    m_u16PrevY = u16Y;

    if (m_u8PressVotes < DRAW_PRESS_VOTES) {
        m_u8PressVotes++;
    }

    if ((m_u8PressVotes >= DRAW_PRESS_VOTES) && \
        (0U != DRAW_Elapsed(u32Now, m_u32AnchorTick, DRAW_PRESS_HOLD_MS))) {
        m_u8HaveAnchor = 0U;    /* keep the anchor coords for the caller */
        m_u8PressVotes = 0U;
        return 1U;
    }
    return 0U;
}

/*******************************************************************************
 * SysTick: 1 ms time base for the touch state machine
 ******************************************************************************/
void SysTick_Handler(void)
{
    SysTick_IncTick();
}

/*******************************************************************************
 * Main
 ******************************************************************************/
int32_t main(void)
{
    uint16_t u16X;
    uint16_t u16Y;
    uint32_t u32Now;
    uint8_t  u8Pressed;
    uint8_t  u8Button;

    /* Unlock write-protected peripheral registers */
    LL_PERIPH_WE(LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU | \
                 LL_PERIPH_EFM | LL_PERIPH_SRAM);

    /* PLLH 240 MHz; EXCLK = HCLK/8 = 30 MHz for the SMC bus */
    BSP_CLK_Init();
    CLK_SetClockDiv(CLK_BUS_EXCLK, LCD_EXCLK_DIV);

    /* ------------------------------------------------------------------
     * TIME BASE REPAIR - root cause of the "phantom strokes".
     * SystemInit() ran SystemCoreClockUpdate() while the core was still on
     * the internal HRC, so SystemCoreClock kept the HRC value. BSP_CLK_Init()
     * then switches to the 240 MHz PLL without refreshing it, and
     *   HCLK_VALUE    (SysTick reload, DDL_DelayMS)
     *   I2C_SRC_CLK   (touch I2C divider)
     * are macros over SystemCoreClock. Result before this call: the "1 ms"
     * tick was really 66.7 us (15x fast), so DRAW_PRESS_HOLD_MS = 30 ms was
     * really 2 ms - far below one FT6336U scan frame, which let a single
     * ghost frame confirm itself and paint a stray mark.
     * ------------------------------------------------------------------ */
    SystemCoreClockUpdate();

    /* Optional debug console (USART1, 115200) */
    BSP_PRINTF_Init();
    DDL_Printf("HC32F4A0 4.0inch touch drawing board\r\n");

    /* Cross-check the repaired value against the clock registers themselves
     * (CLK_GetClockFreq reads CMU, it does not use SystemCoreClock). */
    {
        stc_clock_freq_t stcFreq;

        (void)CLK_GetClockFreq(&stcFreq);
        if (stcFreq.u32HclkFreq != SystemCoreClock) {
            SystemCoreClock = stcFreq.u32HclkFreq;   /* trust the registers */
        }
        DDL_Printf("HCLK: registers=%u Hz, SystemCoreClock=%u Hz\r\n",
                   (unsigned int)stcFreq.u32HclkFreq, (unsigned int)SystemCoreClock);
        DDL_Printf("PCLK1=%u Hz (touch I2C), EXCLK=%u Hz (SMC)\r\n",
                   (unsigned int)stcFreq.u32Pclk1Freq, (unsigned int)stcFreq.u32ExclkFreq);
    }

    /* 1 ms tick. The touch confirmations below are TIME based: the FT6336U
     * scan is ~100 Hz, so counting loop iterations would treat one ghost scan
     * frame as many "stable" samples. */
    (void)SysTick_Init(1000UL);
    {
        uint32_t u32T0     = SysTick_GetTick();
        uint32_t u32Spin   = 0UL;
        uint32_t u32Reload = SysTick->LOAD + 1UL;
        uint32_t u32Expect = SystemCoreClock / 1000UL;

        while ((SysTick_GetTick() == u32T0) && (u32Spin < 400000UL)) {
            u32Spin++;
        }
        m_u8TickOk = (SysTick_GetTick() != u32T0) ? 1U : 0U;

        /* A moving counter is not enough - it must move at 1 kHz. Before the
         * repair above this check printed 16000 vs 240000 (FAIL). */
        DDL_Printf("SysTick reload: %u (expect %u) -> %s\r\n",
                   (unsigned int)u32Reload, (unsigned int)u32Expect,
                   (u32Reload == u32Expect) ? "PASS" : "FAIL");
    }

    /* Panel control GPIOs: TCA9539 LCD /RST + PI0 backlight */
    BSP_LCD_IO_Init();

    /* Backlight ON immediately as a "program is alive" signal */
    BSP_LCD_BKLCmd(LCD_PIN_SET);

    /* Hardware reset: /RESET low >= 50 ms, high >= 120 ms */
    BSP_LCD_RSTCmd(LCD_PIN_RESET);
    DDL_DelayMS(50UL);
    BSP_LCD_RSTCmd(LCD_PIN_SET);
    DDL_DelayMS(120UL);

    /* FT6336U I2C probe */
    TOUCH_Init();

    /* SMC + ST7796 init */
    BSP_LCD_Init();

    /* The first RAMWR burst after DISPON can be swallowed by the ST7796.
     * Give the panel time, then throw away one full write before the real UI. */
    DDL_DelayMS(DRAW_PANEL_SETTLE_MS);
    BSP_LCD_Clear(LCD_COLOR_BLACK);
    DDL_DelayMS(50UL);

    /* Initial UI: white canvas + 5 color buttons */
    DRAW_RedrawUI(0U);
    m_u8LastButton = 0U;
    m_u8Gesture     = DRAW_GESTURE_NONE;
    DRAW_StrokeReset();
    DRAW_ResetPress();
    m_u16DropCount   = 0U;
    m_u8BtnCandidate = 0xFFU;
    m_u8BtnVotes     = 0U;
    DDL_Printf("Background: WHITE\r\n");
    DDL_Printf("Touch timebase: %s\r\n", (0U != m_u8TickOk) ? "1 ms SysTick OK"
                                                          : "FAILED, votes only");

    LL_PERIPH_WP(LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU | \
                 LL_PERIPH_EFM | LL_PERIPH_SRAM);

    for (;;) {
        u32Now = SysTick_GetTick();

        /* ---- One touch read per FT6336U scan frame -------------------------
         * The IC scans at ~100 Hz. Reading it faster only re-reads the SAME
         * frame, which makes the press votes ("N samples") meaningless.
         * Throttling to one read per frame makes 1 sample == 1 new frame, and
         * it also stops the I2C bus from being hammered by the CPU. */
        if (0U == DRAW_Elapsed(u32Now, m_u32PollTick, TOUCH_POLL_INTERVAL_MS)) {
            continue;
        }
        m_u32PollTick = u32Now;

        /* Use the event flag + proximity to pick the point belonging to the
         * ongoing gesture; never blindly use P1. */
        u8Pressed = DRAW_SelectTouch();
        u16X      = m_u16SampleX;
        u16Y      = m_u16SampleY;

        if (0U != u8Pressed) {
            /* ---------------- Pen-down: decide who owns this gesture -------- */
            if (DRAW_GESTURE_NONE == m_u8Gesture) {
                /* Lifting a finger leaves release transients / ghost points for
                 * a few frames; stay quiet for a moment before believing a
                 * new press. Imperceptible for a human, fatal for a ghost. */
                if (0U == DRAW_Elapsed(u32Now, m_u32PenUpTick, DRAW_PENUP_QUIET_MS)) {
                    DRAW_ResetPress();
                    continue;
                }
                if (0U == DRAW_PollPress(u16X, u16Y, u32Now)) {
                    continue;
                }

                /* The stable ANCHOR decides the owner of the gesture, and the
                 * owner can never change until the finger is lifted. */
                m_u32GestureTick  = u32Now;
                m_u16StrokeFrames = 0U;
                if (m_u16AnchorY > DRAW_CANVAS_BOTTOM) {
                    m_u8Gesture      = DRAW_GESTURE_BUTTON;
                    m_u8BtnCandidate = DRAW_GetButtonIndex(m_u16AnchorX);
                    m_u8BtnVotes     = DRAW_BUTTON_VOTES;  /* press vote covers it */
                    if (m_u8BtnCandidate != m_u8LastButton) {
                        DRAW_RedrawUI(m_u8BtnCandidate);
                        m_u8LastButton = m_u8BtnCandidate;
                        DDL_Printf("Background: %s\r\n",
                                   m_astcColors[m_u8BtnCandidate].pu8Name);
                    }
                } else {
                    m_u8Gesture    = DRAW_GESTURE_CANVAS;
                    m_u16LastX     = m_u16AnchorX;
                    m_u16LastY     = m_u16AnchorY;
                    m_u16DropCount = 0U;
                    m_u16LastStep  = 0U;
#if (DRAW_PENDING_ENABLE != 0U)
                    /* Paint NOTHING yet: buffer the press until it proves
                     * itself. */
                    DRAW_PendingReset();
                    DRAW_PendingPush(m_u16AnchorX, m_u16AnchorY);
#else
                    DRAW_StrokeBegin(m_u16AnchorX, m_u16AnchorY);
                    m_u16StrokeFrames = 1U;
#endif
                }
                continue;
            }

            /* ---------------- Button gesture: it owns the bar only ---------- */
            if (DRAW_GESTURE_BUTTON == m_u8Gesture) {
                if (u16Y > DRAW_CANVAS_BOTTOM) {
                    u8Button = DRAW_GetButtonIndex(u16X);
                    if (u8Button != m_u8BtnCandidate) {
                        m_u8BtnCandidate = u8Button;
                        m_u8BtnVotes     = 1U;
                    } else if (m_u8BtnVotes < DRAW_BUTTON_VOTES) {
                        m_u8BtnVotes++;
                    }
                    /* Repaint only when the target really changed, otherwise a
                     * held finger would refill the whole screen every poll. */
                    if ((m_u8BtnVotes >= DRAW_BUTTON_VOTES) && \
                        (u8Button != m_u8LastButton)) {
                        m_u8BtnVotes = 0U;
                        DRAW_RedrawUI(u8Button);
                        m_u8LastButton = u8Button;
                        DDL_Printf("Background: %s\r\n", m_astcColors[u8Button].pu8Name);
                    }
                }
                continue;   /* a bar gesture must never paint the canvas */
            }

            /* ---------------- Canvas gesture: it owns the canvas only ------- */
            if (u16Y > DRAW_CANVAS_BOTTOM) {
                /* This gesture was born on the canvas, so the bar is not its
                 * territory. Dropping the sample is what makes "background
                 * changes while drawing" structurally impossible. */
                continue;
            }

            {
                uint32_t u32Limit = DRAW_StepLimit();
                uint32_t u32LimitSq = u32Limit * u32Limit;

#if (DRAW_PENDING_ENABLE != 0U)
                if (0U != m_u8PendCount) {
                    uint16_t u16PrevX = DRAW_PendingLastX();
                    uint16_t u16PrevY = DRAW_PendingLastY();

                    /* Unproven press: still nothing on the glass. */
                    if (DRAW_DistSq(u16PrevX, u16PrevY, u16X, u16Y) > u32LimitSq) {
                        /* Implausible sample: DROP it. Never re-anchor here -
                         * the old re-anchor painted a line at the ghost point
                         * (see the re-anchor note in the design history). */
                        m_u16DropCount++;
                        m_u16StrokeFrames++;
                        continue;
                    }

                    DRAW_PendingPush(u16X, u16Y);
                    DRAW_NoteStep(u16PrevX, u16PrevY, u16X, u16Y);
                    if (0U != DRAW_PendingProven()) {
                        DRAW_PendingCommit();   /* replay: the stroke appears now */
                    }
                    m_u16StrokeFrames++;
                    continue;
                }
#endif

                if (DRAW_DistSq(m_u16LastX, m_u16LastY, u16X, u16Y) > u32LimitSq) {
                    /* Out-of-family sample: ignore it and keep the previous pen
                     * position. The next real sample continues the SAME stroke,
                     * so a fast circle has no gaps and no line reaches the ghost. */
                    m_u16DropCount++;
                    m_u16StrokeFrames++;
                } else {
                    /* Identical repeats are dropped (they would only add
                     * degenerate arcs). */
                    if ((DRAW_AbsDiff(u16X, m_u16LastX) >= DRAW_MIN_STEP_PX) || \
                        (DRAW_AbsDiff(u16Y, m_u16LastY) >= DRAW_MIN_STEP_PX)) {
                        DRAW_StrokeAdd(u16X, u16Y);
                        DRAW_NoteStep(m_u16LastX, m_u16LastY, u16X, u16Y);
                        m_u16LastX = u16X;
                        m_u16LastY = u16Y;
                        m_u16StrokeFrames++;
                    }
                }
            }
        } else {
            /* ---------------- Pen-up ---------------------------------------- */
            if (DRAW_GESTURE_CANVAS == m_u8Gesture) {
                DRAW_CanvasPenUp(u32Now);   /* commit or drop, then flush tail */
            }
            if (DRAW_GESTURE_NONE != m_u8Gesture) {
                m_u32PenUpTick = u32Now;
            }
            m_u8Gesture      = DRAW_GESTURE_NONE;
            DRAW_ResetPress();
#if (DRAW_PENDING_ENABLE != 0U)
            DRAW_PendingReset();
#endif
            m_u8BtnCandidate = 0xFFU;
            m_u8BtnVotes     = 0U;
        }
    }
}

/**
 * @}
 */
