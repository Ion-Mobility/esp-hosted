#include "at_cmd_test.h"
#include "at_cmd_quectel_helpers.h"
#include "at_cmd_app.h"
#include "protocol_examples_common.h"
#include "common_test_helpers.h"
#include "at_cmd_stack_types.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "unity.h"

#define TOTAL_STEPS_OF_TEST_SUITE(test_suite) sizeof(test_suite) / sizeof(test_step_t)

typedef struct {
    char *resp;
} resp_buff_handle_t;

typedef struct {
    char* at_cmd;
    char* expected_resp;
    int retry_count;
    unsigned int delay_before_send_cmd_ms;
} test_step_t;

typedef struct {
    const test_step_t *target_test_suite;
    int current_retry;
} test_suite_status_t;

static const char *module = "at_cmd_test: ";

static int send_at_response_callback_for_test_app(
    void *sending_resp, uint32_t resp_len);

static bool is_test_step_good(
    const test_step_t *test_step);

static void perform_at_cmd_test_suite(const test_step_t *test_suite, 
    unsigned int num_of_test_steps);

static QueueHandle_t recv_resp_queue_handle = NULL;

static test_suite_status_t running_test_suite_status;

static const test_step_t test_suite_multiple_publishes[] =
{
    {
        .at_cmd = "AT+QMTCONN=0,\"mqtts://ion-broker-s.ionmobility.net\",8883,\"esp32-wifi\",\"9b5766d8-8766-492c-ace8-a80f191e47e6\",\"0caabff1-dd1e-49da-a3a3-9a00d4ff2cb6\"",
        .expected_resp = "OK\r\n\r\n+QMTCONN: 0,0,0"
    },
    {
        .at_cmd = "AT+QMTRECV=0",
        .expected_resp = NULL,
    },
    {
        .at_cmd = "AT+QMTSUB=0,\"channels/ccb53e96-8628-4ac0-9e3f-95185f38733a/messages\",2",
        .expected_resp = "OK\r\n+QMTSUB: 0,0"
    },
    {
        .at_cmd = "AT+QMTPUBEX=0,2,0,\"channels/ccb53e96-8628-4ac0-9e3f-95185f38733a/messages\",\"Speedy Speedy BRRUHHHH\"",
        .expected_resp = "OK\r\n+QMTPUBEX: 0,0"
    },
    {
        .at_cmd = "AT+QMTPUBEX=0,2,0,\"channels/ccb53e96-8628-4ac0-9e3f-95185f38733a/messages\",\"Speedy Speedy BRRUHHHH\"",
        .expected_resp = "OK\r\n+QMTPUBEX: 0,0"
    },
    {
        .at_cmd = "AT+QMTPUBEX=0,2,0,\"channels/ccb53e96-8628-4ac0-9e3f-95185f38733a/messages\",\"Speedy Speedy BRRUHHHH\"",
        .expected_resp = "OK\r\n+QMTPUBEX: 0,0"
    },
    {
        .at_cmd = "AT+QMTPUBEX=0,2,0,\"channels/ccb53e96-8628-4ac0-9e3f-95185f38733a/messages\",\"Speedy Speedy BRRUHHHH\"",
        .expected_resp = "OK\r\n+QMTPUBEX: 0,0"
    },
    {
        .at_cmd = "AT+QMTRECV?",
        .expected_resp = "+QMTRECV\r\n0,1\r\n\r\nOK",
        .delay_before_send_cmd_ms = 800,
    },
    {
        .at_cmd = "AT+QMTRECV=0",
        .expected_resp = "+QMTRECV:\r\n0,\"channels/ccb53e96-8628-4ac0-9e3f-95185f38733a/messages\",\"Speedy Speedy BRRUHHHH\"",
    },
    {
        .at_cmd = "AT+QMTRECV?",
        .expected_resp = "+QMTRECV\r\n0,1\r\n\r\nOK",
    },
    {
        .at_cmd = "AT+QMTRECV=0",
        .expected_resp = "+QMTRECV:\r\n0,\"channels/ccb53e96-8628-4ac0-9e3f-95185f38733a/messages\",\"Speedy Speedy BRRUHHHH\"",
    },
    {
        .at_cmd = "AT+QMTRECV?",
        .expected_resp = "+QMTRECV\r\n0,1\r\n\r\nOK",
    },
    {
        .at_cmd = "AT+QMTRECV=0",
        .expected_resp = "+QMTRECV:\r\n0,\"channels/ccb53e96-8628-4ac0-9e3f-95185f38733a/messages\",\"Speedy Speedy BRRUHHHH\"",
    },
    {
        .at_cmd = "AT+QMTRECV?",
        .expected_resp = "+QMTRECV\r\n0,1\r\n\r\nOK",
    },
    {
        .at_cmd = "AT+QMTRECV=0",
        .expected_resp = "+QMTRECV:\r\n0,\"channels/ccb53e96-8628-4ac0-9e3f-95185f38733a/messages\",\"Speedy Speedy BRRUHHHH\"",
    },
    {
        .at_cmd = "AT+QMTRECV?",
        .expected_resp = "+QMTRECV\r\n0,0\r\n\r\nOK",
    },
    {
        .at_cmd = "AT+QMTUNS=0,\"channels/ccb53e96-8628-4ac0-9e3f-95185f38733a/messages\",",
        .expected_resp = "OK\r\n+QMTUNS: 0,0",
    },
    {
        .at_cmd = "AT+QMTDISC=0",
        .expected_resp = "OK\r\n+QMTDISC: 0,0"
    },
};

static const test_step_t test_suite_qmt_recv_special_characters[] =
{
    {
        .at_cmd = "AT+QMTCONN=0,\"mqtt://broker.hivemq.com\",1883,\"esp32-wifi\"",
        .expected_resp = "OK\r\n\r\n+QMTCONN: 0,0,0"
    },
    {
        .at_cmd = "AT+QMTRECV=0",
        .expected_resp = NULL,
    },
    {
        .at_cmd = "AT+QMTSUB=0,\"\\\"quoted_topic\\\"\",2",
        .expected_resp = "OK\r\n+QMTSUB: 0,0"
    },
    {
        .at_cmd = "AT+QMTPUBEX=0,2,0,\"\\\"quoted_topic\\\"\",\"\\\"temperature\\\": \\\"23,24\\\"\"",
        .expected_resp = "OK\r\n+QMTPUBEX: 0,0"
    },
    {
        .at_cmd = "AT+QMTRECV=0",
        .expected_resp = "+QMTRECV:\r\n0,\"\\\"quoted_topic\\\"\",\"\\\"temperature\\\": \\\"23,24\\\"\"",
        .delay_before_send_cmd_ms = 800,
    },
    {
        .at_cmd = "AT+QMTUNS=0,\"\\\"quoted_topic\\\"\",",
        .expected_resp = "OK\r\n+QMTUNS: 0,0",
    },
    {
        .at_cmd = "AT+QMTDISC=0",
        .expected_resp = "OK\r\n+QMTDISC: 0,0"
    },
};

static const test_step_t test_suite_qmt_wrong_quote[] =
{
    {
        .at_cmd = "AT+QMTCONN=0,\"mqtt://broker.hivemq.com\",1883,\"esp32-wifi\"",
        .expected_resp = "OK\r\n\r\n+QMTCONN: 0,0,0"
    },
    {
        .at_cmd = "AT+QMTRECV=0",
        .expected_resp = NULL,
    },
    {
        .at_cmd = "AT+QMTPUBEX=0,2,0,\"\"quoted_topic\"\",\"\\\"temperature\\\": \\\"23,24\\\"\"",
        .expected_resp = "ERROR"
    },
    {
        .at_cmd = "AT+QMTPUBEX=0,2,0,\"\\\"quoted_topic\\\"\",\"\"temperature\": \\\"23,24\\\"\"",
        .expected_resp = "ERROR"
    },
    {
        .at_cmd = "AT+QMTDISC=0",
        .expected_resp = "OK\r\n+QMTDISC: 0,0"
    },
};


void TestCase_QmtMultiplePublishes()
{
    perform_at_cmd_test_suite(test_suite_multiple_publishes,
        TOTAL_STEPS_OF_TEST_SUITE(test_suite_multiple_publishes));
}

void TestCase_QmtRecvSpecialCharacters()
{
    perform_at_cmd_test_suite(test_suite_qmt_recv_special_characters,
        TOTAL_STEPS_OF_TEST_SUITE(test_suite_qmt_recv_special_characters));
}

void TestCase_QmtPubexWrongQuote()
{
    perform_at_cmd_test_suite(test_suite_qmt_wrong_quote,
        TOTAL_STEPS_OF_TEST_SUITE(test_suite_qmt_wrong_quote));
}

static int send_at_response_callback_for_test_app(
    void *sending_resp, uint32_t resp_len)
{
    char *resp_for_test = calloc(1, resp_len + 1);
    memcpy(resp_for_test, sending_resp, resp_len);
    free(sending_resp);
    resp_buff_handle_t resp_buff_handle ={
        .resp = resp_for_test
    };
    TEST_LOG("Respose from AT stack '%s'", (char*) resp_for_test);

	if (xQueueSend(recv_resp_queue_handle, &resp_buff_handle, portMAX_DELAY) 
        != pdPASS)
    {
        TEST_LOG("Send response FAILED");
	    return -1;
    }
    TEST_LOG("Send response SUCCESS");
    return 0;
}

static void perform_at_cmd_test_suite(const test_step_t *test_suite, 
    unsigned int num_of_test_steps)
{
    running_test_suite_status.target_test_suite = test_suite;
    recv_resp_queue_handle = xQueueCreate(1, sizeof(resp_buff_handle_t));
    test_begin_setup();
    init_at_cmd_app();
    
    for (int index = 0; index < num_of_test_steps; index++)
    {
        const test_step_t *current_test_step = 
            &running_test_suite_status.target_test_suite[index];
        running_test_suite_status.current_retry = current_test_step->retry_count;
        while (1)
        {
            TEST_LOG("Step #%u, current retry: %d\n", 
                index, running_test_suite_status.current_retry);
            if (is_test_step_good(current_test_step))
            {
                TEST_LOG("Test step PASSED\n");
                break;
            }
            if (running_test_suite_status.current_retry-- < 0)
                break;
        }
        TEST_ASSERT_GREATER_OR_EQUAL_INT(0, running_test_suite_status.current_retry);

    }
    test_done_clear_up();    
}

static bool is_test_step_good(
    const test_step_t *test_step)
{
    resp_buff_handle_t get_resp_buff_handle;
    if (test_step->delay_before_send_cmd_ms)
    {
        TEST_LOG("Delay %u for this test step\n", 
            test_step->delay_before_send_cmd_ms);
        vTaskDelay(pdMS_TO_TICKS(test_step->delay_before_send_cmd_ms));
    }

    make_request_to_handle_at_cmd(test_step->at_cmd, 
        send_at_response_callback_for_test_app);
    if (xQueueReceive(recv_resp_queue_handle, &get_resp_buff_handle, 
        portMAX_DELAY))
    {
        if (test_step->expected_resp == NULL)
            return true;
        if (strcmp(get_resp_buff_handle.resp, test_step->expected_resp)
            || (strlen(get_resp_buff_handle.resp) != 
            strlen(test_step->expected_resp)))
            {
                free(get_resp_buff_handle.resp);
                return false;
            }
        free(get_resp_buff_handle.resp);
        return true;
    }

    // Cannot reach here, but still have to return to avoid compiler error
    return false;
}
