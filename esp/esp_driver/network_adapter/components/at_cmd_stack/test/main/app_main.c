#include "parse_test.h"
#include "unity.h"
#include <string.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define INPUT_BUFFER_LENGTH 5


static const char *test_description_table[] =
{
    "Restart board",
    "Test parsing"
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
        RUN_TEST(TestCase_Parse);
        UNITY_END();
        return;
    }
    printf("Not found such option. Please try again\n");
}