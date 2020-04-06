#include "include/string_utils.h"
#include <string.h>
#include <stdio.h>


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

char* hermes_get_stdin(const char* printstr)
{
    char* str;
    int c;
    int i;
    int size = 10;

    str = malloc(size * sizeof(char));

    printf(">: ");

    for(i = 0; (c = getchar()) != '\n' && c != EOF; ++i)
    {
        if(i == size)
        {
            size = 2*size;
            str = realloc(str, size*sizeof(char));

            if(str == NULL)
            {
                printf("Cannot reallic string.\n");
                exit(-1);
            }
        }

        str[i] = c;
    }

    if(i == size)
    {
        str = realloc(str, (size+1)*sizeof(char));

        if(str == NULL)
        {
            printf("Cannot realloc string.\n");
            exit(-1);
        }
    }

    return str;
}
