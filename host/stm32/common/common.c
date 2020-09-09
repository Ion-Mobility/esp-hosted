// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

/** Includes **/
#include "common.h"
#include "trace.h"

/** Constants/Macros **/

/** Exported variables **/


/** Function declaration **/

/** Exported Functions **/
/**
  * @brief  debug buffer print
  * @param  buff - input buffer to print in hex
  *         rx_len - buff len
  *         human_str - helping string to describe about buffer
  * @retval None
  */
#if DEBUG_HEX_STREAM_PRINT
char print_buff[2048*3];
#endif

void print_hex_dump(uint8_t *buff, uint16_t rx_len, char *human_str)
{
#if DEBUG_HEX_STREAM_PRINT
	char * print_ptr = print_buff;
	for (int i = 0; i< rx_len;i++) {
		sprintf(print_ptr, "%02x ",buff[i]);
		print_ptr+=3;
	}
	print_buff[rx_len*3] = '\0';

	if(human_str)
		printf("%s (len: %5u) -> %s\n\r",human_str, rx_len, print_buff);
	else
		printf("(len %5u), Data %s\n\r", rx_len, print_buff);
#else
	UNUSED_VAR(buff);
	UNUSED_VAR(rx_len);
	UNUSED_VAR(human_str);
#endif /* DEBUG_HEX_STREAM_PRINT */
}

/**
  * @brief  Calculate minimum
  * @param  x - number
  *         y - number
  * @retval minimum
  */
int min(int x, int y) {
	return (x < y) ? x : y;
}

/**
  * @brief Delay without context switch
  * @param  None
  * @retval None
  */
void hard_delay(int x)
{
	volatile int idx = 0;
	for (idx=0; idx<100*x; idx++) {
	}
}


/** Local functions **/
