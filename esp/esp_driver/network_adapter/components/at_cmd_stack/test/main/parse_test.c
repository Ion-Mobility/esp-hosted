#include <stdio.h>
#include "at_cmd_parse_and_exec.h"
#include "unity.h"
#include "common_test_helpers.h"
#include <string.h>

#define STRINGIFY_NUM(num) STRINGIFY(num)
#define STRINGIFY(str) #str

typedef struct {
    const char *test_name;
    const char *at_cmd;
    const char *expected_response;
} at_cmd_test_scene_entry_t;

static const char *module = "parse_test: ";

static const at_cmd_test_scene_entry_t parse_test_cases_table[] =
{
    {
        .test_name = "Correct AT test command",
        .at_cmd = "AT+PARSETEST=?",
        .expected_response = "+PARSETEST: <0-1>,<required_quoted_string>[,<0-65535>[,<optional_quoted_string_1>[,<optional_quoted_string_2>]]]\r\n\r\nOK",
    },
    {
        .test_name = "Incorrect AT test command with additional character",
        .at_cmd = "AT+PARSETESTT=?",
        .expected_response = "ERROR"
    },
    {
        .test_name = "Incorrect AT test command with less character",
        .at_cmd = "AT+PARSETES=?",
        .expected_response = "ERROR"
    },
    {
        .test_name = "Correct AT read",
        .at_cmd = "AT+PARSETEST?",
        .expected_response = "+PARSETEST: this is READ cmd"
    },
    {
        .test_name = "Incorrect AT read command with additional character",
        .at_cmd = "AT+PARSETESTT?",
        .expected_response = "ERROR"
    },
    {
        .test_name = "Incorrect AT read command with less character",
        .at_cmd = "AT+PARSETES?",
        .expected_response = "ERROR"
    },
    {
        .test_name = "Correct AT write command with required only",
        .at_cmd = "AT+PARSETEST=0,\"test_req\"",
        .expected_response = "+PARSETEST: found required '0','test_req'\r\n\r\nOK"
    },
    {
        .test_name = "Incorrect AT write command out of range require lint",
        .at_cmd = "AT+PARSETEST=2,\"test_req\"",
        .expected_response = "ERROR"
    },
    {
        .test_name = "Incorrect AT write command wrong format require lint",
        .at_cmd = "AT+PARSETEST=0.5,\"test_req\"",
        .expected_response = "ERROR"
    },
    {
        .test_name = "Incorrect AT write command with incorrect quoted string #1",
        .at_cmd = "AT+PARSETEST=0,\"test_req",
        .expected_response = "ERROR"
    },
    {
        .test_name = "Incorrect AT write command with incorrect quoted string #2",
        .at_cmd = "AT+PARSETEST=0,test_req\"",
        .expected_response = "ERROR"
    },
    {
        .test_name = "Incorrect AT write command with incorrect quoted string #3",
        .at_cmd = "AT+PARSETEST=0,test_req",
        .expected_response = "ERROR"
    },
    {
        .test_name = "Correct AT write command with optional lint",
        .at_cmd = "AT+PARSETEST=0,\"test_req\",0",
        .expected_response = "+PARSETEST: found required '0','test_req'; found optional '0'\r\n\r\nOK"
    },
    {
        .test_name = "Incorrect AT write command with optional lint out of range",
        .at_cmd = "AT+PARSETEST=0,\"test_req\",65536",
        .expected_response = "ERROR"
    },
    {
        .test_name = "Incorrect AT write command with wrong format optional",
        .at_cmd = "AT+PARSETEST=0,\"test_req\",55.5",
        .expected_response = "ERROR"
    },
    {
        .test_name = "Correct AT write command both optional lint and 1 quoted string",
        .at_cmd = "AT+PARSETEST=0,\"test_req\",0,\"test_opt\"",
        .expected_response = "+PARSETEST: found required '0','test_req'; found optional '0','test_opt'\r\n\r\nOK"
    },
    {
        .test_name = "Correct AT write command both optional lint and 1 quoted string. Comma in quote string",
        .at_cmd = "AT+PARSETEST=0,\"test_req, hello\",0,\"test_opt, hello\"",
        .expected_response = "+PARSETEST: found required '0','test_req, hello'; found optional '0','test_opt, hello'\r\n\r\nOK"
    },
    {
        .test_name = "Correct AT write command both optional lint and 1 quoted string. Quote character in quote string",
        .at_cmd = "AT+PARSETEST=0,\"\\\"test_req\\\"\",0,\"\\\"test_opt\\\"\"",
        .expected_response = "+PARSETEST: found required '0','\"test_req\"'; found optional '0','\"test_opt\"'\r\n\r\nOK"
    },
    {
        .test_name = "Correct AT write command both optional lint and 1 quoted string. Quote character and comma in quote string",
        .at_cmd = "AT+PARSETEST=0,\"\\\"test_req\\\", hello\",0,\"\\\"test_opt\\\", hello\"",
        .expected_response = "+PARSETEST: found required '0','\"test_req\", hello'; found optional '0','\"test_opt\", hello'\r\n\r\nOK"
    },
    {
        .test_name = "Correct AT write command both optional lint and 2 quoted strings. Comma in quote strings",
        .at_cmd = "AT+PARSETEST=0,\"test_req, hello\",0,\"test_opt1, hello\",\"test_opt2, hello\"",
        .expected_response = "+PARSETEST: found required '0','test_req, hello'; found optional '0','test_opt1, hello','test_opt2, hello'\r\n\r\nOK"
    },
    {
        .test_name = "Correct AT write command both optional lint and 2 quoted strings. Quote character in quote strings",
        .at_cmd = "AT+PARSETEST=0,\"\\\"test_req\\\"\",0,\"\\\"test_opt1\\\"\",\"\\\"test_opt2\\\"\"",
        .expected_response = "+PARSETEST: found required '0','\"test_req\"'; found optional '0','\"test_opt1\"','\"test_opt2\"'\r\n\r\nOK"
    },
    {
        .test_name = "Correct AT write command both optional lint and 2 quoted strings. Quote character and comma in quote strings",
        .at_cmd = "AT+PARSETEST=0,\"\\\"test_req\\\", hello\",0,\"\\\"test_opt1\\\", hello\",\"\\\"test_opt2\\\", hello\"",
        .expected_response = "+PARSETEST: found required '0','\"test_req\", hello'; found optional '0','\"test_opt1\", hello','\"test_opt2\", hello'\r\n\r\nOK"
    },
    {
        .test_name = "Incorrect AT write command with incorrect optional quoted string #1",
        .at_cmd = "AT+PARSETEST=0,\"test_req\",0,test_opt\"",
        .expected_response = "ERROR"
    },
    {
        .test_name = "Incorrect AT write command with incorrect optional quoted string #2",
        .at_cmd = "AT+PARSETEST=0,\"test_req\",0,\"test_opt",
        .expected_response = "ERROR"
    },
    {
        .test_name = "Incorrect AT write command with incorrect optional quoted string #3",
        .at_cmd = "AT+PARSETEST=0,\"test_req\",0,test_opt",
        .expected_response = "ERROR"
    },
    {
        .test_name = "Correct AT execute COMMAND",
        .at_cmd = "AT+PARSETEST",
        .expected_response = "+PARSETEST: this is EXECUTE cmd"
    },
    {
        .test_name = "Incorrect AT execute command with additional character",
        .at_cmd = "AT+PARSETESTt",
        .expected_response = "ERROR"
    },
    {
        .test_name = "Incorrect AT execute command with less character",
        .at_cmd = "AT+PARSETES",
        .expected_response = "ERROR"
    },
};


static void perform_all_test_cases(
    const at_cmd_test_scene_entry_t *test_cases_table, int num_of_test_cases);


void TestCase_Parse()
{
    int result;
    char *dummy_at_cmd = malloc(10);
    char *dummy_at_resp = malloc(10);

    result = parse_and_exec_at_cmd(NULL, 23, dummy_at_resp);
    TEST_LOG("Test NULL at command\n");
    TEST_ASSERT_EQUAL_UINT(0, result);

    TEST_LOG("Test length 0 at command\n");
    result = parse_and_exec_at_cmd(dummy_at_cmd, 0, dummy_at_resp);
    TEST_ASSERT_EQUAL_UINT(0, result);

    TEST_LOG("Test NULL 0 at response\n");
    result = parse_and_exec_at_cmd(dummy_at_cmd, 0, NULL);
    TEST_ASSERT_EQUAL_UINT(0, result);
    free(dummy_at_cmd);
    free(dummy_at_resp);
    TEST_LOG("Test some commands\n");
    perform_all_test_cases(parse_test_cases_table, 
        sizeof(parse_test_cases_table)/sizeof(parse_test_cases_table[0]));
}


static void perform_all_test_cases(
    const at_cmd_test_scene_entry_t *test_cases_table, int num_of_test_cases)
{
    char *get_at_resp = malloc(MAX_AT_RESP_LENGTH);
    int result;
    for (int test_idx = 0; test_idx < num_of_test_cases; test_idx++)
    {
        at_cmd_test_scene_entry_t test_case = test_cases_table[test_idx];
        printf("\n");
        TEST_LOG("Test case #%d: %s\n", test_idx, test_case.test_name);
        TEST_LOG("AT command to use: '%s'\n", test_case.at_cmd);
        memset(get_at_resp, 0, MAX_AT_RESP_LENGTH);
        result = parse_and_exec_at_cmd(
            test_case.at_cmd, strlen(test_case.at_cmd), get_at_resp);
        TEST_LOG("Get response: '%s', expected '%s'\n", 
            get_at_resp, test_case.expected_response);
        TEST_ASSERT_EQUAL_STRING(get_at_resp, test_case.expected_response);
    }
    free(get_at_resp);
}