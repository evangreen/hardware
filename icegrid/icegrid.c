/*++

Copyright (c) 2016 Evan Green. All Rights Reserved.

Module Name:

    icegrid.c

Abstract:

    This module implements the firmware for the Ice cube LED grid project.

Author:

    Evan Green 12-Nov-2016

Environment:

    STM32F103C8T6 firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "stm32f1xx_hal.h"
#include "icegrid.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

volatile long MyGlobal = 0x12348888;
volatile long MyVar = 0;

const GPIO_InitTypeDef Pc13Gpio = {
    .Pin = GPIO_PIN_13,
    .Mode = GPIO_MODE_OUTPUT_PP,
    .Pull = GPIO_NOPULL,
    .Speed = GPIO_SPEED_FREQ_LOW
};

//
// Configure the main oscillator to run off of HSE (high speed external).
// It's an 8MHz crystal, so 8 / PreDiv1 * Mul = 8 / 1 * 9 = 72MHz.
//

const RCC_OscInitTypeDef OscParameters = {
    .OscillatorType = RCC_OSCILLATORTYPE_HSE,
    .HSEState = RCC_HSE_ON,
    .HSEPredivValue = RCC_HSE_PREDIV_DIV1,
    .LSEState = RCC_LSE_OFF,
    .HSIState = RCC_HSI_OFF,
    .HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT,
    .LSIState = RCC_LSI_OFF,
    .PLL = {
        .PLLState = RCC_PLL_ON,
        .PLLSource = RCC_PLLSOURCE_HSE,
        .PLLMUL = RCC_PLL_MUL9
    }
};

const RCC_ClkInitTypeDef ClkParameters = {
    .ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                 RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2,

    .SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK,
    .AHBCLKDivider = RCC_SYSCLK_DIV1,
    .APB1CLKDivider = RCC_HCLK_DIV2,
    .APB2CLKDivider = RCC_HCLK_DIV1
};

//
// ------------------------------------------------------------------ Functions
//

__NORETURN
int
main (
    void
    )

/*++

Routine Description:

    This routine implements the main function for the firmware. It should not
    return.

Arguments:

    None.

Return Value:

    Does not return.

--*/

{

    uint32_t Error;
    uint32_t Index;

    DbgInititialize();
    HAL_Init();
    HAL_RCC_OscConfig((RCC_OscInitTypeDef *)&OscParameters);
    HAL_RCC_ClockConfig((RCC_ClkInitTypeDef *)&ClkParameters, FLASH_LATENCY_2);
    __HAL_RCC_GPIOC_CLK_ENABLE();
    HAL_GPIO_Init(GPIOC, (GPIO_InitTypeDef *)&Pc13Gpio);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    Ws2812Initialize();
    Esp8622Initialize();
    Error = Esp8266Configure();
    if (Error != 0) {
        Ws2812ClearDisplay();
        Ws2812OutputBinary(0, 5, Error, LED_COLOR_RED);
    }

    Index = 0;
    while (1) {
        HAL_Delay(200);
        //Ws2812OutputBinary(0, 16, Index, Index);
        Index += 1;
    }
}

void
SysTick_Handler (
    void
    )

/*++

Routine Description:

    This routine is called when the system tick timer interrupt fires.

Arguments:

    None.

Return Value:

    None.

--*/

{

    HAL_IncTick();
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

void
__libc_init_array (
    void
    )

/*++

Routine Description:

    This routine is called by the startup assembly to call all the static
    constructors.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

