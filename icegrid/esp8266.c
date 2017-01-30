/*++

Copyright (c) 2017 Evan Green. All Rights Reserved.

Module Name:

    esp8266.c

Abstract:

    This module implements support for working with the ESP8266 WiFi module.

Author:

    Evan Green 21-Jan-2017

Environment:

    STM32

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "stm32f1xx_hal.h"
#include "icegrid.h"

//
// --------------------------------------------------------------------- Macros
//

#define ESP8266_HTTP_OK "HTTP/1.1 200 OK\r\n\r\n"
#define ESP8266_HTTP_OK_SIZE 19
#define ESP8266_HTTP_404 "HTTP/1.1 404 Not Found\r\n\r\n"
#define ESP8266_HTTP_404_SIZE 26
#define ESP8266_CONNECTION_PAGE \
    "<html>" \
    "<head></head>" \
    "<body>" \
    "<h3>Connect to a Wireless Network:</h3>" \
    "<form action=\"/connect/\" method=\"post\">" \
    "Network: <input id=\"network\" name=\"network\" type=\"text\" />" \
    "<br>" \
    "Password: <input id=\"pw\" name=\"pw\" type=\"text\" />" \
    "<br>" \
    "<input type=\"submit\" value=\"Connect\" />" \
    "</form>" \
    "</body>" \
    "</html>" \

#define ESP8266_CONNECTION_ACCEPT_PAGE \
    "<html>" \
    "<head></head>" \
    "<body>" \
    "<h3>Ok!</h3>" \
    "</body>" \
    "</html>" \

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the amount of time in milliseconds to wait for a UART I/O to go
// through.
//

#define ESP8266_UART_TIMEOUT 500

//
// Define the power down GPIO and pin.
//

#define ESP8266_GPIO GPIOA
#define ESP8266_TX_PIN GPIO_PIN_9
#define ESP8266_RX_PIN GPIO_PIN_10
#define ESP8266_PD_PIN GPIO_PIN_11

//
// Define the UART RX buffer size. This must be a power of two.
//

#define UART_RX_SIZE 512
#define UART_RX_MASK 0x1FF

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _HTTP_REQUEST_TYPE {
    HttpRequestInvalid,
    HttpRequestGet,
    HttpRequestPost
} HTTP_REQUEST_TYPE, *PHTTP_REQUEST_TYPE;

//
// ----------------------------------------------- Internal Function Prototypes
//

int
Esp8266Reset (
    void
    );

int
Esp8266GetApIp (
    uint32_t *IpAddress
    );

int
Esp8266ReadIpAddress (
    char *String,
    uint32_t *IpAddress
    );

uint16_t
Esp8266ChecksumCredentials (
    const char *Ssid,
    const char *Password
    );

void
Esp8266GatherNewCredentials (
    void
    );

void
Esp8266HandleHttpRequest (
    char Connection,
    HTTP_REQUEST_TYPE RequestType,
    const char *Uri,
    int32_t DataLength
    );

void
Esp8266SendHttpResponse (
    char Connection,
    const char *Response
    );

void
Esp8266Send404 (
    char Connection
    );

void
Esp8266SendHttpResponseData (
    char Connection,
    const char *Header,
    uint32_t HeaderSize,
    const char *Data,
    uint32_t DataSize
    );

void
Esp8266SendCommand (
    const char *Command
    );

int
Esp8266ReceiveOk (
    void
    );

int
Esp8266ReceiveLine (
    char *Buffer,
    uint16_t Size
    );

void
UartTransmit (
    const void *Buffer,
    uint16_t Size
    );

uint16_t
UartReceive (
    void *Buffer,
    uint16_t Size
    );

int
UartRxDataReady (
    void
    );

void
UartClearRxBuffer (
    void
    );

//
// -------------------------------------------------------------------- Globals
//

UART_HandleTypeDef Esp8266Uart = {
    .Instance = USART1,
    .Init = {
        .BaudRate = 115200,
        .WordLength = UART_WORDLENGTH_8B,
        .StopBits = UART_STOPBITS_1,
        .Parity = UART_PARITY_NONE,
        .Mode = UART_MODE_TX_RX,
        .HwFlowCtl = UART_HWCONTROL_NONE,
        .OverSampling = UART_OVERSAMPLING_16
    },
};

//
// The power down pin, which is just wired to high to make it always function.
//

const GPIO_InitTypeDef Esp8266Pd = {
    .Pin = ESP8266_PD_PIN,
    .Mode = GPIO_MODE_OUTPUT_PP,
    .Pull = GPIO_NOPULL,
    .Speed = GPIO_SPEED_FREQ_LOW
};

const GPIO_InitTypeDef Esp8266TxPin = {
    .Pin = ESP8266_TX_PIN,
    .Mode = GPIO_MODE_AF_PP,
    .Pull = GPIO_NOPULL,
    .Speed = GPIO_SPEED_FREQ_HIGH
};

const GPIO_InitTypeDef Esp8266RxPin = {
    .Pin = ESP8266_RX_PIN,
    .Mode = GPIO_MODE_AF_INPUT,
    .Pull = GPIO_PULLUP,
    .Speed = GPIO_SPEED_FREQ_HIGH
};

const char Esp8266Ssid[64];
const char Esp8266Password[64];
const uint16_t Esp8266CredentialsSum;

//
// Store the AP or client IP, for debugging.
//

uint32_t Esp8266IpAddress;

//
// Define the circular receive buffer.
//

volatile uint8_t UartRxBuffer[UART_RX_SIZE];
volatile uint16_t UartRxProducer;
volatile uint16_t UartRxConsumer;
volatile uint16_t UartErrors;

//
// TODO: Remove this.
//

uint8_t EvanSend[512];
uint16_t EvanSendIndex;

//
// ------------------------------------------------------------------ Functions
//

void
Esp8622Initialize (
    void
    )

/*++

Routine Description:

    This routine initializes the ESP8266.

Arguments:

    None.

Return Value:

    None.

--*/

{

    __HAL_RCC_GPIOA_CLK_ENABLE();
    HAL_GPIO_Init(ESP8266_GPIO, (GPIO_InitTypeDef *)&Esp8266Pd);
    HAL_GPIO_WritePin(ESP8266_GPIO, ESP8266_PD_PIN, GPIO_PIN_SET);
    HAL_GPIO_Init(ESP8266_GPIO, (GPIO_InitTypeDef *)&Esp8266TxPin);
    HAL_GPIO_Init(ESP8266_GPIO, (GPIO_InitTypeDef *)&Esp8266RxPin);
    __HAL_RCC_USART1_CLK_ENABLE();

    //
    // Enable the UART and RX interrupts.
    //

    HAL_UART_Init(&Esp8266Uart);
    __HAL_UART_ENABLE_IT(&Esp8266Uart, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&Esp8266Uart, UART_IT_PE);
    __HAL_UART_ENABLE_IT(&Esp8266Uart, UART_IT_ERR);
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    Esp8266Reset();
    return;
}

uint32_t
Esp8266Configure (
    void
    )

/*++

Routine Description:

    This routine performs wireless network configuration. It starts by acting
    as an AP for 60 seconds, allowing someone to connect to it and configure
    wifi credentials. If credentials have been configured or there are
    previously saved credentials, it connects as a wifi client with those.
    Otherwise it stays in AP mode and waits to be configured.

Arguments:

    None.

Return Value:

    0 on success.

    Returns the step number on which an error occurred if a failure
    occurred.

--*/

{

    uint32_t Color;
    int CredentialsOk;
    uint32_t ErrorStep;
    uint32_t IpAddress;
    uint16_t OldSum;
    uint32_t Timeout;

    //
    // Loop until valid wireless credentials are given.
    //

    while (1) {

        //
        // Enter AP mode.
        //

        ErrorStep = 1;
        Esp8266SendCommand("CWMODE=2");
        if (Esp8266ReceiveOk() != 0) {
            goto ConfigureEnd;
        }

        //
        // Set the SSID info.
        //

        ErrorStep += 1;
        Esp8266SendCommand("CWSAP=\"" WIFI_BSSID "\","",11,0");
        if (Esp8266ReceiveOk() != 0) {
            goto ConfigureEnd;
        }

        ErrorStep += 1;
        if (Esp8266GetApIp(&IpAddress) != 0) {
            goto ConfigureEnd;
        }

        //
        // Go to multi-connection mode.
        //

        ErrorStep += 1;
        Esp8266SendCommand("CIPMUX=1");
        if (Esp8266ReceiveOk() != 0) {
            goto ConfigureEnd;
        }

        //
        // Start the server.
        //

        Esp8266SendCommand("CIPSERVER=1,80");
        if (Esp8266ReceiveOk() != 0) {
            goto ConfigureEnd;
        }

        ErrorStep += 1;
        Color = LED_COLOR_RED;
        CredentialsOk = 0;
        OldSum = (volatile uint16_t)Esp8266CredentialsSum;
        if ((Esp8266ChecksumCredentials(Esp8266Ssid, Esp8266Password) ==
             Esp8266CredentialsSum) &&
            (Esp8266Ssid[0] != '\0')) {

            CredentialsOk = 1;
            Color = LED_COLOR_GREEN;
        }

        Esp8266IpAddress = IpAddress;
        Timeout = HAL_GetTick() + (WIFI_CONNECT_TIMEOUT * 1000);
        while ((HAL_GetTick() <= Timeout) || (CredentialsOk == 0)) {
            Ws2812DisplayIp(IpAddress, Color);
            Esp8266GatherNewCredentials();

            //
            // If the credentials checksum changed, break out and go try it.
            //

            if ((volatile uint16_t)Esp8266CredentialsSum != OldSum) {
                if ((Esp8266ChecksumCredentials(Esp8266Ssid, Esp8266Password) ==
                     Esp8266CredentialsSum) &&
                    (Esp8266Ssid[0] != '\0')) {

                    break;
                }
            }
        }

        //
        // Attempt to connect using the credentials provided.
        //

    }

    ErrorStep = 0;

ConfigureEnd:
    return ErrorStep;
}

void
USART1_IRQHandler (
    void
    )

/*++

Routine Description:

    This routine is called when the UART interrupt fires.

Arguments:

    None.

Return Value:

    None.

--*/

{

    uint32_t Flag;
    uint32_t Source;

    //
    // Check for a parity error.
    //

    Flag = __HAL_UART_GET_FLAG(&Esp8266Uart, UART_FLAG_PE);
    Source = __HAL_UART_GET_IT_SOURCE(&Esp8266Uart, UART_IT_PE);
    if((Flag != RESET) && (Source != RESET)) {
        Esp8266Uart.ErrorCode |= HAL_UART_ERROR_PE;
    }

    //
    // Check for a frame error.
    //

    Flag = __HAL_UART_GET_FLAG(&Esp8266Uart, UART_FLAG_FE);
    Source = __HAL_UART_GET_IT_SOURCE(&Esp8266Uart, UART_IT_ERR);
    if((Flag != RESET) && (Source != RESET)) {
        Esp8266Uart.ErrorCode |= HAL_UART_ERROR_FE;
    }

    //
    // Check for a noise error.
    //

    Flag = __HAL_UART_GET_FLAG(&Esp8266Uart, UART_FLAG_NE);
    if((Flag != RESET) && (Source != RESET)) {
        Esp8266Uart.ErrorCode |= HAL_UART_ERROR_NE;
    }

    //
    // Check for an RX overrun error.
    //

    Flag = __HAL_UART_GET_FLAG(&Esp8266Uart, UART_FLAG_ORE);
    if((Flag != RESET) && (Source != RESET)) {
        Esp8266Uart.ErrorCode |= HAL_UART_ERROR_ORE;
    }

    //
    // Clear all the UART errors.
    //

    if(Esp8266Uart.ErrorCode != HAL_UART_ERROR_NONE) {
        __HAL_UART_CLEAR_PEFLAG(&Esp8266Uart);
    }

    //
    // If there's data, shove it in the circular buffer.
    //

    Flag = __HAL_UART_GET_FLAG(&Esp8266Uart, UART_FLAG_RXNE);
    Source = __HAL_UART_GET_IT_SOURCE(&Esp8266Uart, UART_IT_RXNE);
    if((Flag != RESET) && (Source != RESET)) {
        if (UartRxProducer + 1 == UartRxConsumer) {
            Esp8266Uart.ErrorCode |= HAL_UART_ERROR_ORE;

        } else {
            UartRxBuffer[UartRxProducer & UART_RX_MASK] =
                                                      Esp8266Uart.Instance->DR;

            UartRxProducer += 1;
        }
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

int
Esp8266Reset (
    void
    )

/*++

Routine Description:

    This routine resets the ESP8266.

Arguments:

    None.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    Esp8266SendCommand("RST");
    HAL_Delay(500);
    UartTransmit("ATE0\r\n", 6);
    HAL_Delay(100);
    UartClearRxBuffer();
    return Esp8266ReceiveOk();
}

int
Esp8266GetApIp (
    uint32_t *IpAddress
    )

/*++

Routine Description:

    This routine reads the AP station IP address.

Arguments:

    IpAddress - Supplies a pointer where the IP address will be returned.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    char Line[80];

    *IpAddress = 0;

    Esp8266SendCommand("CIPAP?");

    //
    // The response looks like this:
    // +CIPAP:ip:"192.168.4.1"
    // +CIPAP:gateway:"192.168.4.1"
    // +CIPAP:netmask:"255.255.255.0"
    //
    // OK
    //
    // Start by receiving the IP address.
    //

    if (Esp8266ReceiveLine(Line, sizeof(Line)) <= 11) {
        return -1;
    }

    if (Esp8266ReadIpAddress(Line + 11, IpAddress) != 0) {
        return -1;
    }

    //
    // Now read and ignore the gateway and netmask.
    //

    if (Esp8266ReceiveLine(Line, sizeof(Line)) <= 0) {
        return -1;
    }

    if (Esp8266ReceiveLine(Line, sizeof(Line)) <= 0) {
        return -1;
    }

    if (Esp8266ReceiveOk() != 0) {
        return -1;
    }

    return 0;
}

int
Esp8266ReadIpAddress (
    char *String,
    uint32_t *IpAddress
    )

/*++

Routine Description:

    This routine reads an IP address string and converts it to a network order
    integer.

Arguments:

    String - Supplies a pointer to the IP address string.

    IpAddress - Supplies a pointer where the IP address will be returned.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    char *Before;
    int32_t Byte;
    int Index;

    *IpAddress = 0;
    for (Index = 0; Index < 4; Index += 1) {
        Before = String;
        Byte = LibScanInt(&String);
        if ((Byte < 0) || (Byte > 255) || (String == Before)) {
            return -1;
        }

        if ((Index != 3) && (*String != '.')) {
            return -1;
        }

        String += 1;
        *IpAddress = (*IpAddress << 8) | Byte;
    }

    return 0;
}

uint16_t
Esp8266ChecksumCredentials (
    const char *Ssid,
    const char *Password
    )

/*++

Routine Description:

    This routine checksums the SSID and password, simply summing all the bytes.

Arguments:

    Ssid - Supplies a pointer to the network name to sum.

    Password - Supplies a pointer to the password to sum.

Return Value:

    Returns the sum of all the bytes.

--*/

{

    uint16_t Sum;

    Sum = 0;
    while (*Ssid != '\0') {
        Sum += *Ssid;
        Ssid += 1;
    }

    while (*Password != '\0') {
        Sum += *Password;
        Password += 1;
    }

    return Sum;
}

void
Esp8266GatherNewCredentials (
    void
    )

/*++

Routine Description:

    This routine serves a web requests aimed toward gathering the credentials
    needed to connect to a wireless network.

Arguments:

    None.

Return Value:

    None.

--*/

{

    char Connection;
    char *Current;
    int32_t DataLength;
    char *End;
    char Line[120];
    HTTP_REQUEST_TYPE RequestType;

    //
    // Try to get a new connection.
    //

    if (Esp8266ReceiveLine(Line, sizeof(Line)) <= 0) {
        return;
    }

    if ((Line[0] < '0') || (Line[0] > '9') ||
        (LibStringCompare(Line + 1, ",CONNECT", 8) != 0)) {

        return;
    }

    Connection = Line[0];

    //
    // Service requests from the connection.
    //

    while (1) {
        if (Esp8266ReceiveLine(Line, sizeof(Line)) <= 0) {
            continue;
        }

        //
        // Break out if the connection was closed.
        //

        if ((Line[0] == Connection) &&
            (LibStringCompare(Line + 1, ",CLOSED", 7) == 0)) {

            break;
        }

        //
        // See if it was received data. Ignore anything else.
        //

        if ((LibStringCompare(Line, "+IPD,", 5) == 0) &&
            (Line[5] == Connection) &&
            (Line[6] == ',')) {

            Current = Line + 7;
            DataLength = LibScanInt(&Current);
            if (*Current == ':') {
                Current += 1;
            }

            //
            // Everything after the colon counts towards the data length, and
            // there's also that CRLF that was stripped off by the function
            // receiving the line.
            //

            DataLength -= LibStringLength(Current) + 2;
            RequestType = HttpRequestInvalid;
            if (LibStringCompare(Current, "GET ", 4) == 0) {
                RequestType = HttpRequestGet;
                Current += 4;

            } else if (LibStringCompare(Current, "POST ", 5) == 0) {
                RequestType = HttpRequestPost;
                Current += 5;
            }

            if ((RequestType == HttpRequestGet) ||
                (RequestType == HttpRequestPost)) {

                End = Current;
                while ((*End != ' ') && (*End != '\0')) {
                    End += 1;
                }

                *End = '\0';
                Esp8266HandleHttpRequest(Connection,
                                         RequestType,
                                         Current,
                                         DataLength);

            } else {
                while (DataLength != 0) {
                    UartReceive(&(Line[0]), 1);
                    DataLength -= 1;
                }
            }
        }
    }

    return;
}

void
Esp8266HandleHttpRequest (
    char Connection,
    HTTP_REQUEST_TYPE RequestType,
    const char *Uri,
    int32_t DataLength
    )

/*++

Routine Description:

    This routine handles a GET web server request.

Arguments:

    Connection - Supplies the connection number, as an ascii digit.

    Uri - Supplies a pointer to the URI being requested.

    DataLength - Supplies the number of bytes of data.

Return Value:

    None.

--*/

{

    char Character;

    if ((RequestType == HttpRequestGet) &&
        (LibStringCompare(Uri, "/", 2) == 0)) {

        while (DataLength != 0) {
            UartReceive(&Character, 1);
            DataLength -= 1;
        }

        Esp8266SendHttpResponse(Connection, ESP8266_CONNECTION_PAGE);

    } else if ((RequestType == HttpRequestGet) &&
               (LibStringCompare(Uri, "/test/", 6) == 0)) {

        while (DataLength != 0) {
            UartReceive(&Character, 1);
            DataLength -= 1;
        }

        Esp8266SendHttpResponse(Connection, ESP8266_CONNECTION_ACCEPT_PAGE);

    } else if ((RequestType == HttpRequestPost) &&
               (LibStringCompare(Uri, "/connect/", 10) == 0)) {

        while (DataLength != 0) {
            UartReceive(&Character, 1);
            DataLength -= 1;
        }

        Esp8266SendHttpResponse(Connection, ESP8266_CONNECTION_ACCEPT_PAGE);

    } else {

        while (DataLength != 0) {
            UartReceive(&Character, 1);
            DataLength -= 1;
        }

        Esp8266Send404(Connection);
    }

    return;
}

void
Esp8266SendHttpResponse (
    char Connection,
    const char *Response
    )

/*++

Routine Description:

    This routine sends an HTTP response to the client.

Arguments:

    Connection - Supplies the connection number, as an ascii digit.

    Response - Supplies a pointer to the null terminated response string.

Return Value:

    None.

--*/

{

    Esp8266SendHttpResponseData(Connection,
                                ESP8266_HTTP_OK,
                                ESP8266_HTTP_OK_SIZE,
                                Response,
                                LibStringLength(Response));

    return;
}

void
Esp8266Send404 (
    char Connection
    )

/*++

Routine Description:

    This routine sends an HTTP 404 not found response.

Arguments:

    Connection - Supplies the connection number, as an ascii digit.

Return Value:

    None.

--*/

{

    Esp8266SendHttpResponseData(Connection,
                                ESP8266_HTTP_404,
                                ESP8266_HTTP_404_SIZE,
                                NULL,
                                0);

    return;
}

void
Esp8266SendHttpResponseData (
    char Connection,
    const char *Header,
    uint32_t HeaderSize,
    const char *Data,
    uint32_t DataSize
    )

/*++

Routine Description:

    This routine sends an HTTP response to the client.

Arguments:

    Connection - Supplies the connection number, as an ascii digit.

    Header - Supplies a pointer to the HTTP response header to send.

    HeaderSize - Supplies the size of the HTTP header, not including the null
        terminator.

    Data - Supplies a pointer to the response data.

    DataSize - Supplies the size of the HTTP response data, not including the
        null terminator.

Return Value:

    None.

--*/

{

    char Caret;
    char Line[40];
    int Status;

    LibStringPrint(Line,
                   sizeof(Line),
                   "CIPSEND=%c,%d",
                   Connection,
                   HeaderSize + DataSize);

    Esp8266SendCommand(Line);
    Esp8266ReceiveOk();

    //
    // There should be a caret coming down the pipe.
    //

    UartReceive(&Caret, 1);
    UartTransmit(Header, HeaderSize);
    if (DataSize != 0) {
        UartTransmit(Data, DataSize);
    }

    //
    // Receive the SEND OK response.
    //

    do {
        Status = Esp8266ReceiveLine(Line, sizeof(Line));

    } while ((Status >= 0) && (LibStringCompare(Line, "SEND OK", 8) != 0));

    //
    // Close the connection.
    //

    LibStringPrint(Line, sizeof(Line), "CIPCLOSE=%c", Connection);
    Esp8266SendCommand(Line);
    Esp8266ReceiveOk();
    return;
}

void
Esp8266SendCommand (
    const char *Command
    )

/*++

Routine Description:

    This routine sends a command out of the UART.

Arguments:

    Command - Supplies a pointer to the null terminated command to send.

Return Value:

    None.

--*/

{

    uint16_t Size;

    Size = 0;
    while (Command[Size] != '\0') {
        Size += 1;
    }

    UartTransmit("AT+", 3);
    UartTransmit(Command, Size);
    UartTransmit("\r\n", 2);
    return;
}

int
Esp8266ReceiveOk (
    void
    )

/*++

Routine Description:

    This routine attempts to receive an OK response.

Arguments:

    None.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    char Buffer[6];

    //
    // First receive and empty line.
    //

    if (Esp8266ReceiveLine(Buffer, 6) != 0) {
        return 1;
    }

    //
    // Then get the OK line.
    //

    if (Esp8266ReceiveLine(Buffer, 6) != 2) {
        return 1;
    }

    if ((Buffer[0] != 'O') || (Buffer[1] != 'K')) {
        return 1;
    }

    return 0;
}

int
Esp8266ReceiveLine (
    char *Buffer,
    uint16_t Size
    )

/*++

Routine Description:

    This routine attempts to receive a string of text, terminated by a CRLF.

Arguments:

    Buffer - Supplies a pointer to the response to receive.

    Size - Supplies the maximum size that can be stored in the buffer.
        Additional data will be read from the UART but discarded.

Return Value:

    Returns the size of the line on success. The ending CRLF will not be in
    the buffer.

    -1 on failure.

--*/

{

    uint8_t Character;
    uint16_t RxSize;

    RxSize = 0;
    while (1) {
        if (UartReceive(&Character, 1) == 0) {
            return -1;
        }

        if (Character == '\r') {
            UartReceive(&Character, 1);
            break;
        }

        if (RxSize < Size - 1) {
            Buffer[RxSize] = Character;
        }

        RxSize += 1;
    }

    Buffer[RxSize] = '\0';
    return RxSize;
}

void
UartTransmit (
    const void *Buffer,
    uint16_t Size
    )

/*++

Routine Description:

    This routine transmits out of the UART.

Arguments:

    Buffer - Supplies a pointer to the buffer to transmit.

    Size - Supplies the number of bytes to transmit.

Return Value:

    None.

--*/

{

    //
    // TODO: Remove this.
    //

    {

        const uint8_t *Bytes;
        uint16_t Index;

        Bytes = Buffer;
        for (Index = 0; Index < Size; Index += 1) {
            EvanSend[EvanSendIndex & 0x1FF] = Bytes[Index];
            EvanSendIndex += 1;
        }
    }

    HAL_UART_Transmit(&Esp8266Uart,
                      (uint8_t *)Buffer,
                      Size,
                      ESP8266_UART_TIMEOUT);

    return;
}

uint16_t
UartReceive (
    void *Buffer,
    uint16_t Size
    )

/*++

Routine Description:

    This routine receives data from the UART.

Arguments:

    Buffer - Supplies a pointer to the buffer where the received data will be
        returned.

    Size - Supplies the number of bytes to receive.

Return Value:

    Returns the number of bytes read.

--*/

{

    uint8_t *Bytes;
    uint16_t Count;
    uint32_t Timeout;

    Bytes = Buffer;
    Count = 0;
    Timeout = HAL_GetTick() + ESP8266_UART_TIMEOUT;
    while ((Count != Size) && (HAL_GetTick() <= Timeout)) {
        if (UartRxProducer != UartRxConsumer) {
            Bytes[Count] = UartRxBuffer[UartRxConsumer & UART_RX_MASK];
            Count += 1;
            UartRxConsumer += 1;
        }
    }

    return Count;
}

int
UartRxDataReady (
    void
    )

/*++

Routine Description:

    This routine returns the number of bytes ready to be received from the
    UART.

Arguments:

    None.

Return Value:

    Returns the number of bytes available to receive.

--*/

{

    return UartRxProducer - UartRxConsumer;
}

void
UartClearRxBuffer (
    void
    )

/*++

Routine Description:

    This routine clears all the received receive data so far, and any errors.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UartRxConsumer = UartRxProducer;
    Esp8266Uart.ErrorCode = HAL_UART_ERROR_NONE;
    return;
}

