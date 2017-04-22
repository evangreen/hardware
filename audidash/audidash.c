/*++

Copyright (c) 2017 Evan Green. All Rights Reserved.

Module Name:

    audidash.c

Abstract:

    This module implements the firmware for the Audi A4 dashboard controller.

Author:

    Evan Green 4-Apr-2017

Environment:

    STM32F103C8T6 firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "stm32f1xx_hal.h"
#include "audidash.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define port A pins.
//

#define DASHA_OIL_WARNING GPIO_PIN_3
#define DASHA_COOLANT_WARNING GPIO_PIN_4
#define DASHA_AIRBAG GPIO_PIN_5
#define DASHA_ABS GPIO_PIN_6
#define DASHA_HEADLIGHTS GPIO_PIN_9

#define DASHA_DEFAULT_SET \
    (DASHA_COOLANT_WARNING | DASHA_AIRBAG | DASHA_ABS)

//
// Define port B pins.
//

#define DASHB_CHARGE_WARNING GPIO_PIN_3
#define DASHB_CHECK_ENGINE GPIO_PIN_4
#define DASHB_ESP GPIO_PIN_5
#define DASHB_TAILGATE GPIO_PIN_6
#define DASHB_BRAKE_PAD GPIO_PIN_7
#define DASHB_PARKING_BRAKE GPIO_PIN_8
#define DASHB_EPC GPIO_PIN_9
#define DASHB_IGNITION GPIO_PIN_12
#define DASHB_TURN_RIGHT GPIO_PIN_13
#define DASHB_TURN_LEFT GPIO_PIN_14
#define DASHB_HIGH_BEAM GPIO_PIN_15

#define DASHB_DEFAULT_SET \
    (DASHB_BRAKE_PAD | DASHB_IGNITION)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _AUDIDASH_PARAMETER {
    DashPortA,
    DashPortB,
    DashSpeed,
    DashRpm,
    DashFuel,
    DashOil,
    DashTemp,
    DashParameterCount
} AUDIDASH_PARAMETER, *PAUDIDASH_PARAMETER;

//
// ----------------------------------------------- Internal Function Prototypes
//

void
AudiDashSetParameters (
    uint32_t Parameters[DashParameterCount]
    );

//
// -------------------------------------------------------------------- Globals
//

volatile long MyGlobal = 0x12348888;

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
// Timer 3 controls the PWM for the fuel, coolant, and oil gauges.
//

TIM_HandleTypeDef Timer3 = {
    .Instance = TIM3,
    .Init = {
        .Prescaler = 0, // Timer runs at 72MHz
        .CounterMode = TIM_COUNTERMODE_UP,
        .Period = 0xFFF,
        .ClockDivision = TIM_CLOCKDIVISION_DIV1,
        .RepetitionCounter = 0,
    },
};

const TIM_OC_InitTypeDef Timer3Pwm = {
    .OCMode = TIM_OCMODE_PWM1,
    .Pulse = 0xA0,
    .OCPolarity = TIM_OCPOLARITY_HIGH,
    .OCNPolarity = TIM_OCNPOLARITY_HIGH,
    .OCFastMode = TIM_OCFAST_DISABLE,
    .OCIdleState = TIM_OCIDLESTATE_RESET,
    .OCNIdleState = TIM_OCNIDLESTATE_RESET
};

//
// Timer 1 creates +12V pulses for the RPM gauge.
//

TIM_HandleTypeDef Timer1 = {
    .Instance = TIM1,
    .Init = {
        .Prescaler = 1800 - 1, // Timer runs at 72MHz/1800 = 40kHz.
        .CounterMode = TIM_COUNTERMODE_UP,
        .Period = 0x12C, // About 4000 RPM empirically, what's with this math?
        .ClockDivision = TIM_CLOCKDIVISION_DIV1,
        .RepetitionCounter = 0,
    },
};

const TIM_OC_InitTypeDef Timer1Pwm = {
    .OCMode = TIM_OCMODE_PWM1,
    .Pulse = 30,
    .OCPolarity = TIM_OCPOLARITY_HIGH,
    .OCNPolarity = TIM_OCNPOLARITY_HIGH,
    .OCFastMode = TIM_OCFAST_DISABLE,
    .OCIdleState = TIM_OCIDLESTATE_RESET,
    .OCNIdleState = TIM_OCNIDLESTATE_RESET
};

//
// Timer 2 creates pulses to ground for the speedometer.
//

TIM_HandleTypeDef Timer2 = {
    .Instance = TIM2,
    .Init = {
        .Prescaler = 7200 - 1, // Timer runs at 72MHz/7200 = 10kHz.
        .CounterMode = TIM_COUNTERMODE_UP,
        .Period = 0x200, // About 10 mph.
        .ClockDivision = TIM_CLOCKDIVISION_DIV1,
        .RepetitionCounter = 0,
    },
};

const TIM_OC_InitTypeDef Timer2Pwm = {
    .OCMode = TIM_OCMODE_PWM1,
    .Pulse = 30,
    .OCPolarity = TIM_OCPOLARITY_HIGH,
    .OCNPolarity = TIM_OCNPOLARITY_HIGH,
    .OCFastMode = TIM_OCFAST_DISABLE,
    .OCIdleState = TIM_OCIDLESTATE_RESET,
    .OCNIdleState = TIM_OCNIDLESTATE_RESET
};

//
// Define the current state of affairs.
//

uint32_t DashParameters[DashParameterCount];

//
// Define the periods needed to get the tachometer point directly at the digits
// 0 through 7.
//

const uint16_t DashTachDigits[8] = {
    0x1400,
    0x4A0,
    0x254,
    0x190,
    0x12C,
    0xF1,
    0xC9,
    0xAC
};

//
// ------------------------------------------------------------------ Functions
//

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
    GPIO_InitTypeDef GpioPort;
    uint32_t IpAddress;

    HAL_Init();
    HAL_RCC_OscConfig((RCC_OscInitTypeDef *)&OscParameters);
    HAL_RCC_ClockConfig((RCC_ClkInitTypeDef *)&ClkParameters, FLASH_LATENCY_2);

    //
    // Fire up the GPIO pins.
    //

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();

    //
    // PB3 and PB4 are used, so disable JTAG to regain control of them.
    //

    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    //
    // Set up all the GPIO on Port C.
    //

    GpioPort.Mode = GPIO_MODE_OUTPUT_PP;
    GpioPort.Pull = GPIO_NOPULL;
    GpioPort.Speed = GPIO_SPEED_FREQ_HIGH;
    GpioPort.Pin = GPIO_PIN_13;
    HAL_GPIO_Init(GPIOC, &GpioPort);

    //
    // Set up the GPIO on port A.
    //

    GpioPort.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 |
                   GPIO_PIN_7 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 |
                   GPIO_PIN_15;

    HAL_GPIO_Init(GPIOA, &GpioPort);

    //
    // Set up the GPIO on port B.
    //

    GpioPort.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 |
                   GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_12 |
                   GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;

    HAL_GPIO_Init(GPIOB, &GpioPort);

    //
    // Set up the timer PWM output on port A.
    //

    GpioPort.Mode = GPIO_MODE_AF_PP;
    GpioPort.Pin = GPIO_PIN_8 | GPIO_PIN_7 | GPIO_PIN_2;
    HAL_GPIO_Init(GPIOA, &GpioPort);

    //
    // Set up the timer PWM outputs on port B, and USART.
    //

    GpioPort.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOB, &GpioPort);
    GpioPort.Pin = GPIO_PIN_11;
    GpioPort.Mode = GPIO_MODE_AF_INPUT;
    GpioPort.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GpioPort);

    //
    // Set up the timers for the fuel, temperature, and oil gauges.
    //

    HAL_TIM_PWM_Init(&Timer3);
    HAL_TIM_PWM_ConfigChannel(&Timer3,
                              (TIM_OC_InitTypeDef *)&Timer3Pwm,
                              TIM_CHANNEL_2);

    HAL_TIM_PWM_Start(&Timer3, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&Timer3,
                              (TIM_OC_InitTypeDef *)&Timer3Pwm,
                              TIM_CHANNEL_3);

    HAL_TIM_PWM_Start(&Timer3, TIM_CHANNEL_3);
    HAL_TIM_PWM_ConfigChannel(&Timer3,
                              (TIM_OC_InitTypeDef *)&Timer3Pwm,
                              TIM_CHANNEL_4);

    HAL_TIM_PWM_Start(&Timer3, TIM_CHANNEL_4);

    //
    // Set up the RPM timer.
    //

    HAL_TIM_PWM_Init(&Timer1);
    HAL_TIM_PWM_ConfigChannel(&Timer1,
                              (TIM_OC_InitTypeDef *)&Timer1Pwm,
                              TIM_CHANNEL_1);

    HAL_TIM_PWM_Start(&Timer1, TIM_CHANNEL_1);

    //
    // Set up the Speedometer timer.
    //

    HAL_TIM_PWM_Init(&Timer2);
    HAL_TIM_PWM_ConfigChannel(&Timer2,
                              (TIM_OC_InitTypeDef *)&Timer2Pwm,
                              TIM_CHANNEL_3);

    HAL_TIM_PWM_Start(&Timer2, TIM_CHANNEL_3);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
    AudiDashClearDisplay();
    do {
        Esp8622Initialize();
        Error = Esp8266Configure(&IpAddress);
        if (Error == 0) {
            break;
        }

        AudiDashClearDisplay();
        AudiDashOutputBinary(Error, LED_COLOR_RED);
        HAL_Delay(5000);

    } while (Error != 0);

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    Esp8266ServeUdpForever(IpAddress);
    while (1) {
        ;
    }
}

void
AudiDashDisplayIp (
    uint32_t IpAddress,
    uint32_t Color
    )

/*++

Routine Description:

    This routine displays an IP address on the dashboard.

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
    int Sum;

    //
    // Display the IP address as a series of binary coded decimals.
    //

    for (Index = 0; Index < 4; Index += 1) {
        AudiDashClearDisplay();
        if (Index != 0) {
            HAL_GPIO_WritePin(GPIOA, DASHA_HEADLIGHTS, GPIO_PIN_SET);
        }

        HAL_Delay(1500);
        HAL_GPIO_WritePin(GPIOA, DASHA_HEADLIGHTS, GPIO_PIN_RESET);
        Integer = IpAddress >> 24;
        IpAddress <<= 8;
        Sum = 0;
        for (DigitIndex = 0; DigitIndex < 3; DigitIndex += 1) {
            Digit = Integer / 100;
            Integer = (Integer - (Digit * 100)) * 10;

            //
            // Skip leading zeros by adding all the digits seen so far together,
            // and skipping the digit if the sum so far is zero. Don't skip the
            // last digit in case the whole value is zero.
            //

            Sum += Digit;
            if ((DigitIndex == 2) || (Sum != 0)) {
                AudiDashOutputBinary(Digit, Color);
                HAL_Delay(1500);
            }
        }
    }

    AudiDashClearDisplay();
    return;
}

void
AudiDashOutputBinary (
    uint16_t Value,
    uint32_t RgbColor
    )

/*++

Routine Description:

    This routine encodes a value in binary on the dashboard.

Arguments:

    Led - Supplies the LED index to start from (highest bit).

    BitCount - Supplies the number of bits to display.

    Value - Supplies the hex value to display.

    RgbColor - Supplies the color to display the value in.

Return Value:

    None.

--*/

{

    uint32_t SetPins;
    uint16_t TachValue;

    //
    // Turn off the left and right signals, which indicate the "color", and
    // Check Engine / ESP, which indicate the upper 2 bits of 5.
    //

    HAL_GPIO_WritePin(
        GPIOB,
        DASHB_TURN_LEFT | DASHB_TURN_RIGHT | DASHB_CHECK_ENGINE | DASHB_ESP,
        GPIO_PIN_RESET);

    SetPins = 0;
    if ((RgbColor & 0x1) != 0) {
        SetPins |= DASHB_TURN_LEFT;
    }

    if ((RgbColor & 0x2) != 0) {
        SetPins |= DASHB_TURN_RIGHT;
    }

    if ((Value & 0x08) != 0) {
        SetPins |= DASHB_ESP;
    }

    if ((Value & 0x10) != 0) {
        SetPins |= DASHB_CHECK_ENGINE;
    }

    HAL_GPIO_WritePin(GPIOB, SetPins, GPIO_PIN_SET);

    //
    // Set the tachometer to point at 0-7 for the lower three bits.
    //

    TachValue = DashTachDigits[Value & 0x7];
    TIM1->ARR = TachValue;
    if (TIM1->CNT >= TachValue) {
        TIM1->CNT = 0;
    }

    //
    // Tweak the oil warning to never be on. See the big comment in set
    // parameters for how this works.
    //

    if (TachValue < 0x320) {
        HAL_GPIO_WritePin(GPIOA, DASHA_OIL_WARNING, GPIO_PIN_SET);

    } else {
        HAL_GPIO_WritePin(GPIOA, DASHA_OIL_WARNING, GPIO_PIN_RESET);
    }

    return;
}

void
AudiDashClearDisplay (
    void
    )

/*++

Routine Description:

    This routine clears the dashboard display.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DashParameters[DashPortA] = DASHA_DEFAULT_SET;
    DashParameters[DashPortB] = DASHB_DEFAULT_SET;
    DashParameters[DashSpeed] = 0xFFFF;
    DashParameters[DashRpm] = 0x1400;
    DashParameters[DashFuel] = 0x68;
    DashParameters[DashTemp] = 0x78;
    DashParameters[DashOil] = 0xC0;
    AudiDashSetParameters(DashParameters);
    return;
}

void
AudiDashProcessData (
    char *Data
    )

/*++

Routine Description:

    This routine handles incoming requests to change the LEDs. Data takes the
    form of a comma separated list of hex values in text.
    Example: FF23AC,FF00FF,...,0\r\n. The list can end early, and the remaining
    values will be set to zero.

Arguments:

    Data - Supplies a pointer to the null-terminated request text.

Return Value:

    None.

--*/

{

    int Index;
    char *Previous;
    uint32_t Value;

    //
    // The format is hex integers (without leading 0x) for:
    // PortA,PortB,Speed,Rpm,Fuel,Oil,Temp
    //

    Previous = Data;
    for (Index = 0; Index < DashParameterCount; Index += 1) {
        Value = LibScanHexInt(&Data);
        if (Data == Previous) {
            break;
        }

        DashParameters[Index] = Value;
        if (*Data == ',') {
            Data += 1;
        }
    }

    AudiDashSetParameters(DashParameters);
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
AudiDashSetParameters (
    uint32_t Parameters[DashParameterCount]
    )

/*++

Routine Description:

    This routine enacts the given parameters on the I/O outputs for the
    dashboard.

Arguments:

    Parameters - Supplies a pointer to the dashboard parameters array.

Return Value:

    None.

--*/

{

    uint32_t Flip;

    //
    // Here's an interesting tidbit: whether or not the oil pressure indicator
    // is "good" or "bad" (eg lights up an indicator) depends on the RPM.
    // Here's the algorithm (RPM values in timer ticks at 40kHz):
    // 1) If the oil pressure pin is grounded, then anything <= 0xFA5 is
    //    considered nominal (no indicator).
    // 2) If the oil pressure pin is floating, then anything >= 0x320 is
    //    considered nominal (no indicator).
    // So for RPM values between [0x320, 0xFA5], there is no way to make the
    // oil warning indicator light up. Fine. To make the oil indicator
    // consistent, so that "set" is always "warning indicator", flip the value
    // if RPMs are below 0x320.
    //

    Flip = 0;
    if (Parameters[DashRpm] < 0x320) {
        Flip = DASHA_OIL_WARNING;
    }

    //
    // Here it is, controlling everything with a couple GPIO ports and some
    // timers.
    //

    GPIOA->ODR = Parameters[DashPortA] ^ Flip;
    GPIOB->ODR = Parameters[DashPortB];

    //
    // The speedometer is on port A2 == TIM2_CH3. The speedometer and
    // tachometer count ticks per second, so change the period. The comparison
    // ensures that the reload count didn't jump before the current count,
    // which would mean waiting for the counter to overflow 0xFFFF before
    // kicking back in if it weren't for the counter reset.
    //

    TIM2->ARR = Parameters[DashSpeed];
    if (TIM2->CNT >= Parameters[DashSpeed]) {
        TIM2->CNT = 0;
    }

    //
    // The tachometer is on port A8 == TIM1_CH1. The tach counts +12V pulses.
    //

    TIM1->ARR = Parameters[DashRpm];
    if (TIM1->CNT >= Parameters[DashRpm]) {
        TIM1->CNT = 0;
    }

    //
    // The fuel gauge is on port A7 == TIM3_CH2. These next three gauges are
    // supposed to be analog voltages, which are being approximated here by
    // a PWM and a capacitory. They all share the same period, but have
    // independent duty cycles to approximate different analog voltages.
    //

    TIM3->CCR2 = Parameters[DashFuel];

    //
    // The oil gauge is on port B1 == TIM3_CH4.
    //

    TIM3->CCR4 = Parameters[DashOil];

    //
    // The coolant temperature is on port B0 == TIM3_CH3.
    //

    TIM3->CCR3 = Parameters[DashTemp];
    return;
}

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

