/**
 *******************************************************************************
 * @file  lcd_panel.h
 * @brief ST7796 / ILI9488 panel driver and RGB565 drawing API.
 * @note  Public API keeps the same shape as the official BSP_NT35510_* functions,
 *        so the application layer is a near 1:1 port of exmc_smc_lcd_nt35510.
 *******************************************************************************
 * Copyright (C) 2022-2025, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 * This software component is licensed by XHSC under BSD 3-Clause license.
 *******************************************************************************
 */
#ifndef __LCD_PANEL_H__
#define __LCD_PANEL_H__

#include "hc32_ll.h"
#include "lcd_config.h"
#include "bsp_lcd.h"

/* RGB565 colors (same definitions as the official nt35510.h) */
#define LCD_COLOR_WHITE                             (0xFFFFU)
#define LCD_COLOR_BLACK                             (0x0000U)
#define LCD_COLOR_BLUE                              (0x001FU)
#define LCD_COLOR_BRED                              (0xF81FU)
#define LCD_COLOR_GRED                              (0xFFE0U)
#define LCD_COLOR_GBLUE                             (0x07FFU)
#define LCD_COLOR_RED                               (0xF800U)
#define LCD_COLOR_MAGENTA                           (0xF81FU)
#define LCD_COLOR_GREEN                             (0x07E0U)
#define LCD_COLOR_ORANGE                            (0xFD20U)   /* R31 G20 B0 - orange */
#define LCD_COLOR_CYAN                              (0x7FFFU)
#define LCD_COLOR_YELLOW                            (0xFFE0U)
#define LCD_COLOR_BROWN                             (0xBC40U)
#define LCD_COLOR_GRAY                              (0x8430U)
#define LCD_COLOR_DARKBLUE                          (0x01CFU)
#define LCD_COLOR_LIGHTBLUE                         (0x7D7CU)
#define LCD_COLOR_GRAYBLUE                          (0x5458U)
#define LCD_COLOR_LIGHTGREEN                        (0x841FU)
#define LCD_COLOR_LIGHTGRAY                         (0xEF5BU)
#define LCD_COLOR_LGRAY                             (0xC618U)

/* Display orientation */
#define LCD_DISPLAY_VERTICAL                        (0U)
#define LCD_DISPLAY_HORIZONTAL                      (1U)

/* Scan directions (MADCTL MY/MX/MV bits) */
#define LCD_SCAN_DIR_L2R_U2D                        (0U)
#define LCD_SCAN_DIR_L2R_D2U                        (1U)
#define LCD_SCAN_DIR_R2L_U2D                        (2U)
#define LCD_SCAN_DIR_R2L_D2U                        (3U)
#define LCD_SCAN_DIR_U2D_L2R                        (4U)
#define LCD_SCAN_DIR_U2D_R2L                        (5U)
#define LCD_SCAN_DIR_D2U_L2R                        (6U)
#define LCD_SCAN_DIR_D2U_R2L                        (7U)

/* Normalized panel IDs (not the raw ID bytes) */
#define LCD_ID_INVALID                              (0x0000U)
#define LCD_ID_ST7796                               (0x7796U)
#define LCD_ID_ILI9488                              (0x9488U)

/* Debug observables: watch these in the debugger when the screen stays white. */
extern uint16_t g_u16PanelId;
extern uint8_t  g_u8PanelIdFallback;
extern uint8_t  g_au8IdRaw04[4];
extern uint8_t  g_au8IdRawD3[4];

/* Panel layer */
uint16_t LCD_PANEL_ProbeId(void);
void     LCD_PANEL_Init(void);

/* Public API (compatible with the official example) */
void     BSP_LCD_Init(void);
uint16_t BSP_LCD_ReadID(void);
void     BSP_LCD_DisplayOn(void);
void     BSP_LCD_DisplayOff(void);
uint16_t BSP_LCD_GetPixelWidth(void);
uint16_t BSP_LCD_GetPixelHeight(void);
void     BSP_LCD_SetScanDir(uint16_t u16Dir);
void     BSP_LCD_SetDisplayDir(uint16_t u16Dir);
void     BSP_LCD_SetWindow(uint16_t u16X1, uint16_t u16Y1,
                           uint16_t u16X2, uint16_t u16Y2);
void     BSP_LCD_SetCursor(uint16_t u16Xpos, uint16_t u16Ypos);
void     BSP_LCD_PrepareWriteRAM(void);
void     BSP_LCD_WritePixel(uint16_t u16Xpos, uint16_t u16Ypos, uint16_t u16RGBCode);
void     BSP_LCD_DrawLine(uint16_t u16X1, uint16_t u16Y1,
                          uint16_t u16X2, uint16_t u16Y2, uint16_t u16RGBCode);
void     BSP_LCD_DrawCircle(uint16_t u16Xpos, uint16_t u16Ypos,
                            uint16_t u16Radius, uint16_t u16RGBCode);
void     BSP_LCD_FillTriangle(uint16_t u16X1, uint16_t u16Y1,
                              uint16_t u16X2, uint16_t u16Y2,
                              uint16_t u16X3, uint16_t u16Y3, uint16_t u16RGBCode);
void     BSP_LCD_DrawRectangle(uint16_t u16X1, uint16_t u16Y1,
                               uint16_t u16X2, uint16_t u16Y2, uint16_t u16RGBCode);
void     BSP_LCD_Clear(uint16_t u16RGBCode);

#endif /* __LCD_PANEL_H__ */
