/**
 *******************************************************************************
 * @file  bsp_lcd.c
 * @brief EXMC-SMC port/timing setup and 8080 primitives for the 4.0" LCD.
 * @note  Port init and SMC timing are taken from the official
 *        ev_hc32f4a0_lqfp176_nt35510.c (DDL Rev2.4.0). Only the panel control
 *        signals (RST / backlight) are changed from TCA9539 to direct GPIO.
 *        RM_HC32F4A0 Rev1.50 ch.40: SMC must leave Low-Power, reach READY, be
 *        configured, then receive UPDATEREGS; registers are read back to confirm.
 *******************************************************************************
 * Copyright (C) 2022-2025, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 * This software component is licensed by XHSC under BSD 3-Clause license.
 *******************************************************************************
 */
#include <stdio.h>
#include <string.h>
#include "bsp_lcd.h"
#include "bsp_board.h"

static stc_lcd_controller_t *m_pstcLCD = ((stc_lcd_controller_t *)LCD_SMC_REG_ADDR);

#if (LCD_RST_VIA_TCA9539 != 0U)
/* Shadow of TCA9539 PORT0 output latch. Only IO7 is touched; all other pins keep
 * their power-up value so LEDs / camera / PHY on the EV board are not disturbed. */
static uint8_t m_u8TcaOut0 = 0xFFU;

/**
 * @brief  Lightweight TCA9539 init for the EV board LCD /RST (IO7).
 * @note   Uses the same I2C1 as the FT6336U. TOUCH_Init() re-initializes I2C1
 *         afterwards, which is harmless. No full component driver is needed.
 * @retval LL_OK / LL_ERR ...
 */
static int32_t LCD_TCA9539_Init(void)
{
    stc_gpio_init_t stcGpioInit;
    uint8_t u8Reg;
    uint8_t u8Val;
    int32_t i32Ret;

    /* I2C1 pins: PD3 = SCL (func49), PF10 = SDA (func48) */
    (void)GPIO_StructInit(&stcGpioInit);
    (void)GPIO_Init(TOUCH_I2C_SCL_PORT, TOUCH_I2C_SCL_PIN, &stcGpioInit);
    (void)GPIO_Init(TOUCH_I2C_SDA_PORT, TOUCH_I2C_SDA_PIN, &stcGpioInit);
    GPIO_SetFunc(TOUCH_I2C_SCL_PORT, TOUCH_I2C_SCL_PIN, TOUCH_I2C_SCL_FUNC);
    GPIO_SetFunc(TOUCH_I2C_SDA_PORT, TOUCH_I2C_SDA_PIN, TOUCH_I2C_SDA_FUNC);
    FCG_Fcg1PeriphClockCmd(LCD_TCA9539_I2C_FCG, ENABLE);
    i32Ret = BSP_I2C_Init(LCD_TCA9539_I2C_UNIT);
    if (LL_OK != i32Ret) {
        return i32Ret;
    }

    /* Hardware reset the expander: PC13 low >= 3 ms, then release */
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinState = PIN_STAT_RST;
    stcGpioInit.u16PinDir   = PIN_DIR_OUT;
    (void)GPIO_Init(LCD_TCA9539_RST_PORT, LCD_TCA9539_RST_PIN, &stcGpioInit);
    DDL_DelayMS(3UL);
    GPIO_SetPins(LCD_TCA9539_RST_PORT, LCD_TCA9539_RST_PIN);
    DDL_DelayMS(1UL);

    i32Ret = BSP_I2C_GetDevStatus(LCD_TCA9539_I2C_UNIT, LCD_TCA9539_DEV_ADDR);
    if (LL_OK != i32Ret) {
        return i32Ret;
    }

    /* IO7 -> output, idle high (release the panel reset) */
    u8Reg = LCD_TCA9539_REG_CONFIG0;
    i32Ret = BSP_I2C_Read(LCD_TCA9539_I2C_UNIT, LCD_TCA9539_DEV_ADDR,
                          &u8Reg, 1U, &u8Val, 1UL);
    if (LL_OK != i32Ret) {
        return i32Ret;
    }
    u8Val &= (uint8_t)(~LCD_TCA9539_LCD_RST_PIN);
    u8Reg = LCD_TCA9539_REG_CONFIG0;
    i32Ret = BSP_I2C_Write(LCD_TCA9539_I2C_UNIT, LCD_TCA9539_DEV_ADDR,
                           &u8Reg, 1U, &u8Val, 1UL);
    if (LL_OK != i32Ret) {
        return i32Ret;
    }

    u8Reg = LCD_TCA9539_REG_OUTPUT0;
    i32Ret = BSP_I2C_Read(LCD_TCA9539_I2C_UNIT, LCD_TCA9539_DEV_ADDR,
                          &u8Reg, 1U, &m_u8TcaOut0, 1UL);
    if (LL_OK != i32Ret) {
        return i32Ret;
    }
    m_u8TcaOut0 |= LCD_TCA9539_LCD_RST_PIN;
    u8Reg = LCD_TCA9539_REG_OUTPUT0;
    i32Ret = BSP_I2C_Write(LCD_TCA9539_I2C_UNIT, LCD_TCA9539_DEV_ADDR,
                           &u8Reg, 1U, &m_u8TcaOut0, 1UL);
    return i32Ret;
}

/**
 * @brief  Drive TCA9539 IO7 (the EV board's LCD /RST net).
 */
static void LCD_TCA9539_WriteRst(uint8_t u8Cmd)
{
    uint8_t u8Reg = LCD_TCA9539_REG_OUTPUT0;

    if (LCD_PIN_SET == u8Cmd) {
        m_u8TcaOut0 |= LCD_TCA9539_LCD_RST_PIN;
    } else {
        m_u8TcaOut0 &= (uint8_t)(~LCD_TCA9539_LCD_RST_PIN);
    }
    (void)BSP_I2C_Write(LCD_TCA9539_I2C_UNIT, LCD_TCA9539_DEV_ADDR,
                        &u8Reg, 1U, &m_u8TcaOut0, 1UL);
}
#endif /* LCD_RST_VIA_TCA9539 */

/**
 * @brief  Configure all EXMC pins to function 12 (EXMC) with high drive strength.
 * @param  None
 * @retval None
 */
static void LCD_Port_Init(void)
{
    stc_gpio_init_t stcGpioInit;

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDrv = PIN_HIGH_DRV;

    (void)GPIO_Init(LCD_CS_PORT, LCD_CS_PIN, &stcGpioInit);
    (void)GPIO_Init(LCD_WE_PORT, LCD_WE_PIN, &stcGpioInit);
    (void)GPIO_Init(LCD_OE_PORT, LCD_OE_PIN, &stcGpioInit);
    (void)GPIO_Init(LCD_RS_PORT, LCD_RS_PIN, &stcGpioInit);

    (void)GPIO_Init(LCD_D0_PORT,  LCD_D0_PIN,  &stcGpioInit);
    (void)GPIO_Init(LCD_D1_PORT,  LCD_D1_PIN,  &stcGpioInit);
    (void)GPIO_Init(LCD_D2_PORT,  LCD_D2_PIN,  &stcGpioInit);
    (void)GPIO_Init(LCD_D3_PORT,  LCD_D3_PIN,  &stcGpioInit);
    (void)GPIO_Init(LCD_D4_PORT,  LCD_D4_PIN,  &stcGpioInit);
    (void)GPIO_Init(LCD_D5_PORT,  LCD_D5_PIN,  &stcGpioInit);
    (void)GPIO_Init(LCD_D6_PORT,  LCD_D6_PIN,  &stcGpioInit);
    (void)GPIO_Init(LCD_D7_PORT,  LCD_D7_PIN,  &stcGpioInit);
    (void)GPIO_Init(LCD_D8_PORT,  LCD_D8_PIN,  &stcGpioInit);
    (void)GPIO_Init(LCD_D9_PORT,  LCD_D9_PIN,  &stcGpioInit);
    (void)GPIO_Init(LCD_D10_PORT, LCD_D10_PIN, &stcGpioInit);
    (void)GPIO_Init(LCD_D11_PORT, LCD_D11_PIN, &stcGpioInit);
    (void)GPIO_Init(LCD_D12_PORT, LCD_D12_PIN, &stcGpioInit);
    (void)GPIO_Init(LCD_D13_PORT, LCD_D13_PIN, &stcGpioInit);
    (void)GPIO_Init(LCD_D14_PORT, LCD_D14_PIN, &stcGpioInit);
    (void)GPIO_Init(LCD_D15_PORT, LCD_D15_PIN, &stcGpioInit);

    GPIO_SetFunc(LCD_D0_PORT,  LCD_D0_PIN,  LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_D1_PORT,  LCD_D1_PIN,  LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_D2_PORT,  LCD_D2_PIN,  LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_D3_PORT,  LCD_D3_PIN,  LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_D4_PORT,  LCD_D4_PIN,  LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_D5_PORT,  LCD_D5_PIN,  LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_D6_PORT,  LCD_D6_PIN,  LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_D7_PORT,  LCD_D7_PIN,  LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_D8_PORT,  LCD_D8_PIN,  LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_D9_PORT,  LCD_D9_PIN,  LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_D10_PORT, LCD_D10_PIN, LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_D11_PORT, LCD_D11_PIN, LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_D12_PORT, LCD_D12_PIN, LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_D13_PORT, LCD_D13_PIN, LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_D14_PORT, LCD_D14_PIN, LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_D15_PORT, LCD_D15_PIN, LCD_SMC_GPIO_FUNC);

    GPIO_SetFunc(LCD_CS_PORT, LCD_CS_PIN, LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_RS_PORT, LCD_RS_PIN, LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_WE_PORT, LCD_WE_PIN, LCD_SMC_GPIO_FUNC);
    GPIO_SetFunc(LCD_OE_PORT, LCD_OE_PIN, LCD_SMC_GPIO_FUNC);
}

/**
 * @brief  Enable the SMC clock, leave low power, configure chip 3 and confirm the regs.
 * @param  None
 * @retval int32_t LL_OK / LL_ERR
 */
int32_t BSP_LCD_SMC_Init(void)
{
    __IO uint32_t u32To = 0UL;
    int32_t i32Ret = LL_OK;
    stc_exmc_smc_init_t stcSmcInit;
    stc_exmc_smc_chip_config_t stcChipConfig;
    stc_exmc_smc_timing_config_t stcTimingConfig;
    en_flag_status_t enChipStatus = RESET;
    en_flag_status_t enTimingStatus = RESET;

    LCD_Port_Init();

    FCG_Fcg3PeriphClockCmd(FCG3_PERIPH_SMC, ENABLE);
    EXMC_SMC_Cmd(ENABLE);
    EXMC_SMC_ExitLowPower();

    while (EXMC_SMC_READY != EXMC_SMC_GetStatus()) {
        if (u32To > LCD_SMC_INIT_TIMEOUT) {
            i32Ret = LL_ERR;
            break;
        }
        u32To++;
    }

    if (LL_OK == i32Ret) {
        (void)EXMC_SMC_StructInit(&stcSmcInit);
        stcSmcInit.stcChipConfig.u32AddrMatch     = LCD_SMC_MATCH_ADDR;
        stcSmcInit.stcChipConfig.u32AddrMask      = LCD_SMC_ADDR_MASK;
        stcSmcInit.stcChipConfig.u32MemoryWidth   = LCD_SMC_MEMORY_WIDTH;
        stcSmcInit.stcChipConfig.u32BAA           = LCD_SMC_BAA;
        stcSmcInit.stcChipConfig.u32ADV           = LCD_SMC_ADV;
        stcSmcInit.stcChipConfig.u32BLS           = LCD_SMC_BLS;
        stcSmcInit.stcChipConfig.u32ReadBurstLen  = LCD_SMC_READ_BURST;
        stcSmcInit.stcChipConfig.u32WriteBurstLen = LCD_SMC_WRITE_BURST;
        stcSmcInit.stcChipConfig.u32ReadMode      = LCD_SMC_READ_MODE;
        stcSmcInit.stcChipConfig.u32WriteMode     = LCD_SMC_WRITE_MODE;

        /* EXCLK 30 MHz @ 3.3 V. Keep the slow READ timing for reliable ID reads. */
        stcSmcInit.stcTimingConfig.u8RC    = LCD_SMC_TIMING_RC;
        stcSmcInit.stcTimingConfig.u8WC    = LCD_SMC_TIMING_WC;
        stcSmcInit.stcTimingConfig.u8CEOE  = LCD_SMC_TIMING_CEOE;
        stcSmcInit.stcTimingConfig.u8WP    = LCD_SMC_TIMING_WP;
        stcSmcInit.stcTimingConfig.u8PC    = LCD_SMC_TIMING_PC;
        stcSmcInit.stcTimingConfig.u8TR    = LCD_SMC_TIMING_TR;

        (void)EXMC_SMC_Init(LCD_SMC_CHIP, &stcSmcInit);
        EXMC_SMC_SetCommand(LCD_SMC_CHIP, EXMC_SMC_CMD_UPDATEREGS, 0UL, 0UL);

        u32To = 0UL;
        while ((enChipStatus != SET) || (enTimingStatus != SET)) {
            (void)EXMC_SMC_GetTimingConfig(LCD_SMC_CHIP, &stcTimingConfig);
            if (0 == memcmp((uint8_t *)&stcTimingConfig,
                            (uint8_t *)&stcSmcInit.stcTimingConfig, sizeof(stcTimingConfig))) {
                enTimingStatus = SET;
            }
            (void)EXMC_SMC_GetChipConfig(LCD_SMC_CHIP, &stcChipConfig);
            if (0 == memcmp((uint8_t *)&stcChipConfig,
                            (uint8_t *)&stcSmcInit.stcChipConfig, sizeof(stcChipConfig))) {
                enChipStatus = SET;
            }
            if (u32To > LCD_SMC_INIT_TIMEOUT) {
                i32Ret = LL_ERR;
                break;
            }
            u32To++;
        }
    }

    return i32Ret;
}

/**
 * @brief  Initialize LCD /RESET and backlight GPIOs (direct drive, no IO expander).
 * @param  None
 * @retval None
 */
void BSP_LCD_IO_Init(void)
{
    stc_gpio_init_t stcGpioInit;

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDrv  = PIN_HIGH_DRV;
    stcGpioInit.u16PinDir  = PIN_DIR_OUT;
    (void)GPIO_Init(LCD_BKL_PORT, LCD_BKL_PIN, &stcGpioInit);
    GPIO_ResetPins(LCD_BKL_PORT, LCD_BKL_PIN);   /* backlight off until init */

#if (LCD_RST_VIA_TCA9539 != 0U)
#if (LCD_DIAG_MODE != 1U)
    {
        int32_t i32Ret = LCD_TCA9539_Init();
        DDL_Printf("TCA9539 (LCD /RST via IO7): %s (%d)\r\n",
                   (LL_OK == i32Ret) ? "OK" : "FAILED", (int)i32Ret);
    }
#endif
#else
    (void)GPIO_Init(LCD_RST_PORT, LCD_RST_PIN, &stcGpioInit);
    GPIO_SetPins(LCD_RST_PORT, LCD_RST_PIN);     /* /RESET idle high */
#endif
}

/**
 * @brief  Drive the LCD /RESET pin.
 * @param  [in] u8Cmd  LCD_PIN_RESET (low) / LCD_PIN_SET (high)
 * @retval None
 */
void BSP_LCD_RSTCmd(uint8_t u8Cmd)
{
#if (LCD_RST_VIA_TCA9539 != 0U)
    LCD_TCA9539_WriteRst(u8Cmd);
#else
    if (LCD_PIN_SET == u8Cmd) {
        GPIO_SetPins(LCD_RST_PORT, LCD_RST_PIN);
    } else {
        GPIO_ResetPins(LCD_RST_PORT, LCD_RST_PIN);
    }
#endif
}

/**
 * @brief  Backlight enable signal (drives an external transistor, not the LED string).
 * @param  [in] u8Cmd  LCD_PIN_SET = on / LCD_PIN_RESET = off
 * @retval None
 */
void BSP_LCD_BKLCmd(uint8_t u8Cmd)
{
    if (LCD_PIN_SET == u8Cmd) {
        GPIO_SetPins(LCD_BKL_PORT, LCD_BKL_PIN);
    } else {
        GPIO_ResetPins(LCD_BKL_PORT, LCD_BKL_PIN);
    }
}

/**
 * @brief  Write an 8-bit command to the LCD register port (RS = 0).
 * @param  [in] u8Cmd  Command byte
 * @retval None
 */
void BSP_LCD_WriteCmd(uint8_t u8Cmd)
{
    m_pstcLCD->u16REG = (uint16_t)u8Cmd;
}

/**
 * @brief  Write one 8-bit parameter / data byte to the LCD RAM port (RS = 1).
 * @param  [in] u8Data  Data byte
 * @retval None
 */
void BSP_LCD_WriteByte(uint8_t u8Data)
{
    m_pstcLCD->u16RAM = (uint16_t)u8Data;
}

/**
 * @brief  Write one RGB565 pixel.
 * @note   16-bit bus: one 16-bit write. 8-bit bus: high byte first, then low byte.
 * @param  [in] u16RGB565  Pixel color
 * @retval None
 */
void BSP_LCD_WriteColor(uint16_t u16RGB565)
{
#if (LCD_COLOR_INVERT != 0U)
    /* Fallback for panels that still show inverted colors: invert pixels here.
     * The default BOE module uses the vendor INVON table, so this is disabled. */
    u16RGB565 = (uint16_t)(~u16RGB565);
#endif
#if (LCD_BUS_16BIT != 0U)
    m_pstcLCD->u16RAM = u16RGB565;
#else
    m_pstcLCD->u16RAM = (uint16_t)(u16RGB565 >> 8U);
    m_pstcLCD->u16RAM = (uint16_t)(u16RGB565 & 0xFFU);
#endif
}

/**
 * @brief  Read one data byte from the LCD (RS = 1).
 * @note   On a 16-bit SMC bus the panel drives D[7:0] only; mask the high byte.
 * @param  None
 * @retval uint8_t Data byte
 */
uint8_t BSP_LCD_ReadByte(void)
{
    return (uint8_t)(m_pstcLCD->u16RAM & 0xFFU);
}

/**
 * @brief  Write a single 8-bit register.
 * @param  [in] u8Reg   Register/command
 * @param  [in] u8Data  Value
 * @retval None
 */
void BSP_LCD_WriteRegData(uint8_t u8Reg, uint8_t u8Data)
{
    BSP_LCD_WriteCmd(u8Reg);
    BSP_LCD_WriteByte(u8Data);
}

/**
 * @brief  Read a burst of bytes after issuing a read command.
 * @note   The FIRST read after a command on an i8080 panel is a dummy byte:
 *         the controller only latches the read data on the second /RD pulse.
 *         It is discarded here so callers receive the real ID bytes from
 *         index 0.
 * @param  [in]  u8Reg    Read command (e.g. 0x04 RDDID, 0xD3 for ILI)
 * @param  [out] pu8Buf   Destination buffer
 * @param  [in]  u32Len   Number of bytes
 * @retval None
 */
void BSP_LCD_ReadRegBytes(uint8_t u8Reg, uint8_t *pu8Buf, uint32_t u32Len)
{
    uint32_t i;

    if (NULL == pu8Buf) {
        return;
    }

    BSP_LCD_WriteCmd(u8Reg);
    (void)BSP_LCD_ReadByte();               /* discard the dummy byte */
    for (i = 0UL; i < u32Len; i++) {
        pu8Buf[i] = BSP_LCD_ReadByte();
    }
}
