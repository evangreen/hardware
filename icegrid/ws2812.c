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

#include "stm32f1xx_hal.h"
#include "icegrid.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of bits of data pushed per LED.
//

#define BITS_PER_LED 24

//
// Define the total number of LED bits in a frame.
//

#define LED_BITS_PER_FRAME (LED_COUNT * BITS_PER_LED)

//
// Define the PWM durations in timer ticks for high and low bits.
//

#define LED_BIT_LOW 30
#define LED_BIT_HIGH 60

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
// Define the pixel data.
//

uint32_t Ws2812PixelIo[LED_BITS_PER_FRAME];

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
        .PeriphDataAlignment = DMA_PDATAALIGN_WORD,
        .MemDataAlignment = DMA_MDATAALIGN_WORD,
        .Mode = DMA_NORMAL,
        .Priority = DMA_PRIORITY_HIGH
    },
};

//
// The timer ticks at 72MHz.
//

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

    int Index;

    for (Index = 0; Index < LED_BITS_PER_FRAME; Index += 1) {
        if (Index & 1) {
            Ws2812PixelIo[Index] = LED_BIT_HIGH;

        } else {
            Ws2812PixelIo[Index] = LED_BIT_LOW;
        }
    }

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    //
    // Configure the GPIO pin that will output the signal. In this case it
    // corresponds to TIM4_CH1.
    //

    HAL_GPIO_Init(GPIOB, (GPIO_InitTypeDef *)&Ws2812PwmOut);

    //
    // Initialize the timer to have a period of 1.25us, which is the period for
    // a single bit of the WS2812.
    //

    HAL_TIM_PWM_Init(&Ws2812Timer);
    HAL_TIM_PWM_ConfigChannel(&Ws2812Timer,
                              (TIM_OC_InitTypeDef *)&Ws2812Pwm,
                              TIM_CHANNEL_1);

    HAL_DMA_Init(&Ws2812Dma);
    __HAL_LINKDMA(&Ws2812Timer, hdma[TIM_DMA_ID_CC1], Ws2812Dma);
    TIM4->CCR1 = 0;
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
    HAL_NVIC_SetPriority(TIM4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    //
    // Start DMA. With each compare match of the timer, a DMA request will fire.
    // This causes the DMA engine to write a new value from the given buffer
    // into the timer's compare register, which dictates the length of the PWM
    // signal for that bit.
    //

    HAL_TIM_PWM_Start_DMA(&Ws2812Timer,
                          TIM_CHANNEL_1,
                          Ws2812PixelIo,
                          LED_BITS_PER_FRAME);

    return;
}

void
Ws2812ClearLeds (
    uint16_t Led,
    uint16_t Count
    )

/*++

Routine Description:

    This routine clears the given LEDs to unilluminated.

Arguments:

    Led - Supplies the LED index to clear.

    Count - Supplies the number of LEDs to clear.

Return Value:

    None.

--*/

{

    uint16_t EndIndex;
    uint16_t Index;

    EndIndex = (Led + Count) * BITS_PER_LED;
    for (Index = Led * BITS_PER_LED; Index < EndIndex; Index += 1) {
        Ws2812PixelIo[Index] = LED_BIT_LOW;
    }

    return;
}

void
Ws2812DisplayIp (
    uint32_t IpAddress,
    uint32_t Color
    )

/*++

Routine Description:

    This routine displays an IP address on the ice grid.

Arguments:

    IpAddress - Supplies the IP address to display.

    Color - Supplies the color to display it in.

Return Value:

    None.

--*/

{

    int Digit;
    int DigitIndex;
    int Index;
    int Integer;

    //
    // Display the IP address as a series of binary coded decimals.
    //

    for (Index = 0; Index < 4; Index += 1) {
        Ws2812ClearDisplay();
        HAL_Delay(500);
        Integer = IpAddress >> 24;
        IpAddress <<= 8;
        for (DigitIndex = 0; DigitIndex < 3; DigitIndex += 1) {
            Digit = Integer / 100;
            Integer = (Integer - (Digit * 100)) * 10;
            Ws2812OutputBinary(LED_COLUMNS * DigitIndex, 4, Digit, Color);
            HAL_Delay(500);
        }
    }

    Ws2812ClearDisplay();
    return;
}

void
Ws2812OutputBinary (
    uint16_t Led,
    uint16_t BitCount,
    uint16_t Value,
    uint32_t RgbColor
    )

/*++

Routine Description:

    This routine encodes a value in binary on the LED display.

Arguments:

    Led - Supplies the LED index to start from (highest bit).

    BitCount - Supplies the number of bits to display.

    Value - Supplies the hex value to display.

    RgbColor - Supplies the color to display the value in.

Return Value:

    None.

--*/

{

    uint16_t Bit;
    uint16_t LedIndex;

    for (Bit = 0; Bit < BitCount; Bit += 1) {
        LedIndex = Led + BitCount - 1 - Bit;
        if ((Value & (1 << Bit)) != 0) {
            Ws2812SetLed(LedIndex, RgbColor);

        } else {
            Ws2812SetLed(LedIndex, 0);
        }
    }

    return;
}

void
Ws2812SetLeds (
    uint16_t Led,
    uint32_t RgbColor,
    uint16_t Count
    )

/*++

Routine Description:

    This routine sets multiple LEDs to the given color.

Arguments:

    Led - Supplies the LED index to set.

    RgbColor - Supplies the color in ARGB format (where blue is in the 8 LSB).

    Count - Supplies the number of LEDs to set.

Return Value:

    None.

--*/

{

    uint16_t Index;

    for (Index = 0; Index < Count; Index += 1) {
        Ws2812SetLed(Led + Index, RgbColor);
    }

    return;
}

void
Ws2812SetLed (
    uint16_t Led,
    uint32_t RgbColor
    )

/*++

Routine Description:

    This routine sets an LED to the given color.

Arguments:

    Led - Supplies the LED index to set.

    RgbColor - Supplies the color in ARGB format (where blue is in the 8 LSB).

Return Value:

    None.

--*/

{

    uint16_t DataIndex;
    uint16_t Index;

    //
    // Set the green.
    //

    DataIndex = (Led * BITS_PER_LED) + 7;
    for (Index = 0; Index < 8; Index += 1) {
        if ((RgbColor & (1 << (8 + Index))) != 0) {
            Ws2812PixelIo[DataIndex - Index] = LED_BIT_HIGH;

        } else {
            Ws2812PixelIo[DataIndex - Index] = LED_BIT_LOW;
        }
    }

    //
    // Set the red.
    //

    DataIndex = (Led * BITS_PER_LED) + 15;
    for (Index = 0; Index < 8; Index += 1) {
        if ((RgbColor & (1 << (16 + Index))) != 0) {
            Ws2812PixelIo[DataIndex - Index] = LED_BIT_HIGH;

        } else {
            Ws2812PixelIo[DataIndex - Index] = LED_BIT_LOW;
        }
    }

    //
    // Set the blue.
    //

    DataIndex = (Led * BITS_PER_LED) + 23;
    for (Index = 0; Index < 8; Index += 1) {
        if ((RgbColor & (1 << (0 + Index))) != 0) {
            Ws2812PixelIo[DataIndex - Index] = LED_BIT_HIGH;

        } else {
            Ws2812PixelIo[DataIndex - Index] = LED_BIT_LOW;
        }
    }

    return;
}

void
HAL_TIM_PWM_PulseFinishedCallback (
    TIM_HandleTypeDef *Timer
    )

/*++

Routine Description:

    This routine is called by the HAL when a frame has finished transmitting.

Arguments:

    Timer - Supplies a pointer to the timer that control the PWM.

Return Value:

    None.

--*/

{

    //
    // Stop the timer, and re-arm the timer for a blanking period that signals
    // to the WS2812 that this is the end of the frame.
    //

    HAL_TIM_PWM_Stop_DMA(&Ws2812Timer, TIM_CHANNEL_1);

    //
    // Set a blanking period of 55us (spec wants 50us).
    //

    Ws2812Timer.Init.Period = 3960;
    HAL_TIM_Base_Init(&Ws2812Timer);
    TIM4->CCR1 = 0;
    __HAL_TIM_CLEAR_IT(&Ws2812Timer, TIM_IT_UPDATE);
    HAL_TIM_Base_Start_IT(&Ws2812Timer);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
    return;
}

void
DMA1_Channel1_IRQHandler (
    void
    )

/*++

Routine Description:

    This routine represents the interrupt service routine for the DMA1 engine's
    Channel1 interrupt.

Arguments:

    None.

Return Value:

    None.

--*/

{

    HAL_DMA_IRQHandler(&Ws2812Dma);
    return;
}

void
TIM4_IRQHandler (
    void
    )

/*++

Routine Description:

    This routine represents the interrupt service routine for timer 4.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // This is called only when the blanking period has finished. It restarts
    // the whole process by kicking off the DMA again.
    //

    __HAL_TIM_CLEAR_IT(&Ws2812Timer, TIM_IT_UPDATE);
    HAL_TIM_Base_Stop_IT(&Ws2812Timer);

    //
    // Set the period back to 1.25us.
    //

    Ws2812Timer.Init.Period = 90;
    HAL_NVIC_DisableIRQ(TIM4_IRQn);
    HAL_TIM_PWM_Init(&Ws2812Timer);
    TIM4->CNT = 0;
    HAL_TIM_PWM_Start_DMA(&Ws2812Timer,
                          TIM_CHANNEL_1,
                          Ws2812PixelIo,
                          LED_BITS_PER_FRAME);

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

