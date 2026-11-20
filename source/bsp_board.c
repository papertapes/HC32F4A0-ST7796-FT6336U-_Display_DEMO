/**
 *******************************************************************************
 * @file  bsp_board.c
 * @brief Minimal BSP implementation: clock tree, I2C low level and debug printf.
 * @note  Clock settings are byte-for-byte the official BSP_CLK_Init():
 *        XTAL 8 MHz -> PLLH 240 MHz; PCLK0/HCLK 240 MHz, PCLK1/4 120 MHz,
 *        PCLK2/3 60 MHz, EXCLK 120 MHz (main() divides it to 30 MHz for SMC).
 *******************************************************************************
 * Copyright (C) 2022-2025, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 * This software component is licensed by XHSC under BSD 3-Clause license.
 *******************************************************************************
 */
#include <stdio.h>
#include "bsp_board.h"

/**
 * @brief  Initialize the system clock (PLLH @ 240 MHz) and enable caches.
 * @param  None
 * @retval None
 */
void BSP_CLK_Init(void)
{
    stc_clock_xtal_init_t stcXtalInit;
    stc_clock_pll_init_t stcPLLHInit;

    /* PCLK0/HCLK max 240 MHz, PCLK1/PCLK4 max 120 MHz, PCLK2/PCLK3 max 60 MHz */
    CLK_SetClockDiv(CLK_BUS_CLK_ALL, \
                    (CLK_PCLK0_DIV1 | CLK_PCLK1_DIV2 | CLK_PCLK2_DIV4 | \
                     CLK_PCLK3_DIV4 | CLK_PCLK4_DIV2 | CLK_EXCLK_DIV2 | \
                     CLK_HCLK_DIV1));

    /* XTAL pins must be analog before enabling the oscillator */
    GPIO_AnalogCmd(BSP_XTAL_PORT, BSP_XTAL_PIN, ENABLE);
    (void)CLK_XtalStructInit(&stcXtalInit);
    stcXtalInit.u8Mode       = CLK_XTAL_MD_OSC;
    stcXtalInit.u8Drv        = CLK_XTAL_DRV_ULOW;
    stcXtalInit.u8State      = CLK_XTAL_ON;
    stcXtalInit.u8StableTime = CLK_XTAL_STB_2MS;
    (void)CLK_XtalInit(&stcXtalInit);

    /* VCO = (8 MHz / 1) * 120 = 960 MHz; PLLP/Q/R = /4 -> 240 MHz */
    (void)CLK_PLLStructInit(&stcPLLHInit);
    stcPLLHInit.u8PLLState = CLK_PLL_ON;
    stcPLLHInit.PLLCFGR = 0UL;
    stcPLLHInit.PLLCFGR_f.PLLM   = 1UL - 1UL;
    stcPLLHInit.PLLCFGR_f.PLLN   = 120UL - 1UL;
    stcPLLHInit.PLLCFGR_f.PLLP   = 4UL - 1UL;
    stcPLLHInit.PLLCFGR_f.PLLQ   = 4UL - 1UL;
    stcPLLHInit.PLLCFGR_f.PLLR   = 4UL - 1UL;
    stcPLLHInit.PLLCFGR_f.PLLSRC = CLK_PLL_SRC_XTAL;
    (void)CLK_PLLInit(&stcPLLHInit);

    SRAM_SetWaitCycle(SRAM_SRAMH, SRAM_WAIT_CYCLE0, SRAM_WAIT_CYCLE0);
    SRAM_SetWaitCycle((SRAM_SRAM123 | SRAM_SRAM4 | SRAM_SRAMB), SRAM_WAIT_CYCLE1, SRAM_WAIT_CYCLE1);

    (void)EFM_SetWaitCycle(EFM_WAIT_CYCLE5);   /* 5 wait cycles @ 240 MHz */
    GPIO_SetReadWaitCycle(GPIO_RD_WAIT4);      /* 4 cycles for 200~250 MHz */

    CLK_SetSysClockSrc(CLK_SYSCLK_SRC_PLL);

    EFM_CacheRamReset(ENABLE);
    EFM_CacheRamReset(DISABLE);
    EFM_PrefetchCmd(ENABLE);
    EFM_DCacheCmd(ENABLE);
    EFM_ICacheCmd(ENABLE);
}

/**
 * @brief  Pre-initialize the debug UART used by DDL_Printf.
 * @param  [in] vpDevice     USART instance (unused, taken from macro)
 * @param  [in] u32Baudrate  Target baudrate
 * @retval int32_t LL_OK / LL_ERR
 */
int32_t BSP_PRINTF_Preinit(void *vpDevice, uint32_t u32Baudrate)
{
    uint32_t u32Div;
    float32_t f32Error;
    stc_usart_uart_init_t stcUartInit;
    int32_t i32Ret = LL_ERR_INVD_PARAM;

    (void)vpDevice;

    if (0UL != u32Baudrate) {
        GPIO_SetFunc(BSP_PRINTF_PORT, BSP_PRINTF_PIN, BSP_PRINTF_PORT_FUNC);
        FCG_Fcg3PeriphClockCmd(BSP_PRINTF_DEVICE_FCG, ENABLE);

        (void)USART_UART_StructInit(&stcUartInit);
        stcUartInit.u32OverSampleBit = USART_OVER_SAMPLE_8BIT;
        (void)USART_UART_Init(BSP_PRINTF_DEVICE, &stcUartInit, NULL);

        for (u32Div = 0UL; u32Div <= USART_CLK_DIV64; u32Div++) {
            USART_SetClockDiv(BSP_PRINTF_DEVICE, u32Div);
            i32Ret = USART_SetBaudrate(BSP_PRINTF_DEVICE, u32Baudrate, &f32Error);
            if ((LL_OK == i32Ret) && \
                ((-BSP_PRINTF_BAUDRATE_ERR_MAX <= f32Error) && (f32Error <= BSP_PRINTF_BAUDRATE_ERR_MAX))) {
                USART_FuncCmd(BSP_PRINTF_DEVICE, USART_TX, ENABLE);
                break;
            } else {
                i32Ret = LL_ERR;
            }
        }
    }

    return i32Ret;
}

/**
 * @brief  Route printf() to USART1. Optional: safe to omit on a board without a serial console.
 * @param  None
 * @retval None
 */
void BSP_PRINTF_Init(void)
{
    DDL_PrintfInit(BSP_PRINTF_DEVICE, BSP_PRINTF_BAUDRATE, BSP_PRINTF_Preinit);
}

/**
 * @brief  Initialize an I2C unit @ 100 kHz (standard mode).
 * @param  [in] I2Cx  I2C instance
 * @retval int32_t LL_OK / LL_ERR...
 */
int32_t BSP_I2C_Init(CM_I2C_TypeDef *I2Cx)
{
    int32_t i32Ret;
    float32_t fErr;
    stc_i2c_init_t stcI2cInit;
    uint32_t u32SrcClk;
    uint32_t u32ClkDiv;
    uint32_t u32ClkDivReg;

    u32SrcClk = I2C_SRC_CLK;
    u32ClkDiv = u32SrcClk / TOUCH_I2C_BAUDRATE / I2C_WIDTH_MAX_IMME;
    for (u32ClkDivReg = I2C_CLK_DIV1; u32ClkDivReg <= I2C_CLK_DIV128; u32ClkDivReg++) {
        if (u32ClkDiv < (1UL << u32ClkDivReg)) {
            break;
        }
    }

    (void)I2C_DeInit(I2Cx);
    (void)I2C_StructInit(&stcI2cInit);
    stcI2cInit.u32Baudrate = TOUCH_I2C_BAUDRATE;
    /* SCL rise/fall time about 400 ns on the EV board */
    stcI2cInit.u32SclTime  = (uint32_t)((uint64_t)400UL * \
                             ((uint64_t)u32SrcClk / ((uint64_t)1UL << u32ClkDivReg)) / \
                             (uint64_t)1000000000UL);
    stcI2cInit.u32ClockDiv = u32ClkDivReg;
    i32Ret = I2C_Init(I2Cx, &stcI2cInit, &fErr);

    if (LL_OK == i32Ret) {
        I2C_BusWaitCmd(I2Cx, ENABLE);
        I2C_Cmd(I2Cx, ENABLE);
    }

    return i32Ret;
}

/**
 * @brief  I2C register write: [START][ADDR+W][reg...][data...][STOP].
 * @retval int32_t LL_OK / LL_ERR / LL_ERR_TIMEOUT
 */
int32_t BSP_I2C_Write(CM_I2C_TypeDef *I2Cx, uint16_t u16DevAddr,
                      const uint8_t *pu8Reg, uint8_t u8RegLen,
                      const uint8_t *pu8Buf, uint32_t u32Len)
{
    int32_t i32Ret;

    I2C_SWResetCmd(I2Cx, ENABLE);
    I2C_SWResetCmd(I2Cx, DISABLE);
    i32Ret = I2C_Start(I2Cx, TOUCH_I2C_TIMEOUT);
    if (LL_OK == i32Ret) {
        i32Ret = I2C_TransAddr(I2Cx, u16DevAddr, I2C_DIR_TX, TOUCH_I2C_TIMEOUT);
        if (LL_OK == i32Ret) {
            i32Ret = I2C_TransData(I2Cx, pu8Reg, u8RegLen, TOUCH_I2C_TIMEOUT);
            if (LL_OK == i32Ret) {
                i32Ret = I2C_TransData(I2Cx, pu8Buf, u32Len, TOUCH_I2C_TIMEOUT);
            }
        }
    }
    (void)I2C_Stop(I2Cx, TOUCH_I2C_TIMEOUT);
    return i32Ret;
}

/**
 * @brief  I2C register read: [START][ADDR+W][reg...][ReSTART][ADDR+R][data...][STOP].
 * @retval int32_t LL_OK / LL_ERR / LL_ERR_TIMEOUT
 */
int32_t BSP_I2C_Read(CM_I2C_TypeDef *I2Cx, uint16_t u16DevAddr,
                     const uint8_t *pu8Reg, uint8_t u8RegLen,
                     uint8_t *pu8Buf, uint32_t u32Len)
{
    int32_t i32Ret;

    I2C_SWResetCmd(I2Cx, ENABLE);
    I2C_SWResetCmd(I2Cx, DISABLE);
    i32Ret = I2C_Start(I2Cx, TOUCH_I2C_TIMEOUT);
    if (LL_OK == i32Ret) {
        i32Ret = I2C_TransAddr(I2Cx, u16DevAddr, I2C_DIR_TX, TOUCH_I2C_TIMEOUT);
        if (LL_OK == i32Ret) {
            i32Ret = I2C_TransData(I2Cx, pu8Reg, u8RegLen, TOUCH_I2C_TIMEOUT);
            if (LL_OK == i32Ret) {
                i32Ret = I2C_Restart(I2Cx, TOUCH_I2C_TIMEOUT);
                if (LL_OK == i32Ret) {
                    if (1UL == u32Len) {
                        I2C_AckConfig(I2Cx, I2C_NACK);
                    }
                    i32Ret = I2C_TransAddr(I2Cx, u16DevAddr, I2C_DIR_RX, TOUCH_I2C_TIMEOUT);
                    if (LL_OK == i32Ret) {
                        i32Ret = I2C_MasterReceiveDataAndStop(I2Cx, pu8Buf, u32Len, TOUCH_I2C_TIMEOUT);
                    }
                    I2C_AckConfig(I2Cx, I2C_ACK);
                }
            }
        }
    }
    if (LL_OK != i32Ret) {
        (void)I2C_Stop(I2Cx, TOUCH_I2C_TIMEOUT);
    }
    return i32Ret;
}

/**
 * @brief  Probe whether an I2C device answers with ACK.
 * @retval int32_t LL_OK = device present
 */
int32_t BSP_I2C_GetDevStatus(CM_I2C_TypeDef *I2Cx, uint16_t u16DevAddr)
{
    int32_t i32Ret;

    i32Ret = I2C_Start(I2Cx, TOUCH_I2C_TIMEOUT);
    if (LL_OK == i32Ret) {
        i32Ret = I2C_TransAddr(I2Cx, u16DevAddr, I2C_DIR_TX, TOUCH_I2C_TIMEOUT);
        if (LL_OK == i32Ret) {
            if (SET == I2C_GetStatus(I2Cx, I2C_FLAG_ACKR)) {
                i32Ret = LL_ERR;
            }
        }
    }
    (void)I2C_Stop(I2Cx, TOUCH_I2C_TIMEOUT);
    return i32Ret;
}
