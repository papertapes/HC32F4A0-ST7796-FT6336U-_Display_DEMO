/**
 *******************************************************************************
 * @file  bsp_board.h
 * @brief Minimal board support: system clock, hardware I2C and debug printf.
 * @note  Extracted from XHSC ev_hc32f4a0_lqfp176.c (DDL Rev2.4.0); KEYSCAN / LED /
 *        XTAL32 / TCA9539 parts intentionally removed for a standalone project.
 *******************************************************************************
 * Copyright (C) 2022-2025, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 * This software component is licensed by XHSC under BSD 3-Clause license.
 *******************************************************************************
 */
#ifndef __BSP_BOARD_H__
#define __BSP_BOARD_H__

#include "hc32_ll.h"
#include "lcd_config.h"

/* 8 MHz XTAL on PH0/PH1 (EV_HC32F4A0_LQFP176) */
#define BSP_XTAL_PORT                               (GPIO_PORT_H)
#define BSP_XTAL_PIN                                (GPIO_PIN_00 | GPIO_PIN_01)

/* Debug UART: USART1 TX on PH15, 115200-8-N-1 */
#define BSP_PRINTF_DEVICE                           (CM_USART1)
#define BSP_PRINTF_DEVICE_FCG                       (FCG3_PERIPH_USART1)
#define BSP_PRINTF_BAUDRATE                         (115200UL)
#define BSP_PRINTF_BAUDRATE_ERR_MAX                 (0.025F)
#define BSP_PRINTF_PORT                             (GPIO_PORT_H)
#define BSP_PRINTF_PIN                              (GPIO_PIN_15)
#define BSP_PRINTF_PORT_FUNC                        (GPIO_FUNC_32)

void     BSP_CLK_Init(void);
void     BSP_PRINTF_Init(void);
int32_t  BSP_PRINTF_Preinit(void *vpDevice, uint32_t u32Baudrate);
int32_t  BSP_I2C_Init(CM_I2C_TypeDef *I2Cx);
int32_t  BSP_I2C_Write(CM_I2C_TypeDef *I2Cx, uint16_t u16DevAddr,
                       const uint8_t *pu8Reg, uint8_t u8RegLen,
                       const uint8_t *pu8Buf, uint32_t u32Len);
int32_t  BSP_I2C_Read(CM_I2C_TypeDef *I2Cx, uint16_t u16DevAddr,
                      const uint8_t *pu8Reg, uint8_t u8RegLen,
                      uint8_t *pu8Buf, uint32_t u32Len);
int32_t  BSP_I2C_GetDevStatus(CM_I2C_TypeDef *I2Cx, uint16_t u16DevAddr);

#endif /* __BSP_BOARD_H__ */
