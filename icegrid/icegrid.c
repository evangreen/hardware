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
    uint32_t IpAddress;

    DbgInititialize();
    HAL_Init();
    HAL_RCC_OscConfig((RCC_OscInitTypeDef *)&OscParameters);
    HAL_RCC_ClockConfig((RCC_ClkInitTypeDef *)&ClkParameters, FLASH_LATENCY_2);
    __HAL_RCC_GPIOC_CLK_ENABLE();
    HAL_GPIO_Init(GPIOC, (GPIO_InitTypeDef *)&Pc13Gpio);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    Ws2812Initialize();
    do {
        Esp8622Initialize();
        Error = Esp8266Configure(&IpAddress);
        if (Error == 0) {
            break;
        }

        Ws2812ClearDisplay();
        Ws2812OutputBinary(0, 5, Error, LED_COLOR_RED);
        HAL_Delay(5000);

    } while (Error != 0);

    Esp8266ServeUdpForever(IpAddress);
    while (1) {
        ;
    }
}

void
IceGridProcessData (
    char *Data
    )

/*++

Routine Description:

    This routine handles incoming requests to change the LEDs. Data takes the
    form of a comma separated list of hex values in text.
    Example: FF23AC,FF00FF,...,0\r\n. The list can end early, and the remaining
    values will be set to black.

Arguments:

    Data - Supplies a pointer to the null-terminated request text.
Return Value:

    None.

--*/

{

    int Led;
    char *Previous;
    uint32_t Value;

    Previous = Data;
    for (Led = 0; Led < LED_COUNT; Led += 1) {
        Value = LibScanHexInt(&Data);
        if (Data == Previous) {
            break;
        }

        Ws2812SetLed(Led, Value);
        if (*Data == ',') {
            Data += 1;
        }
    }

    if (Led != LED_COUNT) {
        Ws2812SetLeds(Led, LED_COLOR_BLACK, LED_COUNT - Led);
    }

    return;
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

