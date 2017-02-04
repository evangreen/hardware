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

//
// Define the base in flash (non-volatile) where the credentials are stored.
// Pick something up near the end of the device to minimize the chance that
// code will conflict with it.
//

#define FLASH_CREDENTIALS_ADDRESS 0x0800F800

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

#define ESP8266_CONNECTION_REJECT_PAGE \
    "<html>" \
    "<head></head>" \
    "<body>" \
    "<h3>Something went wrong! Problem %d.</h3>" \
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
// Define the UART TX buffer size. Used only for debugging. This must be a
// power of two.
//

#define UART_TX_SIZE 512
#define UART_TX_MASK 0x1FF

//
// Define the size of the SSID and password fields for the client connection.
//

#define ESP8266_CREDENTIAL_SIZE 64

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
Esp8266GetIp (
    char *Command,
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

int
Esp8266GetPostParameter (
    char *PostData,
    char *PostEnd,
    char *Field,
    char *Data,
    uint32_t DataSize
    );

int
Esp8266UrlDecode (
    char *PostData,
    char *PostEnd,
    char *Data,
    uint32_t DataSize
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

int
FlashProgram (
    void *FlashAddress,
    void *Data,
    uint32_t Size
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

char *Esp8266Ssid = (char *)FLASH_CREDENTIALS_ADDRESS;
char *Esp8266Password = \
    (char *)FLASH_CREDENTIALS_ADDRESS + ESP8266_CREDENTIAL_SIZE;

uint16_t *Esp8266CredentialsSum =
    (void *)FLASH_CREDENTIALS_ADDRESS + (ESP8266_CREDENTIAL_SIZE * 2);

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
// Save the transmit history for debugging purposes. If things are getting
// tight this (and its associated code) can be removed without consequence.
//

uint8_t UartTxBuffer[UART_TX_SIZE];
uint16_t UartTxIndex;

const FLASH_EraseInitTypeDef FlashEraseCommand = {
    .TypeErase = FLASH_TYPEERASE_PAGES,
    .Banks = FLASH_BANK_1,
    .PageAddress = FLASH_CREDENTIALS_ADDRESS,
    .NbPages = 1
};

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
    uint16_t NewSum;
    uint16_t OldSum;
    char *Password;
    char *Ssid;
    uint32_t Timeout;

    Password = Esp8266Password;
    Ssid = Esp8266Ssid;

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
        if (Esp8266GetIp("CIPAP?", &IpAddress) != 0) {
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
        OldSum = *Esp8266CredentialsSum;
        if ((Esp8266ChecksumCredentials(Ssid, Password) == OldSum) &&
            (Ssid[0] != '\0')) {

            CredentialsOk = 1;
            Color = LED_COLOR_YELLOW;
        }

        Esp8266IpAddress = IpAddress;
        Timeout = HAL_GetTick() + (WIFI_RECONFIGURE_TIMEOUT * 1000);
        while ((HAL_GetTick() <= Timeout) || (CredentialsOk == 0)) {
            Ws2812DisplayIp(IpAddress, Color);
            Esp8266GatherNewCredentials();

            //
            // If the credentials checksum changed, break out and go try it.
            //

            NewSum = *Esp8266CredentialsSum;
            if (NewSum != OldSum) {
                if ((Esp8266ChecksumCredentials(Ssid, Password) == NewSum) &&
                    (Ssid[0] != '\0')) {

                    break;
                }
            }
        }

        Ws2812OutputBinary(0, 1, 1, LED_COLOR_YELLOW);

        //
        // Attempt to connect using the credentials provided. Stop the server
        // first.
        //

        ErrorStep += 1;
        Esp8266SendCommand("CIPSERVER=0");
        if (Esp8266ReceiveOk() != 0) {
            goto ConfigureEnd;
        }

        //
        // Act as a client.
        //

        ErrorStep += 1;
        Esp8266SendCommand("CWMODE=1");
        if (Esp8266ReceiveOk() != 0) {
            goto ConfigureEnd;
        }

        //
        // Send the command to connect. Send it out piecemeal to avoid having
        // to allocate a line buffer.
        //

        ErrorStep += 1;
        UartTransmit("AT+CWJAP=\"", 10);
        UartTransmit(Ssid, LibStringLength(Ssid));
        UartTransmit("\",\"", 3);
        UartTransmit(Password, LibStringLength(Password));
        UartTransmit("\"\r\n", 3);

        //
        // The result looks something like:
        // busy p...
        // WIFI CONNECTED
        // WIFI GOT IP
        //
        // OK
        //
        // Wait a while for all those things to come in, hoping for an OK.
        //

        Timeout = HAL_GetTick() + (WIFI_CONNECT_TIMEOUT * 1000);
        CredentialsOk = 0;
        while (HAL_GetTick() <= Timeout) {
            if (Esp8266ReceiveOk() == 0) {
                CredentialsOk = 1;
                break;
            }
        }

        if (CredentialsOk == 0) {
            continue;
        }

        ErrorStep += 1;
        if (Esp8266GetIp("CIPSTA?", &IpAddress) != 0) {
            goto ConfigureEnd;
        }

        Ws2812DisplayIp(IpAddress, LED_COLOR_GREEN);
        break;
    }

    ErrorStep = 0;

ConfigureEnd:
    return ErrorStep;
}

void
Esp8266ServeUdpForever (
    void
    )

/*++

Routine Description:

    This routine receives UDP requests forever.

Arguments:

    None.

Return Value:

    None.

--*/

{

    char *Current;
    int32_t DataSize;
    char Line[160];
    int LineSize;

    //
    // Fire up a UDP connection. CIPSTART takes the form:
    // "TCP|UDP",<id>,<addr>,<remote port>,<local port>,<mode>
    // Mode 2 indicates that a response will be sent to whichever port was
    // received from last.
    //

    Esp8266SendCommand("CIPSTART=0,\"UDP\",\"0.0.0.0\",8080,8080,2");
    Esp8266ReceiveOk();
    while (1) {
        LineSize = Esp8266ReceiveLine(Line, sizeof(Line) - 1);
        if (LineSize < 8) {
            continue;
        }

        if ((LibStringCompare(Line, "+IPD,", 5) == 0) &&
            ((Line[5] >= '0') && (Line[5] <= '9')) &&
            (Line[6] == ',')) {

            Current = Line + 7;
            DataSize = LibScanInt(&Current);
            if (*Current == ':') {
                Current += 1;
            }

            if (Current + DataSize > &(Line[LineSize])) {
                DataSize = &(Line[LineSize]) - Current;
            }

            Current[DataSize] = '\0';
            IceGridProcessData(Current);
        }
    }

    return;
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
Esp8266GetIp (
    char *Command,
    uint32_t *IpAddress
    )

/*++

Routine Description:

    This routine reads the AP station or client IP address.

Arguments:

    Command - Supplies the command to send, including the question mark. Valid
        values are CIPAP? and CIPSTA?

    IpAddress - Supplies a pointer where the IP address will be returned.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    uint32_t Length;
    char Line[80];

    *IpAddress = 0;

    Length = LibStringLength(Command);
    Esp8266SendCommand(Command);

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

    if (Esp8266ReceiveLine(Line, sizeof(Line)) <= Length + 5) {
        return -1;
    }

    if (Esp8266ReadIpAddress(Line + Length + 5, IpAddress) != 0) {
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

    const char *PasswordEnd;
    const char *SsidEnd;
    uint16_t Sum;

    SsidEnd = Ssid + ESP8266_CREDENTIAL_SIZE;
    PasswordEnd = Password + ESP8266_CREDENTIAL_SIZE;
    Sum = 0;
    while ((*Ssid != '\0') && (Ssid < SsidEnd)) {
        Sum += *Ssid;
        Ssid += 1;
    }

    while ((*Password != '\0') && (Password < PasswordEnd)) {
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
    int OpenConnections;
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
    // Service requests.
    //

    OpenConnections = 1;
    while (1) {
        if (Esp8266ReceiveLine(Line, sizeof(Line)) <= 0) {
            continue;
        }

        if (((Line[0] >= '0') && (Line[0] <= '9')) &&
            (LibStringCompare(Line + 1, ",CONNECT", 8) == 0)) {

            OpenConnections += 1;
            continue;
        }

        //
        // Break out if the connection was closed.
        //

        if (((Line[0] >= '0') && (Line[0] <= '9')) &&
            (LibStringCompare(Line + 1, ",CLOSED", 7) == 0)) {

            OpenConnections -= 1;
            if (OpenConnections == 0) {
                break;
            }
        }

        //
        // See if it was received data. Ignore anything else.
        //

        if ((LibStringCompare(Line, "+IPD,", 5) == 0) &&
            ((Line[5] >= '0') && (Line[5] <= '9')) &&
            (Line[6] == ',')) {

            Connection = Line[5];
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

    char *Current;
    char Character;
    uint16_t Checksum;
    char *End;
    uint32_t PageError;
    char Password[ESP8266_CREDENTIAL_SIZE];
    char Post[256];
    int Problem;
    char Ssid[ESP8266_CREDENTIAL_SIZE];
    int Status;

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

        //
        // Gather the post parameters. There's an extra blank line queued up
        // from the previous receive. And actually the post parameters don't
        // end in a CRLF so the receive line call will technically fail.
        //

        Problem = 1;
        Esp8266ReceiveLine(Post, sizeof(Post));
        Status = 1;
        Esp8266ReceiveLine(Post, sizeof(Post));
        if ((LibStringCompare(Post, "+IPD,", 5) == 0) &&
            (Post[5] == Connection) &&
            (Post[6] == ',')) {

            Current = Post + 7;
            DataLength = LibScanInt(&Current);
            if (*Current == ':') {
                Current += 1;
            }

            End = Current + DataLength;
            if (End > &(Post[sizeof(Post)])) {
                End = &(Post[sizeof(Post)]);
            }

            Status = Esp8266GetPostParameter(Current,
                                             End,
                                             "network",
                                             Ssid,
                                             ESP8266_CREDENTIAL_SIZE);

            Status |= Esp8266GetPostParameter(Current,
                                              End,
                                              "pw",
                                              Password,
                                              ESP8266_CREDENTIAL_SIZE);

            if (Status != 0) {
                Problem = 2;
            }

        } else {
            Problem = 3;
        }

        //
        // If it worked, save the credentials in flash.
        //

        if (Status == 0) {
            HAL_FLASH_Unlock();
            Status = HAL_FLASHEx_Erase(
                                  (FLASH_EraseInitTypeDef *)&FlashEraseCommand,
                                  &PageError);

            Status |= FlashProgram(Esp8266Ssid,
                                   Ssid,
                                   LibStringLength(Ssid) + 1);

            Status |= FlashProgram(Esp8266Password,
                                   Password,
                                   LibStringLength(Password) + 1);

            if (Status != 0) {
                Ws2812OutputBinary(0, 5, 1, LED_COLOR_CYAN);
                Problem = 4;
            }

            Checksum = Esp8266ChecksumCredentials(Esp8266Ssid, Esp8266Password);
            Status |= FlashProgram(Esp8266CredentialsSum,
                                   &Checksum,
                                   sizeof(uint16_t));

            if (Status != 0) {
                Ws2812OutputBinary(0, 5, 1, LED_COLOR_CYAN);
                Problem = 5;
            }

            HAL_FLASH_Lock();
        }

        if (Status == 0) {
            Esp8266SendHttpResponse(Connection, ESP8266_CONNECTION_ACCEPT_PAGE);

        } else {
            LibStringPrint(Post,
                           sizeof(Post),
                           ESP8266_CONNECTION_REJECT_PAGE,
                           Problem);

            Esp8266SendHttpResponse(Connection, Post);
        }

    } else {
        while (DataLength != 0) {
            UartReceive(&Character, 1);
            DataLength -= 1;
        }

        Esp8266Send404(Connection);
    }

    return;
}

int
Esp8266GetPostParameter (
    char *PostData,
    char *PostEnd,
    char *Field,
    char *Data,
    uint32_t DataSize
    )

/*++

Routine Description:

    This routine searches for a particular POST data element.

Arguments:

    PostData - Supplies a pointer to the POST data.

    PostEnd - Supplies the end of the POST data.

    Field - Supplies the member to search for.

    Data - Supplies a pointer where the decoded data will be returned on
        success.

    DataSize - Supplies the size of the data buffer.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    uint32_t FieldLength;

    FieldLength = LibStringLength(Field);
    while (PostData + FieldLength + 1 < PostEnd) {
        if (LibStringCompare(PostData, Field, FieldLength) != 0) {
            PostData += 1;
            continue;
        }

        PostData += FieldLength;
        if (*PostData != '=') {
            continue;
        }

        PostData += 1;
        return Esp8266UrlDecode(PostData, PostEnd, Data, DataSize);
    }

    return -1;
}

int
Esp8266UrlDecode (
    char *PostData,
    char *PostEnd,
    char *Data,
    uint32_t DataSize
    )

/*++

Routine Description:

    This routine URL decodes the given string.

Arguments:

    PostData - Supplies a pointer to the POST data.

    PostEnd - Supplies the end of the POST data.

    Data - Supplies a pointer where the decoded data will be returned on
        success.

    DataSize - Supplies the size of the data buffer.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    char *DataEnd;
    int Index;

    DataEnd = Data + DataSize - 1;
    while ((PostData < PostEnd) && (Data < DataEnd)) {

        //
        // Alphanumeric characters pass unmolested.
        //

        if (((*PostData >= 'A') && (*PostData <= 'Z')) ||
            ((*PostData >= 'a') && (*PostData <= 'z')) ||
            ((*PostData >= '0') && (*PostData <= '9'))) {

            *Data = *PostData;
            PostData += 1;

        //
        // Plus gets converted to space.
        //

        } else if (*PostData == '+') {
            *Data = ' ';
            PostData += 1;

        //
        // This is a percent encoded hex value: ie %2D.
        //

        } else if (*PostData == '%') {
            PostData += 1;
            *Data = 0;
            for (Index = 0; Index < 2; Index += 1) {
                *Data <<= 4;
                if ((*PostData >= 'A') && (*PostData <= 'F')) {
                    *Data |= *PostData - 'A' + 0xA;

                } else if ((*PostData >= 'a') && (*PostData <= 'f')) {
                    *Data |= *PostData - 'a' + 0xA;

                } else if ((*PostData >= '0') && (*PostData <= '9')) {
                    *Data |= *PostData - '0';

                } else {
                    return -1;
                }

                PostData += 1;
            }

        //
        // Other characters like '&' signify the start of the next field.
        //

        } else {
            break;
        }

        Data += 1;
    }

    *Data = '\0';
    return 0;
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
    int Result;

    while (1) {
        Result = Esp8266ReceiveLine(Buffer, 6);

        //
        // Allow for any number of empty lines before the OK.
        //

        if (Result == 0) {
            continue;
        }

        //
        // Anything but OK is bad.
        //

        if (Result != 2) {
            return 1;
        }

        if ((Buffer[0] != 'O') || (Buffer[1] != 'K')) {
            return 1;
        }

        break;
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

    const uint8_t *Bytes;
    uint16_t Index;

    //
    // This is really only used for debugging, but it's quite handy so I'm
    // leaving it in.
    //

    Bytes = Buffer;
    for (Index = 0; Index < Size; Index += 1) {
        UartTxBuffer[UartTxIndex & UART_TX_MASK] = Bytes[Index];
        UartTxIndex += 1;
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

int
FlashProgram (
    void *FlashAddress,
    void *Data,
    uint32_t Size
    )

/*++

Routine Description:

    This routine programs the flash at the given address.

Arguments:

    FlashAddress - Supplies a pointer to the flash address to program.

    Data - Supplies a pointer to the data to write.

    Size - Supplies the size of the data.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    uint8_t *DataBytes;
    uint32_t Value;
    int Status;

    DataBytes = Data;
    while (Size >= 2) {
        Value = DataBytes[0] | (DataBytes[1] << 8);
        Status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                                   (uint32_t)FlashAddress,
                                   Value);

        if (Status != HAL_OK) {
            return Status;
        }

        DataBytes += 2;
        FlashAddress += 2;
        Size -= 2;
    }

    if (Size != 0) {
        Status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                                   (uint32_t)FlashAddress,
                                   DataBytes[0]);

        if (Status != HAL_OK) {
            return Status;
        }
    }

    return 0;
}

