#include "parse_and_exec_cmd_handlers_list.h"
#include "cmd_handlers/qmtcfg_parse_exec_handler.h"
#include "cmd_handlers/qmtopen_parse_exec_handler.h"
#include "cmd_handlers/qmtconn_parse_exec_handler.h"
#include "cmd_handlers/qmtsub_parse_exec_handler.h"
#include "cmd_handlers/qmtuns_parse_exec_handler.h"
#include "cmd_handlers/qmtdisc_parse_exec_handler.h"
#include "cmd_handlers/qmtpubex_parse_exec_handler.h"
#include "cmd_handlers/qmtrecv_parse_exec_handler.h"
#include "cmd_handlers/qmtdump_parse_exec_handler.h"
#include "cmd_handlers/parsetest_parse_exec_handler.h"

const parse_exec_handler_entry_t parse_exec_handlers_table
    [MAX_NUMBER_OF_SUPPORTED_AT_COMMANDS] =
{
    {
        .at_command_family = "AT+QMTCFG",
        .test_cmd_parse_exec_handler = qmtcfg_test_cmd_parse_exec_handler,
        .write_cmd_parse_exec_handler  = qmtcfg_write_cmd_parse_exec_handler,
    },
    {
        .at_command_family = "AT+QMTOPEN",
        .test_cmd_parse_exec_handler = qmtopen_test_cmd_parse_exec_handler,
    },
    {
        .at_command_family = "AT+QMTCONN",
        .test_cmd_parse_exec_handler = qmtconn_test_cmd_parse_exec_handler,
        .read_cmd_parse_exec_handler = qmtconn_read_cmd_parse_exec_handler,
        .write_cmd_parse_exec_handler = qmtconn_write_cmd_parse_exec_handler,
    },
    {
        .at_command_family = "AT+QMTSUB",
        .test_cmd_parse_exec_handler = qmtsub_test_cmd_parse_exec_handler,
        .write_cmd_parse_exec_handler = qmtsub_write_cmd_parse_exec_handler,
    },
    {
        .at_command_family = "AT+QMTUNS",
        .test_cmd_parse_exec_handler = qmtuns_test_cmd_parse_exec_handler,
        .write_cmd_parse_exec_handler = qmtuns_write_cmd_parse_exec_handler,
    },
    {
        .at_command_family = "AT+QMTDISC",
        .test_cmd_parse_exec_handler = qmtdisc_test_cmd_parse_exec_handler,
        .write_cmd_parse_exec_handler = qmtdisc_write_cmd_parse_exec_handler,
    },
    {
        .at_command_family = "AT+QMTPUBEX",
        .test_cmd_parse_exec_handler = qmtpubex_test_cmd_parse_exec_handler,
        .write_cmd_parse_exec_handler = qmtpubex_write_cmd_parse_exec_handler,
    },
    {
        .at_command_family = "AT+QMTRECV",
        .test_cmd_parse_exec_handler = qmtrecv_test_cmd_parse_exec_handler,
        .read_cmd_parse_exec_handler = qmtrecv_read_cmd_parse_exec_handler,
        .write_cmd_parse_exec_handler = qmtrecv_write_cmd_parse_exec_handler,
    },
    {
        .at_command_family = "AT+QMTDUMP",
        .read_cmd_parse_exec_handler = qmtdump_read_cmd_parse_exec_handler,
    },
    {
        .at_command_family = "AT+PARSETEST",
        .test_cmd_parse_exec_handler = parsetest_test_cmd_parse_exec_handler,
        .read_cmd_parse_exec_handler = parsetest_read_cmd_parse_exec_handler,
        .write_cmd_parse_exec_handler = parsetest_write_cmd_parse_exec_handler,
        .exec_cmd_parse_exec_handler = parsetest_exec_cmd_parse_exec_handler,
    },
};

