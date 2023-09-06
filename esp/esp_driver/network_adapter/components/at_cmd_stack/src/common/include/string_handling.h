#ifndef _COMMON_STRING_HANDLING_H_
#define _COMMON_STRING_HANDLING_H_

/**
 * @brief Convert and validate a string to unsigned long integer
 * 
 * @param str input string to convert [in]
 * @param converted_number converted number [out]
 * @retval 0 if input string is valid
 * @retval 1 if input string is invalid
 */
extern int convert_and_validate(const char* str, 
    long int* converted_number);

/**
 * @brief Tokenize next quoted string
 * 
 * @param str input string to tokenize. Can be NULL if token_ctx is not NULL
 * for continuing processing [in]
 * @param delim delimiter string [in]
 * @param out_token return token. NULL if no such token is found [out]
 * @param token_ctx context to tokenize and to return for next tokenizing
 * [in,out]
 * 
 * @retval 0 if found quoted string VALID or no token is found
 * @retval -1 if found quoted string INVALID
 */
extern int tokenize_quoted_string(const char *str, const char *delim,
    char **out_token, char **token_ctx);

/**
 * @brief Remove all escape characters in a string
 * 
 * @param string_to_remove pointer to string to resolve [in,out]
 */
extern void remove_escape_characters_in_string(char* string_to_remove);

/**
 * @brief Add escape characters before every special characters in string
 * 
 * @param dst_string pointer to string to add escape characters [in,out]
 * @param dst_string_buff_size size of adding string buffer to [in]
 * 
 * @retval 0 add escape characters SUCCESS
 * @retval -1 0 add escape characters FAILED due to new string's is bigger than
 * buffer size
 */
extern int add_escape_characters_in_string(char* dst_string,
    unsigned int dst_string_buff_size);

/**
 * @brief Add begin and end quote characters to string
 * 
 * @param string_to_quote pointer to string to quote [in,out]
 * 
 * @retval 0 add quote characters SUCCESS
 * @retval -1 add quote characters FAILED due to new string's is bigger than
 * buffer size
 */
extern int add_quote_characters_to_string(char* dst_string,
    unsigned int dst_string_buff_size);

#endif