#include "parse_test.h"
#include "unity.h"
#include <string.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void getLineInput(char *buf, size_t len)
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

void app_main()
{
    char chosen_test_option[3];
    while (1)
    {
        printf("---AT command stack test app v0.1---\n\n");
        printf("Choose your action:\n");
        printf("'0': Restart board\n");
        printf("'1': Test parsing\n");    
        printf("\n\n");
        printf("Your answer (from '0' to '1'): ");
        getLineInput(chosen_test_option, 3);    
        printf("\nAnswer is '%s'\n", chosen_test_option);
        if ((!strcmp(chosen_test_option, "0")) && 
            (strlen(chosen_test_option) == 1))
        {
            esp_restart();
        }
        if ((!strcmp(chosen_test_option, "1")) && 
            (strlen(chosen_test_option) == 1))
        {
            UNITY_BEGIN();
            RUN_TEST(TestCase_Parse);
            UNITY_END();
        }
        printf("Not found such option. Please try again\n");
    }


}