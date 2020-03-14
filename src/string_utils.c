#include "include/string_utils.h"
#include <string.h>


char* hermes_char_to_string(char c)
{
    char* str = calloc(2, sizeof(char));
    str[0] = c;
    str[1] = '\0';  // not really needed because of calloc.

    return str;
}

char* hermes_init_str(const char* value)
{
    char* str = calloc(strlen(value) + 1, sizeof(char));
    strcpy(str, value);
    return str;
}
