#include "parse_test.h"
#include "mqtt_test.h"
#include "at_cmd_test.h"
#include "unity.h"
#include <string.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define INPUT_BUFFER_LENGTH 5
#define WAIT_TO_RESULT_BEFORE_RESET_ms 3000

#define CHECK_MQTT_TEST_OPTION_AND_RUN(target_option, option_len, test_func) do \
{ \
    if ((!strcmp(option, target_option)) && (strlen(option) == option_len)) \
    { \
        UNITY_BEGIN(); \
        RUN_TEST(test_func); \
        UNITY_END(); \
        printf("Restart ESP32 after %d seconds...\n", \
            WAIT_TO_RESULT_BEFORE_RESET_ms / 1000); \
        uint32_t ticks_to_wait = pdMS_TO_TICKS(2000); \
        vTaskDelay(ticks_to_wait); \
        esp_restart(); \
    } \
} while (0)

static const char *test_description_table[] =
{
    "Restart board",
    "Connect to WiFi",
    "Test parsing",
    "Test TCP-MQTT connect/disconnect with public broker",
    "Test TCP-MQTT basic actions with public broker",
    "Test TCP-MQTT stress test with public broker",
    "Test TLS-MQTT connect/disconnect with public broker",
    "Test TLS-MQTT basic actions with public broker",
    "Test TLS-MQTT stress test with public broker",
    "Test TLS-MQTT connect/disconnect with ION broker",
    "Test TLS-MQTT basic actions with ION broker",
    "Test TLS-MQTT stress test with ION broker",
    "Test AT command with AT+QMTRECV trimmed",
    "Test AT command with AT+QMTRECV that has quote characters in message and topic",
    "Test AT command with AT+PUBEX that has wrong format quote character in message and topic",
};


static void getLineInput(char *buf, size_t len);
static void printHelper();
static void parse_and_exec_option(char *option);

void app_main()
{
    char chosen_test_option[INPUT_BUFFER_LENGTH];
    while (1)
    {
        printHelper();
        getLineInput(chosen_test_option, INPUT_BUFFER_LENGTH);
        parse_and_exec_option(chosen_test_option);
    }
}

static void printHelper()
{
    printf("---AT command stack test app v0.1---\n\n");
    printf("Choose your action:\n");
    int num_of_test_desc = 
        sizeof(test_description_table) / sizeof(test_description_table[0]);
    for (int index = 0; index < num_of_test_desc; index++)
    {
        printf("'%d': %s\n", index, test_description_table[index]);
    }
    printf("\n\n");
    printf("Your answer is (from '%d' to '%d'): ", 0, num_of_test_desc - 1);
}

static void getLineInput(char *buf, size_t len)
{
    memset(buf, 0, len);
    fpurge(stdin); //clears any junk in stdin
    char *bufp;
    bufp = buf;
    while(true)
        {
            vTaskDelay(100/portTICK_PERIOD_MS);
            *bufp = getchar();
            if(*bufp != '\0' && *bufp != 0xFF && *bufp != '\r') //ignores null input, 0xFF, CR in CRLF
            {
                //'enter' (EOL) handler 
                if(*bufp == '\n'){
                    *bufp = '\0';
                    break;
                } //backspace handler
                else if (*bufp == '\b'){
                    if(bufp-buf >= 1)
                    {
                        printf("\b \b");
                        bufp--;
                    }
                }
                else{
                    //pointer to next character
                    printf("%c", *bufp);
                    bufp++;
                }
            }
            
            //only accept len-1 characters, (len) character being null terminator.
            if(bufp-buf > (len)-2){
                bufp = buf + (len -1);
                *bufp = '\0';
                break;
            }
        } 
}

static void parse_and_exec_option(char *option)
{
    printf("\nAnswer is '%s'\n", option);
    if ((!strcmp(option, "0")) && (strlen(option) == 1))
    {
        esp_restart();
    }
    else if ((!strcmp(option, "1")) && (strlen(option) == 1))
    {
        UNITY_BEGIN();
        RUN_TEST(TestCase_Connect_to_WiFi);
        UNITY_END();
        return;
    }
    else if ((!strcmp(option, "2")) && (strlen(option) == 1))
    {
        UNITY_BEGIN();
        RUN_TEST(TestCase_Parse);
        UNITY_END();
        return;
    }
    CHECK_MQTT_TEST_OPTION_AND_RUN("3", 1, TestCase_MQTT_Connect_Disconnect);
    CHECK_MQTT_TEST_OPTION_AND_RUN("4", 1, TestCase_MQTT_Basic_Actions);
    CHECK_MQTT_TEST_OPTION_AND_RUN("5", 1, TestCase_MQTT_StressTest);
    CHECK_MQTT_TEST_OPTION_AND_RUN("6", 1, TestCase_MQTT_TLS_Connect_Disconnect);
    CHECK_MQTT_TEST_OPTION_AND_RUN("7", 1, TestCase_MQTT_TLS_Basic_Actions);
    CHECK_MQTT_TEST_OPTION_AND_RUN("8", 1, TestCase_MQTT_TLS_StressTest);
    CHECK_MQTT_TEST_OPTION_AND_RUN("9", 1, TestCase_MQTT_ION_Broker_Connect_Disconnect);
    CHECK_MQTT_TEST_OPTION_AND_RUN("10", 2, TestCase_MQTT_ION_Broker_Basic_Actions);
    CHECK_MQTT_TEST_OPTION_AND_RUN("11", 2, TestCase_MQTT_ION_Broker_StressTest);
    CHECK_MQTT_TEST_OPTION_AND_RUN("12", 2, TestCase_QmtRecvTrimmed);
    CHECK_MQTT_TEST_OPTION_AND_RUN("13", 2, TestCase_QmtRecvSpecialCharacters);
    CHECK_MQTT_TEST_OPTION_AND_RUN("14", 2, TestCase_QmtPubexWrongQuote);
    printf("Not found such option. Please try again\n");
}