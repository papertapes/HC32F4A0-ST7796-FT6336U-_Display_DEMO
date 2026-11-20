/**
 *******************************************************************************
 * @file  bsp_touch_ft6336.h
 * @brief FT6336U capacitive touch controller (I2C) for the 4.0" LCD module.
 *******************************************************************************
 * Copyright (C) 2022-2025, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 * This software component is licensed by XHSC under BSD 3-Clause license.
 *******************************************************************************
 */
#ifndef __BSP_TOUCH_FT6336_H__
#define __BSP_TOUCH_FT6336_H__

#include "hc32_ll.h"
#include "lcd_config.h"

/* FT6x36 event flag (Pn_XH bits 7:6, see FT6X36 register map 2.1.4) */
#define TOUCH_EVT_PUT_DOWN                          (0U)
#define TOUCH_EVT_PUT_UP                            (1U)
#define TOUCH_EVT_CONTACT                           (2U)

/* One decoded touch point. Coordinates are already mapped to LCD pixels. */
typedef struct {
    uint16_t u16X;
    uint16_t u16Y;
    uint8_t  u8Event;       /* TOUCH_EVT_* */
    uint8_t  u8Weight;      /* raw Pn_WEIGHT, 0 = unusable */
} stc_touch_point_t;

void     TOUCH_Init(void);
uint8_t  TOUCH_IsPressed(void);
uint8_t  TOUCH_GetPoints(stc_touch_point_t *pstcPoints, uint8_t u8Max);
uint8_t  TOUCH_GetXY(uint16_t *pu16X, uint16_t *pu16Y);   /* legacy: first point */
uint8_t  TOUCH_ReadChipId(void);
int32_t  TOUCH_ReadReg(uint8_t u8Reg, uint8_t *pu8Buf, uint32_t u32Len);
int32_t  TOUCH_WriteReg(uint8_t u8Reg, uint8_t u8Data);

#endif /* __BSP_TOUCH_FT6336_H__ */
