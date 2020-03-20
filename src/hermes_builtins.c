#include "include/hermes_builtins.h"
#include "include/hermes_runtime.h"
#include "include/hermes_lexer.h"
#include "include/hermes_parser.h"
#include "include/string_utils.h"
#include "include/io.h"
#include <string.h>

/**
 * Simple print implementation.
 *
 * @param AST_T* self
 * @param dynamic_list_T* args
 *
 * @return AST_T*
 */
AST_T* hermes_builtin_function_print(AST_T* self, dynamic_list_T* args)
{
    for (int i = 0; i < args->size; i++)
    {
        AST_T* ast_arg = (AST_T*) args->items[i];
        char* str = ast_to_string(ast_arg);

        if (str == (void*)0)
        {
            printf("(void*)0\n");
        }
        else
        {
            printf("%s\n", str);
            free(str);
        }
    }

    return INITIALIZED_NOOP;
}

/**
 * Print the adress of a value
 *
 * @param AST_T* self
 * @param dynamic_list_T* args
 *
 * @return AST_T*
 */
AST_T* hermes_builtin_function_aprint(AST_T* self, dynamic_list_T* args)
{
    for (int i = 0; i < args->size; i++)
        printf("%p\n", (AST_T*) args->items[i]);

    return INITIALIZED_NOOP;
}

/**
 * Method for including other scripts, this will return the root node
 * as a compound.
 *
 * @param AST_T* self
 * @param dynamic_list_T* args
 *
 * @return AST_T*
 */
AST_T* hermes_builtin_function_include(AST_T* self, dynamic_list_T* args)
{
    runtime_expect_args(args, 1, (int[]) {AST_STRING});

    AST_T* ast_str = (AST_T*) args->items[0];
    char* filename = ast_str->string_value;

    hermes_lexer_T* lexer = init_hermes_lexer(hermes_read_file(filename));
    hermes_parser_T* parser = init_hermes_parser(lexer);
    AST_T* node = hermes_parser_parse(parser, (void*) 0);

    hermes_lexer_free(lexer);

    return node;
}

/**
 * Builtin method to dump an AST node to disk, serialized.
 *
 * @param AST_T* self
 * @param dynamic_list_T* args
 *
 * @return AST_T*
 */
AST_T* hermes_builtin_function_wad(AST_T* self, dynamic_list_T* args)
{
    runtime_expect_args(args, 2, (int[]) {AST_COMPOUND, AST_STRING});

    AST_T* ast_compound = (AST_T*) args->items[0];
    AST_T* ast_string = (AST_T*) args->items[1];
    
    char* filename = ast_string->string_value;

    FILE *outfile;

    const char* filename_template = "%s.dat";
    char* fname = calloc(strlen(filename) + strlen(filename_template) + 1, sizeof(char));
    sprintf(fname, filename_template, filename);
      
    outfile = fopen(fname, "w");

    if (outfile == NULL) 
    { 
        fprintf(stderr, "Could not open %s\n", fname);
        free(fname); 
        exit(1); 
    } 
  
    // write struct to file 
    int r = fwrite (&*ast_compound, sizeof(struct AST_STRUCT), 1, outfile);
      
    if(r != 0)
    {
        // silence
    }
    else
    {
        printf("Could not write to %s\n", fname);
    }
    
    free(fname); 
  
    fclose(outfile); 
  
    return ast_compound;
}

/**
 * Built-in function to load a compound AST from disk
 *
 * @param AST_T* self
 * @param dynamic_list_T* args
 *
 * @return AST_T*
 */
AST_T* hermes_builtin_function_lad(AST_T* self, dynamic_list_T* args)
{
    runtime_expect_args(args, 1, (int[]) {AST_STRING});
    
    AST_T* ast_string = (AST_T*) args->items[0];

    char* filename = ast_string->string_value;

    const char* filename_template = "%s.dat";
    char* fname = calloc(strlen(filename) + strlen(filename_template) + 1, sizeof(char));
    sprintf(fname, filename_template, filename);

    FILE *infile; 
    struct AST_STRUCT input; 
      
    infile = fopen(fname, "r"); 
    if (infile == NULL) 
    { 
        fprintf(stderr, "Error reading %s\n", fname); 
        free(fname);
        exit(1); 
    } 
      
    // read file contents till end of file 
    while(fread(&input, sizeof(struct AST_STRUCT), 1, infile)) 
        printf("...\n");
  
    // close file 
    fclose (infile);

    AST_T* loaded = &input;

    return loaded;
}

static AST_T* object_file_function_read(AST_T* self, dynamic_list_T* args)
{
    FILE* f = self->object_value;

    char* buffer = 0;
    long length;

    if (f)
    {
        fseek (f, 0, SEEK_END);
        length = ftell (f);
        fseek (f, 0, SEEK_SET);
        buffer = calloc (length, length);

        if (buffer)
            fread (buffer, 1, length, f);
    }

    AST_T* ast_string = init_ast(AST_STRING);
    ast_string->string_value = buffer;

    return ast_string;
}

/**
 * Built-in function to open file.
 *
 * @param AST_T* self
 * @param dynamic_list_T* args
 *
 * @return AST_T*
 */
AST_T* hermes_builtin_function_fopen(AST_T* self, dynamic_list_T* args)
{
    runtime_expect_args(args, 2, (int[]) {AST_STRING, AST_STRING});

    char* filename = ((AST_T*)args->items[0])->string_value;
    char* mode = ((AST_T*)args->items[1])->string_value;

    FILE* f = fopen(filename, mode);

    AST_T* ast_obj = init_ast(AST_OBJECT);
    ast_obj->variable_type = init_ast(AST_TYPE);
    ast_obj->variable_type->type_value = "file";
    ast_obj->object_value = f;

    AST_T* fdef_read = init_ast(AST_FUNCTION_DEFINITION);
    fdef_read->function_name = "read";
    fdef_read->fptr = object_file_function_read;

    ast_obj->function_definitions = init_dynamic_list(sizeof(struct AST_STRUCT*));
    dynamic_list_append(ast_obj->function_definitions, fdef_read);

    return ast_obj;
}

/**
 * Built-in function to close file.
 *
 * @param AST_T* self
 * @param dynamic_list_T* args
 *
 * @return AST_T*
 */
AST_T* hermes_builtin_function_fclose(AST_T* self, dynamic_list_T* args)
{
    runtime_expect_args(args, 1, (int[]) {AST_OBJECT});

    AST_T* return_ast = init_ast(AST_INTEGER);
    FILE* f = (FILE*) ((AST_T*)args->items[0])->object_value;
    return_ast->int_value = 1;
    fclose(f);

    return return_ast;
}

/**
 * Built-in function to write string to file.
 *
 * @param AST_T* self
 * @param dynamic_list_T* args
 *
 * @return AST_T*
 */
AST_T* hermes_builtin_function_fputs(AST_T* self, dynamic_list_T* args)
{
    runtime_expect_args(args, 2, (int[]) {AST_STRING, AST_OBJECT});

    AST_T* return_ast = init_ast(AST_INTEGER);
    return_ast->int_value = 1;

    char* line = ((AST_T*)args->items[0])->string_value;
    FILE* f = (FILE*) ((AST_T*)args->items[1])->object_value;

    fputs(line, f);

    return return_ast;
}

/**
 * Built-in function to read user input from stdin.
 *
 * @param AST_T* self
 * @param dynamic_list_T* args
 *
 * @return AST_T*
 */
AST_T* hermes_builtin_function_input(AST_T* self, dynamic_list_T* args)
{
    char* str;
    int c;
    int i;
    int size = 10;

    str = malloc(size * sizeof(char));
    
    if (args->size > 0)
        printf("%s", ((AST_T*)args->items[0])->string_value);

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

    str[i] = '\0';

    AST_T* ast = init_ast(AST_STRING);
    ast->string_value = str;

    return ast;
}


static char* my_itoa(int num, char *str)
{
    if(str == NULL)
        return NULL;

    sprintf(str, "%d", num);
    return str;
}

AST_T* hermes_builtin_function_char_to_bin(AST_T* self, dynamic_list_T* args)
{
    runtime_expect_args(args, 1, (int[]) {AST_CHAR});

    AST_T* ast_char = args->items[0];
    char c = ast_char->char_value;

    char* tmp_buffer = calloc(8, sizeof(char));
    int i;
    for(i=0;i<7;i++)
    {
        char* str = calloc(2, sizeof(char));
        my_itoa(c%2, str);
        strcat(tmp_buffer, str);
        c=c/2;      
    }

    char* output = calloc(8, sizeof(char));

    for (int i = 8; i >= 0; i--)
    {
        char charv = tmp_buffer[i];
        char* str = hermes_char_to_string(charv);
        strcat(output, str);
    }

    free(tmp_buffer);

    AST_T* ast_string = init_ast(AST_STRING);
    ast_string->string_value = calloc(strlen(output) + 1, sizeof(char));
    strcpy(ast_string->string_value, output);

    return ast_string;
}

AST_T* hermes_builtin_function_char_to_oct(AST_T* self, dynamic_list_T* args)
{
    runtime_expect_args(args, 1, (int[]) {AST_CHAR});

    AST_T* ast_char = args->items[0];
    char c = ast_char->char_value;

    char* tmp_buffer = calloc(4, sizeof(char));
    int i;
    for(i = 0; i < 3; i++)
    {
        char* str = calloc(2, sizeof(char));
        my_itoa(c%8, str);
        strcat(tmp_buffer, str);
        c=c/8; 
    }

    char* output = calloc(4, sizeof(char));

    for (int i = 4; i >= 0; i--)
    {
        char charv = tmp_buffer[i];
        char* str = hermes_char_to_string(charv);
        strcat(output, str);
    }

    free(tmp_buffer);

    AST_T* ast_string = init_ast(AST_STRING);
    ast_string->string_value = calloc(strlen(output) + 1, sizeof(char));
    strcpy(ast_string->string_value, output);

    return ast_string;
}

AST_T* hermes_builtin_function_char_to_dec(AST_T* self, dynamic_list_T* args)
{
    runtime_expect_args(args, 1, (int[]) {AST_CHAR});

    AST_T* ast_char = args->items[0];
    char c = ast_char->char_value;

    AST_T* ast_int = init_ast(AST_INTEGER);
    ast_int->int_value = (int) c;

    return ast_int;
}

AST_T* hermes_builtin_function_char_to_hex(AST_T* self, dynamic_list_T* args)
{
    runtime_expect_args(args, 1, (int[]) {AST_CHAR});

    AST_T* ast_char = args->items[0];
    char c = ast_char->char_value;

    char* str = calloc(8, sizeof(char));
    sprintf(str, "%x", c);

    AST_T* ast_string = init_ast(AST_STRING);
    ast_string->string_value = str;

    return ast_string;
}
