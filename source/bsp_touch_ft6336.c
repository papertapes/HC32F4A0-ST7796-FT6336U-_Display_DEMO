/**
 *******************************************************************************
 * @file  bsp_touch_ft6336.c
 * @brief FT6336U (FocalTech FT6x36 family) I2C touch driver.
 *
 * @note  Register map (public FT6x36 / FT6336U documentation):
 *          0x00 DEV_MODE, 0x01 GEST_ID, 0x02 TD_STATUS (low nibble = touch count)
 *          0x03 P1_XH (bit3..0 = X[11:8]), 0x04 P1_XL (X[7:0])
 *          0x05 P1_YH (bit3..0 = Y[11:8]), 0x06 P1_YL (Y[7:0])
 *          0xA4 ID_G_MODE
 *        7-bit I2C address = 0x38 (see lcd_config.h).
 *        INT is optional: TOUCH_USE_INT = 0 polls TD_STATUS in the main loop.
 *******************************************************************************
 * Copyright (C) 2022-2025, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 * This software component is licensed by XHSC under BSD 3-Clause license.
 *******************************************************************************
 */
#include <stdio.h>
#include "bsp_touch_ft6336.h"
#include "bsp_board.h"
#include "bsp_lcd.h"
#include "lcd_panel.h"

static uint8_t m_u8TouchPresent = 0U;

/**
 * @brief  Read one or more FT6336U registers.
 */
int32_t TOUCH_ReadReg(uint8_t u8Reg, uint8_t *pu8Buf, uint32_t u32Len)
{
    return BSP_I2C_Read(TOUCH_I2C_UNIT, TOUCH_I2C_ADDR, &u8Reg, 1U, pu8Buf, u32Len);
}

/**
 * @brief  Write one FT6336U register.
 */
int32_t TOUCH_WriteReg(uint8_t u8Reg, uint8_t u8Data)
{
    return BSP_I2C_Write(TOUCH_I2C_UNIT, TOUCH_I2C_ADDR, &u8Reg, 1U, &u8Data, 1UL);
}

/**
 * @brief  Read the chip/device mode byte (0xA4); 0xFF usually means no ACK.
 */
uint8_t TOUCH_ReadChipId(void)
{
    uint8_t u8Id = 0xFFU;

    if (LL_OK != TOUCH_ReadReg(FT6336U_REG_ID_G_MODE, &u8Id, 1UL)) {
        u8Id = 0xFFU;
    }
    return u8Id;
}

/**
 * @brief  Initialize the FT6336U: reset pulse, I2C pins/unit, presence check.
 * @param  None
 * @retval None
 */
void TOUCH_Init(void)
{
    stc_gpio_init_t stcGpioInit;
    uint8_t u8Id;

    /* /RST as output. Drive low >= 5 ms, then high and wait >= 50 ms. */
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDir = PIN_DIR_OUT;
    (void)GPIO_Init(TOUCH_RST_PORT, TOUCH_RST_PIN, &stcGpioInit);
    GPIO_ResetPins(TOUCH_RST_PORT, TOUCH_RST_PIN);
    DDL_DelayMS(10UL);
    GPIO_SetPins(TOUCH_RST_PORT, TOUCH_RST_PIN);
    DDL_DelayMS(50UL);

#if (TOUCH_USE_INT != 0U)
    /* INT is an input from the touch IC; polling mode does not need it. */
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDir = PIN_DIR_IN;
    (void)GPIO_Init(TOUCH_INT_PORT, TOUCH_INT_PIN, &stcGpioInit);
#endif

    /* I2C1: PD3 = SCL (func 49), PF10 = SDA (func 48) */
    (void)GPIO_StructInit(&stcGpioInit);
    (void)GPIO_Init(TOUCH_I2C_SCL_PORT, TOUCH_I2C_SCL_PIN, &stcGpioInit);
    (void)GPIO_Init(TOUCH_I2C_SDA_PORT, TOUCH_I2C_SDA_PIN, &stcGpioInit);
    GPIO_SetFunc(TOUCH_I2C_SCL_PORT, TOUCH_I2C_SCL_PIN, TOUCH_I2C_SCL_FUNC);
    GPIO_SetFunc(TOUCH_I2C_SDA_PORT, TOUCH_I2C_SDA_PIN, TOUCH_I2C_SDA_FUNC);

    FCG_Fcg1PeriphClockCmd(TOUCH_I2C_FCG, ENABLE);
    (void)BSP_I2C_Init(TOUCH_I2C_UNIT);

    m_u8TouchPresent = 0U;
    if (LL_OK == BSP_I2C_GetDevStatus(TOUCH_I2C_UNIT, TOUCH_I2C_ADDR)) {
        m_u8TouchPresent = 1U;
        u8Id = TOUCH_ReadChipId();
        DDL_Printf("FT6336U found @0x%02X, ID_G_MODE=0x%02X\r\n",
                   (unsigned int)TOUCH_I2C_ADDR, (unsigned int)u8Id);
    } else {
        DDL_Printf("FT6336U not found @0x%02X\r\n", (unsigned int)TOUCH_I2C_ADDR);
    }
}

/**
 * @brief  Return 1 when at least one touch point is reported.
 */
uint8_t TOUCH_IsPressed(void)
{
    uint8_t u8Status = 0U;

    if (0U == m_u8TouchPresent) {
        return 0U;
    }
    if (LL_OK != TOUCH_ReadReg(FT6336U_REG_TD_STATUS, &u8Status, 1UL)) {
        return 0U;
    }
    return ((u8Status & FT6336U_TD_STATUS_MASK) > 0U) ? 1U : 0U;
}

/**
 * @brief  Map one raw 12-bit coordinate pair to LCD pixels.
 */
static uint8_t TOUCH_MapRaw(uint16_t u16RawX, uint16_t u16RawY,
                            uint16_t *pu16X, uint16_t *pu16Y)
{
    uint16_t u16LcdW;
    uint16_t u16LcdH;
    uint32_t u32Tmp;

#if (TOUCH_SWAP_XY != 0U)
    u32Tmp = u16RawX;
    u16RawX = u16RawY;
    u16RawY = (uint16_t)u32Tmp;
#endif

#if (TOUCH_REJECT_OUT_OF_RANGE != 0U)
    /* Idle / invalid frames report a full-scale value; the linear map below
     * would clamp it to the panel corner (which lies inside the button bar).
     * Treat it as "no touch" instead of inventing a phantom point there. */
    if ((u16RawX >= TOUCH_RAW_X_MAX) || (u16RawY >= TOUCH_RAW_Y_MAX)) {
        return 0U;
    }
#endif

    u16LcdW = BSP_LCD_GetPixelWidth();
    u16LcdH = BSP_LCD_GetPixelHeight();

    /* Linear map with clamping (32-bit intermediate avoids 16-bit overflow). */
    u32Tmp = ((uint32_t)u16RawX * (uint32_t)u16LcdW) / (uint32_t)TOUCH_RAW_X_MAX;
    if (u32Tmp >= (uint32_t)u16LcdW) {
        u32Tmp = (uint32_t)u16LcdW - 1UL;
    }
    *pu16X = (uint16_t)u32Tmp;

    u32Tmp = ((uint32_t)u16RawY * (uint32_t)u16LcdH) / (uint32_t)TOUCH_RAW_Y_MAX;
    if (u32Tmp >= (uint32_t)u16LcdH) {
        u32Tmp = (uint32_t)u16LcdH - 1UL;
    }
    *pu16Y = (uint16_t)u32Tmp;

#if (TOUCH_INVERT_X != 0U)
    *pu16X = (uint16_t)(u16LcdW - 1UL - *pu16X);
#endif
#if (TOUCH_INVERT_Y != 0U)
    *pu16Y = (uint16_t)(u16LcdH - 1UL - *pu16Y);
#endif

    return 1U;
}

/**
 * @brief  Read BOTH reported touch points in one frame, keeping their event flags.
 * @param  [out] pstcPoints  decoded points (LCD coordinates)
 * @param  u8Max             capacity of pstcPoints (1 or 2)
 * @retval number of valid points (0..u8Max), 0 = no touch / I2C error
 * @note   One burst read from TD_STATUS (0x02) through P2_WEIGHT (0x0D).
 *         Register layout per point (5 bytes, starting at 0x03 + 6*i):
 *           XH[7:6] = event (00 put down / 01 put up / 10 contact), XH[3:0] = X[11:8]
 *           XL      = X[7:0], YH[3:0] = Y[11:8], YL = Y[7:0], WEIGHT = pressure
 *         Two separate reads would mix coordinates from different scan frames.
 */
uint8_t TOUCH_GetPoints(stc_touch_point_t *pstcPoints, uint8_t u8Max)
{
    uint8_t au8Buf[12U] = {0U};
    uint8_t u8Count;
    uint8_t u8N;
    uint8_t u8Out = 0U;
    uint8_t i;
    uint8_t u8Base;
    uint16_t u16RawX;
    uint16_t u16RawY;

    if ((NULL == pstcPoints) || (0U == u8Max) || (0U == m_u8TouchPresent)) {
        return 0U;
    }

    if (LL_OK != TOUCH_ReadReg(FT6336U_REG_TD_STATUS, au8Buf, (uint32_t)sizeof(au8Buf))) {
        return 0U;
    }

    u8Count = (uint8_t)(au8Buf[0] & FT6336U_TD_STATUS_MASK);
#if (TOUCH_REJECT_MULTI != 0U)
    if (1U != u8Count) {
        return 0U;
    }
#endif
    if (0U == u8Count) {
        return 0U;
    }

    u8N = (u8Count < u8Max) ? u8Count : u8Max;
    if (u8N > 2U) {
        u8N = 2U;   /* the burst only covers P1 and P2 */
    }

    for (i = 0U; i < u8N; i++) {
        u8Base = (uint8_t)(1U + (6U * i));   /* 1 = offset of 0x03 from 0x02 */
        u16RawX = ((uint16_t)(au8Buf[u8Base] & FT6336U_P1_XH_MASK) << 8U) | \
                  (uint16_t)au8Buf[u8Base + 1U];
        u16RawY = ((uint16_t)(au8Buf[u8Base + 2U] & FT6336U_P1_XH_MASK) << 8U) | \
                  (uint16_t)au8Buf[u8Base + 3U];

        if (0U == TOUCH_MapRaw(u16RawX, u16RawY,
                               &pstcPoints[u8Out].u16X, &pstcPoints[u8Out].u16Y)) {
            continue;   /* out of raw range: not a real touch */
        }
        pstcPoints[u8Out].u8Event  = (uint8_t)((au8Buf[u8Base] >> 6U) & 0x03U);
        pstcPoints[u8Out].u8Weight = au8Buf[u8Base + 4U];
        u8Out++;
    }

    return u8Out;
}

/**
 * @brief  Legacy wrapper: first reported point, mapped to LCD pixels.
 */
uint8_t TOUCH_GetXY(uint16_t *pu16X, uint16_t *pu16Y)
{
    stc_touch_point_t astcPt[2U];

    if (0U == TOUCH_GetPoints(astcPt, 2U)) {
        return 0U;
    }
    if ((NULL != pu16X) && (NULL != pu16Y)) {
        *pu16X = astcPt[0].u16X;
        *pu16Y = astcPt[0].u16Y;
    }
    return 1U;
}
