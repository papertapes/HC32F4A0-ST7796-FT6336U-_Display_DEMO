/**
 *******************************************************************************
 * @file  lcd_panel.c
 * @brief ST7796 / ILI9488 init tables, ID auto-detection and RGB565 drawing.
 *
 * @note  Sources of the init tables:/n *          - ST7796 : Bodmer/TFT_eSPI  TFT_Drivers/ST7796_Init.h
 *          - ILI9488: Bodmer/TFT_eSPI  TFT_Drivers/ILI9488_Init.h (parallel 16-bit branch,
 *                     COLMOD = 0x55)
 *          - Drawing geometry: XHSC DDL components/nt35510/nt35510.c (BSD-3-Clause)
 *
 *        The ST7796 table was aligned to the panel vendor's own init sequence:
 *          CUST_屏/CL40BC267-40A_IPS_CTP/IC&Code/ST7796S_BOE3.98_IPS_Init.txt
 *          Key point: the vendor table ends with INVON (0x21) before DISPON.
 *          ST7796 powers up in "display inversion off"; this BOE IPS module needs
 *          inversion ON for correct colors. INVOFF (0x20) is a no-op at power-up
 *          (datasheet: "no effect when already in inversion off mode").
 *
 *        Fixes compared with the official nt35510.c driver:
 *          1) CASET (0x2A) and RASET (0x2B) are BOTH written before every RAMWR.
 *             The official driver wrote 0x2B twice and never set 0x2A.
 *          2) Width/height come from the device table (320x480), not hardcoded 240x320.
 *          3) A failed ID probe falls back instead of dereferencing a NULL device pointer.
 *******************************************************************************
 * Copyright (C) 2022-2025, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 * This software component is licensed by XHSC under BSD 3-Clause license.
 *******************************************************************************
 */
#include <stdio.h>
#include "lcd_panel.h"

/* ------------------------------------------------------------------------------------------------
 * Command / register definitions (ST7796S & ILI9488 datasheets, command table)
 * ---------------------------------------------------------------------------------------------- */
#define LCD_CMD_NOP                                 (0x00U)
#define LCD_CMD_SWRESET                             (0x01U)
#define LCD_CMD_RDDID                               (0x04U)
#define LCD_CMD_SLPIN                               (0x10U)
#define LCD_CMD_SLPOUT                              (0x11U)
#define LCD_CMD_INVOFF                              (0x20U)
#define LCD_CMD_INVON                               (0x21U)
#define LCD_CMD_DISPOFF                             (0x28U)
#define LCD_CMD_DISPON                              (0x29U)
#define LCD_CMD_CASET                               (0x2AU)
#define LCD_CMD_RASET                               (0x2BU)
#define LCD_CMD_RAMWR                               (0x2CU)
#define LCD_CMD_MADCTL                              (0x36U)
#define LCD_CMD_COLMOD                              (0x3AU)
#define LCD_CMD_READ_ID4                            (0xD3U)  /* ILI9488: read ID4 */

/* MADCTL bits */
#define LCD_MADCTL_MY                               (0x80U)
#define LCD_MADCTL_MX                               (0x40U)
#define LCD_MADCTL_MV                               (0x20U)
#define LCD_MADCTL_ML                               (0x10U)
#define LCD_MADCTL_BGR                              (0x08U)
#define LCD_MADCTL_MH                               (0x04U)

/* Portrait / landscape MADCTL: MX+BGR and MV+BGR, same as TFT_eSPI rotation 0/1 */
#define LCD_MADCTL_VERTICAL                         (LCD_MADCTL_MX | LCD_MADCTL_BGR)
#define LCD_MADCTL_HORIZONTAL                       (LCD_MADCTL_MV | LCD_MADCTL_BGR)

/* ------------------------------------------------------------------------------------------------
 * Types
 * ---------------------------------------------------------------------------------------------- */
typedef struct {
    uint8_t  u8Cmd;
    uint8_t  u8Len;
    uint8_t  au8Data[16U];
    uint16_t u16DelayMs;
} stc_lcd_init_cmd_t;

typedef struct {
    uint16_t u16Id;
    uint16_t u16Width;
    uint16_t u16Height;
    void (*Init)(void);
} stc_lcd_panel_t;

/* ------------------------------------------------------------------------------------------------
 * Debug observables
 * ---------------------------------------------------------------------------------------------- */
uint16_t g_u16PanelId       = LCD_ID_INVALID;
uint8_t  g_u8PanelIdFallback = 0U;
uint8_t  g_au8IdRaw04[4U]  = {0U, 0U, 0U, 0U};
uint8_t  g_au8IdRawD3[4U]  = {0U, 0U, 0U, 0U};

/* ------------------------------------------------------------------------------------------------
 * Private state
 * ---------------------------------------------------------------------------------------------- */
static uint16_t m_u16Width  = LCD_NATIVE_WIDTH;
static uint16_t m_u16Height = LCD_NATIVE_HEIGHT;
static uint16_t m_u16Dir    = LCD_DISPLAY_VERTICAL;

static void LCD_ST7796_Init(void);
static void LCD_ILI9488_Init(void);

static const stc_lcd_panel_t m_astcPanel[] = {
    {LCD_ID_ST7796,  LCD_NATIVE_WIDTH, LCD_NATIVE_HEIGHT, LCD_ST7796_Init},
    {LCD_ID_ILI9488, LCD_NATIVE_WIDTH, LCD_NATIVE_HEIGHT, LCD_ILI9488_Init},
};

static const stc_lcd_panel_t *m_pstcPanel = &m_astcPanel[0];

/* ------------------------------------------------------------------------------------------------
 * Init tables
 * ---------------------------------------------------------------------------------------------- */
/* ST7796S, aligned to the panel vendor's table:
 *   CUST_屏/CL40BC267-40A_IPS_CTP/IC&Code/ST7796S_BOE3.98_IPS_Init .txt
 * A 0x00 (NOP) entry is a pure delay carrier. Differences vs the earlier TFT_eSPI table:
 *   0xB6(80 02 3B) -> 0xB7(C6)  [entry mode set], E1 gamma first byte E0->F0 and 2B->2D,
 *   and INVON (0x21) added before DISPON. MADCTL 0x48 (MX+BGR) is unchanged. */
static const stc_lcd_init_cmd_t m_astcST7796Init[] = {
    {LCD_CMD_SWRESET, 0U, {0U}, 120U},
    {LCD_CMD_SLPOUT,  0U, {0U}, 120U},
    {0xF0U, 1U, {0xC3U}, 0U},                                  /* enable extension command 2 part I   */
    {0xF0U, 1U, {0x96U}, 0U},                                  /* enable extension command 2 part II  */
    {LCD_CMD_MADCTL, 1U, {LCD_MADCTL_VERTICAL}, 0U},           /* 0x48: MX + BGR (portrait)           */
    {LCD_CMD_COLMOD, 1U, {0x55U}, 0U},                         /* RGB565, 16-bit/pixel                */
    {0xB4U, 1U, {0x01U}, 0U},                                  /* DIC: 1-dot inversion                */
    {0xB7U, 1U, {0xC6U}, 0U},                                  /* entry mode set (vendor table)       */
    {0xE8U, 8U, {0x40U, 0x8AU, 0x00U, 0x00U,
                  0x29U, 0x19U, 0xA5U, 0x33U}, 0U},             /* display output control adjust       */
    {0xC1U, 1U, {0x06U}, 0U},                                  /* power control 2                      */
    {0xC2U, 1U, {0xA7U}, 0U},                                  /* power control 3                      */
    {0xC5U, 1U, {0x18U}, 0U},                                  /* VCOM control                         */
    {LCD_CMD_NOP, 0U, {0U}, 120U},
    {0xE0U, 14U, {0xF0U, 0x09U, 0x0BU, 0x06U, 0x04U, 0x15U, 0x2FU,
                   0x54U, 0x42U, 0x3CU, 0x17U, 0x14U, 0x18U, 0x1BU}, 0U},  /* positive gamma */
    {0xE1U, 14U, {0xF0U, 0x09U, 0x0BU, 0x06U, 0x04U, 0x03U, 0x2DU,
                   0x43U, 0x42U, 0x3BU, 0x16U, 0x14U, 0x17U, 0x1BU}, 0U},  /* negative gamma */
    {LCD_CMD_NOP, 0U, {0U}, 120U},
    {0xF0U, 1U, {0x3CU}, 0U},                                  /* disable extension command 2 part I  */
    {0xF0U, 1U, {0x69U}, 0U},                                  /* disable extension command 2 part II */
    {LCD_CMD_NOP, 0U, {0U}, 120U},
    {LCD_CMD_INVON, 0U, {0U}, 120U},                           /* key: vendor INVON, correct colors   */
    {LCD_CMD_DISPON, 0U, {0U}, 0U},
};

/* ILI9488, parallel 16-bit branch (COLMOD=0x55). Source: TFT_eSPI ILI9488_Init.h. */
static const stc_lcd_init_cmd_t m_astcILI9488Init[] = {
    {LCD_CMD_SWRESET, 0U, {0U}, 120U},
    {0xE0U, 15U, {0x00U, 0x03U, 0x09U, 0x08U, 0x16U, 0x0AU, 0x3FU, 0x78U,
                   0x4CU, 0x09U, 0x0AU, 0x08U, 0x16U, 0x1AU, 0x0FU}, 0U},  /* positive gamma */
    {0xE1U, 15U, {0x00U, 0x16U, 0x19U, 0x03U, 0x0FU, 0x05U, 0x32U, 0x45U,
                   0x46U, 0x04U, 0x0EU, 0x0DU, 0x35U, 0x37U, 0x0FU}, 0U},  /* negative gamma */
    {0xC0U, 2U, {0x17U, 0x15U}, 0U},                           /* power control 1  */
    {0xC1U, 1U, {0x41U}, 0U},                                  /* power control 2  */
    {0xC5U, 3U, {0x00U, 0x12U, 0x80U}, 0U},                    /* VCOM control     */
    {LCD_CMD_MADCTL, 1U, {LCD_MADCTL_VERTICAL}, 0U},           /* MX + BGR (portrait) */
    {LCD_CMD_COLMOD, 1U, {0x55U}, 0U},                         /* RGB565 (parallel 16-bit) */
    {0xB0U, 1U, {0x00U}, 0U},                                  /* interface mode control */
    {0xB1U, 1U, {0xA0U}, 0U},                                  /* frame rate control    */
    {0xB4U, 1U, {0x02U}, 0U},                                  /* display inversion     */
    {0xB6U, 3U, {0x02U, 0x02U, 0x3BU}, 0U},                    /* display function      */
    {0xB7U, 1U, {0xC6U}, 0U},                                  /* entry mode set        */
    {0xF7U, 4U, {0xA9U, 0x51U, 0x2CU, 0x82U}, 0U},             /* adjust control 3      */
    {LCD_CMD_SLPOUT, 0U, {0U}, 120U},
    {LCD_CMD_DISPON, 0U, {0U}, 25U},
};

/* ------------------------------------------------------------------------------------------------
 * Local helpers
 * ---------------------------------------------------------------------------------------------- */
static void LCD_RunInitTable(const stc_lcd_init_cmd_t *pstcTable, uint32_t u32Count)
{
    uint32_t i;
    uint32_t j;

    for (i = 0UL; i < u32Count; i++) {
        BSP_LCD_WriteCmd(pstcTable[i].u8Cmd);
        for (j = 0UL; j < (uint32_t)pstcTable[i].u8Len; j++) {
            BSP_LCD_WriteByte(pstcTable[i].au8Data[j]);
        }
        if (0U != pstcTable[i].u16DelayMs) {
            DDL_DelayMS((uint32_t)pstcTable[i].u16DelayMs);
        }
    }
}

static void LCD_ST7796_Init(void)
{
    LCD_RunInitTable(m_astcST7796Init, sizeof(m_astcST7796Init) / sizeof(m_astcST7796Init[0]));
}

static void LCD_ILI9488_Init(void)
{
    LCD_RunInitTable(m_astcILI9488Init, sizeof(m_astcILI9488Init) / sizeof(m_astcILI9488Init[0]));
}

/**
 * @brief  Look for a consecutive byte pair in an ID readback buffer.
 * @note   Only referenced by the auto-detect path; compiled out when the panel is fixed.
 */
#if (LCD_PANEL_AUTO != 0U)
static uint8_t LCD_IdHasPair(const uint8_t *pu8Buf, uint32_t u32Len, uint8_t u8A, uint8_t u8B)
{
    uint32_t i;
    uint8_t u8Found = 0U;

    for (i = 0UL; (i + 1UL) < u32Len; i++) {
        if ((u8A == pu8Buf[i]) && (u8B == pu8Buf[i + 1UL])) {
            u8Found = 1U;
            break;
        }
    }
    return u8Found;
}
#endif

/* ------------------------------------------------------------------------------------------------
 * ID detection
 * ---------------------------------------------------------------------------------------------- */
/**
 * @brief  Read ID from RDDID (0x04) and ILI ID4 (0xD3), then classify the panel.
 * @note   On a 16-bit SMC bus the panel drives D[7:0] only, one byte per read.
 *         Known signatures:
 *           ILI9488 : 0x94 0x88 in the 0x04 or 0xD3 readback
 *           ST7796  : 0x85 as ID1 (Sitronix), or 0x85 0x85 0x52, or 0x77 0x96
 *         If /RD is not wired, all reads return 0x00/0xFF and the forced/default
 *         panel is used (g_u8PanelIdFallback is set for the debugger).
 * @param  None
 * @retval Normalized ID: LCD_ID_ST7796 / LCD_ID_ILI9488 / LCD_ID_INVALID
 */
uint16_t LCD_PANEL_ProbeId(void)
{
    uint8_t au8Id04[4U] = {0U, 0U, 0U, 0U};
    uint8_t au8IdD3[4U] = {0U, 0U, 0U, 0U};
    uint16_t u16Id = LCD_ID_INVALID;
    uint32_t i;

#if (LCD_PANEL_AUTO != 0U)
    /* ST7796 / ILI9488 remain in Sleep-In after a hardware reset and do not
     * answer RDDID reliably in that state. Exit sleep first and wait 120 ms
     * for the internal oscillator to settle before probing. The vendor init
     * table issues SLPOUT again afterwards, which is harmless. */
    BSP_LCD_WriteCmd(LCD_CMD_SLPOUT);
    DDL_DelayMS(120UL);

    BSP_LCD_ReadRegBytes(LCD_CMD_RDDID,    au8Id04, 4UL);
    BSP_LCD_ReadRegBytes(LCD_CMD_READ_ID4, au8IdD3, 4UL);

    for (i = 0UL; i < 4UL; i++) {
        g_au8IdRaw04[i] = au8Id04[i];
        g_au8IdRawD3[i] = au8IdD3[i];
    }

    /* ILI9488 has an unambiguous 0x9488 signature */
    if ((1U == LCD_IdHasPair(au8Id04, 4UL, 0x94U, 0x88U)) || \
        (1U == LCD_IdHasPair(au8IdD3, 4UL, 0x94U, 0x88U))) {
        u16Id = LCD_ID_ILI9488;
    } else if ((0x85U == au8Id04[0]) || (0x85U == au8Id04[1]) || \
               (0x85U == au8Id04[2]) || (0x52U == au8Id04[3]) || \
               (0x85U == au8IdD3[1]) || (0x52U == au8IdD3[3]) || \
               (1U == LCD_IdHasPair(au8Id04, 4UL, 0x77U, 0x96U)) || \
               (1U == LCD_IdHasPair(au8IdD3, 4UL, 0x77U, 0x96U))) {
        /* Sitronix ST7796S ID1 = 0x85; ST7789-family ID3 = 0x52;
         * some clones report 0x77 0x96 instead. */
        u16Id = LCD_ID_ST7796;
    } else {
        u16Id = LCD_ID_INVALID;
    }
#else
    (void)au8Id04;
    (void)au8IdD3;
    (void)i;
#endif

#if (LCD_PANEL_FORCE_ST7796 != 0U)
    u16Id = LCD_ID_ST7796;
#elif (LCD_PANEL_FORCE_ILI9488 != 0U)
    u16Id = LCD_ID_ILI9488;
#endif

    return u16Id;
}

/**
 * @brief  Select a panel, run its init table and set the default orientation.
 * @param  None
 * @retval None
 */
void LCD_PANEL_Init(void)
{
    uint16_t u16Id;
    uint32_t i;

    g_u8PanelIdFallback = 0U;

#if (LCD_DEBUG_PRINT_RAW_ID != 0U)
    /* Real bus health check. The "LCD panel: 0x7796" line is only the forced
     * driver selection; these bytes actually come from the panel over i8080.
     * All 0x00 / 0xFF -> /RST held low, /RD or data lines not connected. */
    BSP_LCD_WriteCmd(LCD_CMD_SLPOUT);
    DDL_DelayMS(120UL);
    BSP_LCD_ReadRegBytes(LCD_CMD_RDDID,    g_au8IdRaw04, 4UL);
    BSP_LCD_ReadRegBytes(LCD_CMD_READ_ID4, g_au8IdRawD3, 4UL);
    DDL_Printf("Panel raw ID: 04=%02X %02X %02X %02X, D3=%02X %02X %02X %02X\r\n",
               (unsigned int)g_au8IdRaw04[0], (unsigned int)g_au8IdRaw04[1],
               (unsigned int)g_au8IdRaw04[2], (unsigned int)g_au8IdRaw04[3],
               (unsigned int)g_au8IdRawD3[0], (unsigned int)g_au8IdRawD3[1],
               (unsigned int)g_au8IdRawD3[2], (unsigned int)g_au8IdRawD3[3]);
#endif

    u16Id = LCD_PANEL_ProbeId();

    if (LCD_ID_INVALID == u16Id) {
        /* No /RD wire, slow read timing or an unknown clone: use the safest default.
         * Set LCD_PANEL_FORCE_ILI9488 in lcd_config.h if your module is ILI9488. */
        u16Id = LCD_ID_ST7796;
        g_u8PanelIdFallback = 1U;
    }

    m_pstcPanel = &m_astcPanel[0];
    for (i = 0UL; i < (sizeof(m_astcPanel) / sizeof(m_astcPanel[0])); i++) {
        if (m_astcPanel[i].u16Id == u16Id) {
            m_pstcPanel = &m_astcPanel[i];
            break;
        }
    }

    g_u16PanelId = u16Id;
    m_pstcPanel->Init();

    /* Re-apply MADCTL and a full-screen window after the vendor table,
     * so CASET/RASET are always explicitly set (fix for the official driver). */
    BSP_LCD_SetDisplayDir(LCD_DEFAULT_ORIENTATION);
    BSP_LCD_SetCursor(0U, 0U);
    BSP_LCD_PrepareWriteRAM();

    DDL_Printf("LCD panel: 0x%04X, fallback=%u, %ux%u\r\n",
               (unsigned int)u16Id, (unsigned int)g_u8PanelIdFallback,
               (unsigned int)m_u16Width, (unsigned int)m_u16Height);
}

/* ------------------------------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------------------------- */
/**
 * @brief  Initialize SMC, panel and drawing state.
 */
void BSP_LCD_Init(void)
{
    int32_t i32Ret = BSP_LCD_SMC_Init();

    if (LL_OK != i32Ret) {
        /* The SMC never left low-power or its registers did not update.
         * Continuing would write to an unmapped 0x6000xxxx window and then
         * hang in HardFault with no clue. Report on serial AND blink the
         * backlight slowly (~1 s) so the failure is visible without a console. */
        DDL_Printf("EXMC SMC init failed (%d), halted\r\n", (int)i32Ret);
        for (;;) {
            BSP_LCD_BKLCmd(LCD_PIN_SET);
            DDL_DelayMS(500UL);
            BSP_LCD_BKLCmd(LCD_PIN_RESET);
            DDL_DelayMS(500UL);
        }
    }
    LCD_PANEL_Init();
}

uint16_t BSP_LCD_ReadID(void)
{
    return g_u16PanelId;
}

void BSP_LCD_DisplayOn(void)
{
    BSP_LCD_WriteCmd(LCD_CMD_DISPON);
}

void BSP_LCD_DisplayOff(void)
{
    BSP_LCD_WriteCmd(LCD_CMD_DISPOFF);
}

uint16_t BSP_LCD_GetPixelWidth(void)
{
    return m_u16Width;
}

uint16_t BSP_LCD_GetPixelHeight(void)
{
    return m_u16Height;
}

/**
 * @brief  Set CASET (column) and RASET (row) window explicitly.
 * @note   This is the fix for the official driver, which wrote 0x2B twice.
 */
void BSP_LCD_SetWindow(uint16_t u16X1, uint16_t u16Y1, uint16_t u16X2, uint16_t u16Y2)
{
    BSP_LCD_WriteCmd(LCD_CMD_CASET);
    BSP_LCD_WriteByte((uint8_t)(u16X1 >> 8U));
    BSP_LCD_WriteByte((uint8_t)(u16X1 & 0xFFU));
    BSP_LCD_WriteByte((uint8_t)(u16X2 >> 8U));
    BSP_LCD_WriteByte((uint8_t)(u16X2 & 0xFFU));

    BSP_LCD_WriteCmd(LCD_CMD_RASET);
    BSP_LCD_WriteByte((uint8_t)(u16Y1 >> 8U));
    BSP_LCD_WriteByte((uint8_t)(u16Y1 & 0xFFU));
    BSP_LCD_WriteByte((uint8_t)(u16Y2 >> 8U));
    BSP_LCD_WriteByte((uint8_t)(u16Y2 & 0xFFU));
}

/**
 * @brief  Set the RAM write start point; the window ends at the screen edge.
 */
void BSP_LCD_SetCursor(uint16_t u16Xpos, uint16_t u16Ypos)
{
    if ((u16Xpos < m_u16Width) && (u16Ypos < m_u16Height)) {
        BSP_LCD_SetWindow(u16Xpos, u16Ypos, m_u16Width - 1U, m_u16Height - 1U);
    }
}

void BSP_LCD_PrepareWriteRAM(void)
{
    BSP_LCD_WriteCmd(LCD_CMD_RAMWR);
}

/**
 * @brief  Set display orientation: vertical 320x480, horizontal 480x320.
 */
void BSP_LCD_SetDisplayDir(uint16_t u16Dir)
{
    if (LCD_DISPLAY_HORIZONTAL == u16Dir) {
        m_u16Width  = m_pstcPanel->u16Height;
        m_u16Height = m_pstcPanel->u16Width;
        BSP_LCD_WriteRegData(LCD_CMD_MADCTL, LCD_MADCTL_HORIZONTAL);
        m_u16Dir = LCD_DISPLAY_HORIZONTAL;
    } else {
        m_u16Width  = m_pstcPanel->u16Width;
        m_u16Height = m_pstcPanel->u16Height;
        BSP_LCD_WriteRegData(LCD_CMD_MADCTL, LCD_MADCTL_VERTICAL);
        m_u16Dir = LCD_DISPLAY_VERTICAL;
    }
    BSP_LCD_SetWindow(0U, 0U, m_u16Width - 1U, m_u16Height - 1U);
}

/**
 * @brief  Set the MADCTL scan direction (advanced use).
 * @note   Width/height are NOT swapped here; use BSP_LCD_SetDisplayDir() for rotation.
 */
void BSP_LCD_SetScanDir(uint16_t u16Dir)
{
    uint8_t u8RegVal = LCD_MADCTL_BGR;

    switch (u16Dir) {
        case LCD_SCAN_DIR_L2R_D2U:
            u8RegVal |= LCD_MADCTL_MY;
            break;
        case LCD_SCAN_DIR_R2L_U2D:
            u8RegVal |= LCD_MADCTL_MX;
            break;
        case LCD_SCAN_DIR_R2L_D2U:
            u8RegVal |= (LCD_MADCTL_MY | LCD_MADCTL_MX);
            break;
        case LCD_SCAN_DIR_U2D_L2R:
            u8RegVal |= LCD_MADCTL_MV;
            break;
        case LCD_SCAN_DIR_U2D_R2L:
            u8RegVal |= (LCD_MADCTL_MX | LCD_MADCTL_MV);
            break;
        case LCD_SCAN_DIR_D2U_L2R:
            u8RegVal |= (LCD_MADCTL_MY | LCD_MADCTL_MV);
            break;
        case LCD_SCAN_DIR_D2U_R2L:
            u8RegVal |= (LCD_MADCTL_MY | LCD_MADCTL_MX | LCD_MADCTL_MV);
            break;
        case LCD_SCAN_DIR_L2R_U2D:
        default:
            break;
    }

    if (LCD_DISPLAY_HORIZONTAL == m_u16Dir) {
        u8RegVal ^= LCD_MADCTL_MV;   /* keep the current rotation */
    }

    BSP_LCD_WriteRegData(LCD_CMD_MADCTL, u8RegVal);
    BSP_LCD_SetWindow(0U, 0U, m_u16Width - 1U, m_u16Height - 1U);
}

void BSP_LCD_WritePixel(uint16_t u16Xpos, uint16_t u16Ypos, uint16_t u16RGBCode)
{
    BSP_LCD_SetCursor(u16Xpos, u16Ypos);
    BSP_LCD_PrepareWriteRAM();
    BSP_LCD_WriteColor(u16RGBCode);
}

void BSP_LCD_Clear(uint16_t u16RGBCode)
{
    uint32_t i;
    uint32_t u32TotalPoint;

    BSP_LCD_SetWindow(0U, 0U, m_u16Width - 1U, m_u16Height - 1U);
    BSP_LCD_PrepareWriteRAM();

    u32TotalPoint = (uint32_t)m_u16Width * (uint32_t)m_u16Height;
    for (i = 0UL; i < u32TotalPoint; i++) {
        BSP_LCD_WriteColor(u16RGBCode);
    }
}

/* ------------------------------------------------------------------------------------------------
 * Drawing algorithms (ported from XHSC DDL components/nt35510/nt35510.c)
 * ---------------------------------------------------------------------------------------------- */
void BSP_LCD_DrawLine(uint16_t u16X1, uint16_t u16Y1,
                      uint16_t u16X2, uint16_t u16Y2, uint16_t u16RGBCode)
{
    int16_t t;
    int16_t xerr = 0;
    int16_t yerr = 0;
    int16_t delta_x;
    int16_t delta_y;
    int16_t distance;
    int16_t incx;
    int16_t incy;
    int16_t Row;
    int16_t Col;

    Row = (int16_t)u16X1;
    Col = (int16_t)u16Y1;
    delta_x = ((int16_t)u16X2 - (int16_t)u16X1);
    delta_y = ((int16_t)u16Y2 - (int16_t)u16Y1);

    if (delta_x > 0) {
        incx = 1;
    } else if (delta_x == 0) {
        incx = 0;
    } else {
        incx = -1;
        delta_x = -delta_x;
    }

    if (delta_y > 0) {
        incy = 1;
    } else if (delta_y == 0) {
        incy = 0;
    } else {
        incy = -1;
        delta_y = -delta_y;
    }

    distance = (delta_x > delta_y) ? delta_x : delta_y;

    for (t = 0; t <= (distance + 1); t++) {
        BSP_LCD_WritePixel((uint16_t)Row, (uint16_t)Col, u16RGBCode);
        xerr += delta_x;
        yerr += delta_y;
        if (xerr > distance) {
            xerr -= distance;
            Row += incx;
        }
        if (yerr > distance) {
            yerr -= distance;
            Col += incy;
        }
    }
}

void BSP_LCD_DrawCircle(uint16_t u16Xpos, uint16_t u16Ypos,
                        uint16_t u16Radius, uint16_t u16RGBCode)
{
    int32_t decision;
    uint32_t current_x;
    uint32_t current_y;

    decision = 3 - ((int32_t)u16Radius * 2);
    current_x = 0U;
    current_y = u16Radius;

    while (current_x <= current_y) {
        BSP_LCD_WritePixel((uint16_t)(u16Xpos + current_x), (uint16_t)(u16Ypos - current_y), u16RGBCode);
        BSP_LCD_WritePixel((uint16_t)(u16Xpos + current_y), (uint16_t)(u16Ypos - current_x), u16RGBCode);
        BSP_LCD_WritePixel((uint16_t)(u16Xpos + current_y), (uint16_t)(u16Ypos + current_x), u16RGBCode);
        BSP_LCD_WritePixel((uint16_t)(u16Xpos + current_x), (uint16_t)(u16Ypos + current_y), u16RGBCode);
        BSP_LCD_WritePixel((uint16_t)(u16Xpos - current_x), (uint16_t)(u16Ypos + current_y), u16RGBCode);
        BSP_LCD_WritePixel((uint16_t)(u16Xpos - current_y), (uint16_t)(u16Ypos + current_x), u16RGBCode);
        BSP_LCD_WritePixel((uint16_t)(u16Xpos - current_x), (uint16_t)(u16Ypos - current_y), u16RGBCode);
        BSP_LCD_WritePixel((uint16_t)(u16Xpos - current_y), (uint16_t)(u16Ypos - current_x), u16RGBCode);
        current_x++;
        if (decision < 0) {
            decision += ((4 * (int32_t)current_x) + 6);
        } else {
            decision += (10 + (4 * ((int32_t)current_x - (int32_t)current_y)));
            current_y--;
        }
    }
}

void BSP_LCD_FillTriangle(uint16_t u16X1, uint16_t u16Y1,
                          uint16_t u16X2, uint16_t u16Y2,
                          uint16_t u16X3, uint16_t u16Y3, uint16_t u16RGBCode)
{
    uint16_t deltax;
    uint16_t deltay;
    int16_t xoff;
    int16_t yoff;
    int16_t xinc1;
    int16_t xinc2;
    int16_t yinc1;
    int16_t yinc2;
    uint16_t den;
    uint16_t num;
    uint16_t numadd;
    uint16_t numpixels;
    uint16_t curpixel;

    xoff = (int16_t)u16X1;
    yoff = (int16_t)u16Y1;
    deltax = (u16X2 > u16X1) ? (u16X2 - u16X1) : (u16X1 - u16X2);
    deltay = (u16Y2 > u16Y1) ? (u16Y2 - u16Y1) : (u16Y1 - u16Y2);

    if (u16X2 >= u16X1) {
        xinc1 = 1;
        xinc2 = 1;
    } else {
        xinc1 = -1;
        xinc2 = -1;
    }
    if (u16Y2 >= u16Y1) {
        yinc1 = 1;
        yinc2 = 1;
    } else {
        yinc1 = -1;
        yinc2 = -1;
    }

    if (deltax >= deltay) {
        xinc1 = 0;
        yinc2 = 0;
        den = deltax;
        num = (deltax / 2U);
        numadd = deltay;
        numpixels = deltax;
    } else {
        xinc2 = 0;
        yinc1 = 0;
        den = deltay;
        num = (deltay / 2U);
        numadd = deltax;
        numpixels = deltay;
    }

    for (curpixel = 0U; curpixel <= numpixels; curpixel++) {
        BSP_LCD_DrawLine((uint16_t)xoff, (uint16_t)yoff, u16X3, u16Y3, u16RGBCode);
        num += numadd;
        if (num >= den) {
            num -= den;
            xoff += xinc1;
            yoff += yinc1;
        }
        xoff += xinc2;
        yoff += yinc2;
    }
}

void BSP_LCD_DrawRectangle(uint16_t u16X1, uint16_t u16Y1,
                           uint16_t u16X2, uint16_t u16Y2, uint16_t u16RGBCode)
{
    BSP_LCD_DrawLine(u16X1, u16Y1, u16X2, u16Y1, u16RGBCode);
    BSP_LCD_DrawLine(u16X1, u16Y1, u16X1, u16Y2, u16RGBCode);
    BSP_LCD_DrawLine(u16X1, u16Y2, u16X2, u16Y2, u16RGBCode);
    BSP_LCD_DrawLine(u16X2, u16Y1, u16X2, u16Y2, u16RGBCode);
}
