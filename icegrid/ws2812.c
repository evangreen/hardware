/*++

Copyright (c) 2016 Evan Green.

Module Name:

    ws2812.c

Abstract:

    This module implements support for communicating with WS2812 LED strips
    on the STM32F1xx microcontrollers.

Author:

    Evan Green 10-Dec-2016

Environment:

    STM32 MCU

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "icegrid.h"
#include "stm32f1xx_hal.h"

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

//
// Configure the GPIO pin for the timer.
//

const GPIO_InitTypeDef Ws2812PwmOut = {
    .Pin = GPIO_PIN_6,
    .Mode = GPIO_MODE_AF_PP,
    .Pull = 0,
    .Speed = GPIO_SPEED_HIGH
};

//
// Configure the DMA controller to write to the timer's CCR (pulse width)
// register from the memory buffer.
//

DMA_HandleTypeDef Ws2812Dma = {
    .Instance = DMA1_Channel1,
    .Init = {
        .Direction = DMA_MEMORY_TO_PERIPH,
        .PeriphInc = DMA_PINC_DISABLE,
        .MemInc = DMA_MINC_ENABLE,
        .PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD,
        .MemDataAlignment = DMA_MDATAALIGN_HALFWORD,
        .Mode = DMA_CIRCULAR,
        .Priority = DMA_PRIORITY_HIGH
    },
};

TIM_HandleTypeDef Ws2812Timer = {
    .Instance = TIM4,
    .Init = {
        .Prescaler = 0,
        .CounterMode = TIM_COUNTERMODE_UP,
        .Period = 90, // 1.25us
        .ClockDivision = TIM_CLOCKDIVISION_DIV1,
        .RepetitionCounter = 0,
    },

    .Channel = TIM_CHANNEL_1
};

const TIM_OC_InitTypeDef Ws2812Pwm = {
    .OCMode = TIM_OCMODE_PWM1,
    .Pulse = 60,
    .OCPolarity = TIM_OCPOLARITY_HIGH,
    .OCNPolarity = TIM_OCNPOLARITY_HIGH,
    .OCFastMode = TIM_OCFAST_DISABLE,
    .OCIdleState = TIM_OUTPUTSTATE_DISABLE,
    .OCNIdleState = TIM_OUTPUTSTATE_DISABLE
};

//
// ------------------------------------------------------------------ Functions
//

void
Ws2812Initialize (
    void
    )

/*++

Routine Description:

    This routine initializes hardware support for controlling a WS2812 strip.

Arguments:

    None.

Return Value:

    None.

--*/

{

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();
    HAL_GPIO_Init(GPIOB, (GPIO_InitTypeDef *)&Ws2812PwmOut);
    HAL_TIM_Base_Init(&Ws2812Timer);
    HAL_TIM_PWM_ConfigChannel(&Ws2812Timer,
                              (TIM_OC_InitTypeDef *)&Ws2812Pwm,
                              TIM_CHANNEL_1);

    HAL_TIM_PWM_Start(&Ws2812Timer, TIM_CHANNEL_1);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

