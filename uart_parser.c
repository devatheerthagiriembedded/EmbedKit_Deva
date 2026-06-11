#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define UART_SOF                0xAAU

#define UART_MAX_PAYLOAD_SIZE   16U

#define UART_FRAME_OK           1

#define UART_FRAME_IN_PROGRESS  0

#define UART_CHECKSUM_ERROR     -1

#define UART_TIMEOUT_ERROR      -2

typedef enum
{
    WAIT_SOF = 0,

    WAIT_CMD,

    WAIT_LEN,

    WAIT_PAYLOAD,

    WAIT_CHECKSUM

} parser_state_t;

typedef struct
{
    parser_state_t state;

    uint8_t cmd;

    uint8_t len;

    uint8_t payload[UART_MAX_PAYLOAD_SIZE];

    uint8_t payload_count;

    uint8_t running_checksum;

    uint32_t last_timestamp_ms;

    uint32_t timeout_ms;

    uint32_t timeout_gap_ms;

} uart_parser_t;

void uart_parser_reset(uart_parser_t *parser);

void uart_parser_init(uart_parser_t *parser,
                      uint32_t timeout_ms);

int uart_parser_feed(uart_parser_t *parser,
                     uint8_t byte,
                     uint32_t timestamp_ms);

void run_test(const char *test_name,
              const uint8_t *frame,
              const uint32_t *timestamps,
              uint32_t frame_length,
              uint32_t timeout_ms);

void uart_parser_reset(uart_parser_t *parser)
{
    parser->state = WAIT_SOF;

    parser->cmd = 0U;
    parser->len = 0U;

    memset(parser->payload, 0, sizeof(parser->payload));

    parser->payload_count = 0U;

    parser->running_checksum = 0U;

    parser->last_timestamp_ms = 0U;
}

void uart_parser_init(uart_parser_t *parser,
                      uint32_t timeout_ms)
{
    parser->timeout_ms = timeout_ms;

    uart_parser_reset(parser);

    parser->timeout_gap_ms = 0U;
}

int uart_parser_feed(uart_parser_t *parser,
                     uint8_t byte,
                     uint32_t timestamp_ms)
{
    int timeout_occurred = 0;

    /* Timeout check */
    if ((parser->timeout_ms != 0U) &&
        (parser->state != WAIT_SOF))
    {
        uint32_t gap = timestamp_ms -
                       parser->last_timestamp_ms;

        if (gap > parser->timeout_ms)
        {
            parser->timeout_gap_ms = gap;

            uart_parser_reset(parser);

            timeout_occurred = 1;
        }
    }

    switch (parser->state)
    {
        case WAIT_SOF:

            if (byte == UART_SOF)
            {
                parser->state = WAIT_CMD;
            }

            break;


        case WAIT_CMD:

            parser->cmd = byte;

            parser->running_checksum = byte;

            parser->state = WAIT_LEN;

            break;


        case WAIT_LEN:

            parser->len = byte;

            parser->running_checksum ^= byte;

            if (parser->len == 0U)
            {
                parser->state = WAIT_CHECKSUM;
            }
            else
            {
                parser->state = WAIT_PAYLOAD;
            }

            break;


        case WAIT_PAYLOAD:

            parser->payload[parser->payload_count] = byte;

            parser->payload_count++;

            parser->running_checksum ^= byte;

            if (parser->payload_count >= parser->len)
            {
                parser->state = WAIT_CHECKSUM;
            }

            break;


        case WAIT_CHECKSUM:

            if (byte == parser->running_checksum)
            {
                parser->state = WAIT_SOF;

                parser->last_timestamp_ms = timestamp_ms;

                return UART_FRAME_OK;
            }
            else
            {
                uart_parser_reset(parser);

                parser->last_timestamp_ms = timestamp_ms;

                return UART_CHECKSUM_ERROR;
            }


        default:

            uart_parser_reset(parser);

            break;
    }

    parser->last_timestamp_ms = timestamp_ms;

    if (timeout_occurred)
    {
        return UART_TIMEOUT_ERROR;
    }

    return UART_FRAME_IN_PROGRESS;
}

void run_test(const char *test_name,
              const uint8_t frame[],
              const uint32_t timestamps[],
              size_t length,
              uint32_t timeout_ms)
{
    uart_parser_t parser;

    uart_parser_init(&parser, timeout_ms);

    printf("\n=================================\n");
    printf("%s\n", test_name);
    printf("=================================\n");

    for (size_t i = 0; i < length; i++)
    {
        int status =
            uart_parser_feed(&parser,
                             frame[i],
                             timestamps[i]);

        if (status == UART_FRAME_IN_PROGRESS)
        {
            printf("t=%3lums  byte=0x%02X  -> receiving...\n",
                   (unsigned long)timestamps[i],
                   frame[i]);
        }
        else if (status == UART_FRAME_OK)
        {
            printf("t=%3lums  byte=0x%02X  -> FRAME OK  ",
                   (unsigned long)timestamps[i],
                   frame[i]);

            printf("CMD=0x%02X ",
                   parser.cmd);

            printf("LEN=%u ",
                   parser.len);

            printf("PAYLOAD=[");

            for (uint8_t j = 0; j < parser.len; j++)
            {
                printf("%02X", parser.payload[j]);

                if (j < parser.len - 1)
                {
                    printf(" ");
                }
            }

            printf("]\n");

            uart_parser_reset(&parser);
        }
        else if (status == UART_CHECKSUM_ERROR)
        {
            printf("t=%3lums  byte=0x%02X  -> CHECKSUM ERROR -- parser reset\n",
                   (unsigned long)timestamps[i],
                   frame[i]);
        }
        else if (status == UART_TIMEOUT_ERROR)
        {
            printf("t=%3lums  byte=0x%02X  -> TIMEOUT (%lums gap > %lums) -- parser reset\n",
                   (unsigned long)timestamps[i],
                   frame[i],
                   (unsigned long)parser.timeout_gap_ms,
                   (unsigned long)timeout_ms);

            printf("t=%3lums  byte=0x%02X  -> receiving... (re-fed after reset)\n",
                   (unsigned long)timestamps[i],
                   frame[i]);
        }
    }
}

int main(void)
{
    /* Official Test 1 */

    uint8_t frame1[] =
    {
        0xAA,
        0x01,
        0x03,
        0x10,
        0x20,
        0x30,
        0x02
    };

    uint32_t time1[] =
    {
        0U,
        5U,
        10U,
        15U,
        20U,
        25U,
        30U
    };

    run_test("Test 1 -- Clean Valid Frame",
             frame1,
             time1,
             sizeof(frame1) / sizeof(frame1[0]),
             50U);


    /* Official Test 2 */

    uint8_t frame2[] =
    {
        0xAA,
        0x01,
        0x03,
        0x10,

        0xAA,
        0x05,
        0x01,
        0x7F,
        0x7B
    };

    uint32_t time2[] =
    {
        0U,
        5U,
        10U,
        15U,

        200U,
        205U,
        210U,
        215U,
        220U
    };

    run_test("Test 2 -- Timeout Mid-Frame, Then Recovery",
             frame2,
             time2,
             sizeof(frame2) / sizeof(frame2[0]),
             50U);


    /* Official Test 3 */

    uint8_t frame3[] =
    {
        0xAA,
        0x03,
        0x01,
        0x55,
        0x57,

        0xAA,
        0x04,
        0x02,
        0xAA,
        0xBB,
        0x17
    };

    uint32_t time3[] =
    {
        0U,
        5U,
        10U,
        15U,
        20U,

        25U,
        30U,
        35U,
        40U,
        45U,
        50U
    };

    run_test("Test 3 -- Two Valid Frames Back-to-Back",
             frame3,
             time3,
             sizeof(frame3) / sizeof(frame3[0]),
             50U);


    /* Official Test 4 */

    uint8_t frame4[] =
    {
        0xAA,
        0x01,
        0x03,
        0x10,

        0xAA,
        0x05,
        0x01,
        0x7F,
        0x7B
    };

    uint32_t time4[] =
    {
        0U,
        5U,
        10U,
        15U,

        200U,
        205U,
        210U,
        215U,
        220U
    };

    run_test("Test 4 -- Timeout Disabled",
             frame4,
             time4,
             sizeof(frame4) / sizeof(frame4[0]),
             0U);

    return 0;
}
