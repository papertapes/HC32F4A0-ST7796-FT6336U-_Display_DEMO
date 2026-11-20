/**
 *******************************************************************************
 * @file  hc32f4xx_conf.h
 * @brief DDL module selection for the standalone 4.0" 320x480 i8080 LCD project.
 * @note  Derived from exmc_smc_lcd_nt35510/source/hc32f4xx_conf.h (XHSC DDL Rev2.4.0).
 *        Only the modules used by this project are set to DDL_ON.
 *        LL_ICG_ENABLE and LL_UTILITY_ENABLE must stay ON (see DDL documentation).
 *******************************************************************************
 * Copyright (C) 2022-2025, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 * This software component is licensed by XHSC under BSD 3-Clause license.
 *******************************************************************************
 */
#ifndef __HC32F4XX_CONF_H__
#define __HC32F4XX_CONF_H__

#ifdef __cplusplus
extern "C"
{
#endif

/* Core modules: must be ON */
#define LL_ICG_ENABLE                               (DDL_ON)
#define LL_UTILITY_ENABLE                           (DDL_ON)
#define LL_PRINT_ENABLE                             (DDL_ON)

/* Modules used: clock / flash / FCG / GPIO / I2C / INTC / PWC / SMC / SRAM / USART */
#define LL_CLK_ENABLE                               (DDL_ON)
#define LL_EFM_ENABLE                               (DDL_ON)
#define LL_FCG_ENABLE                               (DDL_ON)
#define LL_GPIO_ENABLE                              (DDL_ON)
#define LL_I2C_ENABLE                               (DDL_ON)
#define LL_INTERRUPTS_ENABLE                        (DDL_ON)
#define LL_PWC_ENABLE                               (DDL_ON)
#define LL_SMC_ENABLE                               (DDL_ON)
#define LL_SRAM_ENABLE                              (DDL_ON)
#define LL_USART_ENABLE                             (DDL_ON)

/* All other LL modules are not used */
#define LL_ADC_ENABLE                               (DDL_OFF)
#define LL_AES_ENABLE                               (DDL_OFF)
#define LL_AOS_ENABLE                               (DDL_OFF)
#define LL_CAN_ENABLE                               (DDL_OFF)
#define LL_CMP_ENABLE                               (DDL_OFF)
#define LL_CRC_ENABLE                               (DDL_OFF)
#define LL_CTC_ENABLE                               (DDL_OFF)
#define LL_DAC_ENABLE                               (DDL_OFF)
#define LL_DBGC_ENABLE                              (DDL_OFF)
#define LL_DCU_ENABLE                               (DDL_OFF)
#define LL_DMA_ENABLE                               (DDL_OFF)
#define LL_DMC_ENABLE                               (DDL_OFF)
#define LL_DVP_ENABLE                               (DDL_OFF)
#define LL_EMB_ENABLE                               (DDL_OFF)
#define LL_ETH_ENABLE                               (DDL_OFF)
#define LL_EVENT_PORT_ENABLE                        (DDL_OFF)
#define LL_FCM_ENABLE                               (DDL_OFF)
#define LL_FMAC_ENABLE                              (DDL_OFF)
#define LL_HASH_ENABLE                              (DDL_OFF)
#define LL_HRPWM_ENABLE                             (DDL_OFF)
#define LL_INTERRUPTS_SHARE_ENABLE                  (DDL_OFF)
#define LL_I2S_ENABLE                               (DDL_OFF)
#define LL_KEYSCAN_ENABLE                           (DDL_OFF)
#define LL_MAU_ENABLE                               (DDL_OFF)
#define LL_MPU_ENABLE                               (DDL_OFF)
#define LL_NFC_ENABLE                               (DDL_OFF)
#define LL_OTS_ENABLE                               (DDL_OFF)
#define LL_QSPI_ENABLE                              (DDL_OFF)
#define LL_RMU_ENABLE                               (DDL_OFF)
#define LL_RTC_ENABLE                               (DDL_OFF)
#define LL_SDIOC_ENABLE                             (DDL_OFF)
#define LL_SPI_ENABLE                               (DDL_OFF)
#define LL_SWDT_ENABLE                              (DDL_OFF)
#define LL_TMR0_ENABLE                              (DDL_OFF)
#define LL_TMR2_ENABLE                              (DDL_OFF)
#define LL_TMR4_ENABLE                              (DDL_OFF)
#define LL_TMR6_ENABLE                              (DDL_OFF)
#define LL_TMRA_ENABLE                              (DDL_OFF)
#define LL_TRNG_ENABLE                              (DDL_OFF)
#define LL_USB_ENABLE                               (DDL_OFF)
#define LL_WDT_ENABLE                               (DDL_OFF)

/* Board BSP components are NOT used: this project implements its own board layer.
 * The macros are kept (all OFF) for source compatibility with the official DDL headers. */
#define BSP_EV_HC32F4A0_LQFP176                     (1U)
#define BSP_EV_HC32F4XX                             (0U)
#define BSP_24CXX_ENABLE                            (DDL_OFF)
#define BSP_GT9XX_ENABLE                            (DDL_OFF)
#define BSP_IS42S16400J7TLI_ENABLE                  (DDL_OFF)
#define BSP_IS62WV51216_ENABLE                      (DDL_OFF)
#define BSP_MT29F2G08AB_ENABLE                      (DDL_OFF)
#define BSP_NT35510_ENABLE                          (DDL_OFF)
#define BSP_OV5640_ENABLE                           (DDL_OFF)
#define BSP_RTL8201_ENABLE                          (DDL_OFF)
#define BSP_TCA9539_ENABLE                          (DDL_OFF)
#define BSP_W25QXX_ENABLE                           (DDL_OFF)
#define BSP_WM8731_ENABLE                           (DDL_OFF)

#ifdef __cplusplus
}
#endif

#endif /* __HC32F4XX_CONF_H__ */
