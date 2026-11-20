/**
 *******************************************************************************
 * @file  bsp_lcd.h
 * @brief EXMC-SMC i8080 low-level driver for the 4.0" LCD module.
 * @note  Register/GRAM access is memory mapped:
 *          0x6000_1FFE -> command (RS=0), 0x6000_2000 -> data (RS=1).
 *        See RM_HC32F4A0 Rev1.50 chapter 40 and lcd_config.h.
 *******************************************************************************
 * Copyright (C) 2022-2025, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 * This software component is licensed by XHSC under BSD 3-Clause license.
 *******************************************************************************
 */
#ifndef __BSP_LCD_H__
#define __BSP_LCD_H__

#include "hc32_ll.h"
#include "lcd_config.h"

/* LCD controller register view (16-bit memory width) */
typedef struct {
    volatile uint16_t u16REG;
    volatile uint16_t u16RAM;
} stc_lcd_controller_t;

/* Active levels of the panel control signals */
#define LCD_PIN_RESET                               (0U)
#define LCD_PIN_SET                                 (1U)

int32_t  BSP_LCD_SMC_Init(void);
void     BSP_LCD_IO_Init(void);
void     BSP_LCD_RSTCmd(uint8_t u8Cmd);
void     BSP_LCD_BKLCmd(uint8_t u8Cmd);

/* 8080 primitives. Commands and parameters are 8-bit; RGB565 pixels are 16-bit. */
void     BSP_LCD_WriteCmd(uint8_t u8Cmd);
void     BSP_LCD_WriteByte(uint8_t u8Data);
void     BSP_LCD_WriteColor(uint16_t u16RGB565);
uint8_t  BSP_LCD_ReadByte(void);
void     BSP_LCD_WriteRegData(uint8_t u8Reg, uint8_t u8Data);
void     BSP_LCD_ReadRegBytes(uint8_t u8Reg, uint8_t *pu8Buf, uint32_t u32Len);

#endif /* __BSP_LCD_H__ */
