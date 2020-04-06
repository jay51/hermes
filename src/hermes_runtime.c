#include "include/hermes_runtime.h"
#include "include/hermes_builtins.h"
#include "include/dl.h"
#include "include/token.h"
#include <string.h>


hermes_scope_T* get_scope(runtime_T* runtime, AST_T* node)
{
    if (!node->scope)
        return runtime->scope;

    return (hermes_scope_T*) node->scope;
}

static void _multiple_variable_definitions_error(int line_n, char* variable_name)
{
    printf("[Line %d]: The variable `%s` is already defined.\n", line_n, variable_name);
    exit(1);
}

static AST_T* list_add_fptr(AST_T* self, dynamic_list_T* args)
{
    for (int i = 0; i < args->size; i++)
        dynamic_list_append(self->list_children, args->items[i]);

    return self;
}

static AST_T* list_remove_fptr(AST_T* self, dynamic_list_T* args)
{
    runtime_expect_args(args, 1, (int[]){ AST_INTEGER });

    AST_T* ast_int = (AST_T*) args->items[0];

    if (ast_int->int_value > self->list_children->size)
    {
        printf("Index out of range\n");
        exit(1);
    }

    dynamic_list_remove(self->list_children, self->list_children->items[ast_int->int_value], (void*)0);

    return self;
}

static char* create_str(const char* str)
{
    char* newstr = calloc(strlen(str) + 1, sizeof(char));
    strcpy(newstr, str);

    return newstr;
}

static void collect_and_sweep_garbage(dynamic_list_T* old_def_list, hermes_scope_T* scope)
{
    dynamic_list_T* garbage = init_dynamic_list(sizeof(AST_T*));

    for (int i = 0; i < scope->variable_definitions->size; i++)
    {
        AST_T* new_def = scope->variable_definitions->items[i];
        unsigned int exists = 0;
        
        for (int j = 0; j < old_def_list->size; j++)
        {
            AST_T* old_def = old_def_list->items[j];


            if (old_def == new_def)
            {
                exists = 1;
            } 
        }

        if (!exists)
            dynamic_list_append(garbage, new_def);
    }

    for (int i = 0; i < garbage->size; i++)
    {
        dynamic_list_remove(
            scope->variable_definitions,
            (AST_T*) garbage->items[i],
            _ast_free
        );
    }

    if (old_def_list->items != (void*)0)
        free(old_def_list->items);
    free(old_def_list);

    if (garbage->items != (void*)0)
        free(garbage->items);
    free(garbage);
}

static AST_T* _runtime_function_call(runtime_T* runtime, AST_T* fcall, AST_T* fdef)
{
    hermes_scope_T* function_definition_body_scope = (hermes_scope_T*) fdef->function_definition_body->scope;

    // Clear all existing arguments to prepare for the new definitions
    for (int i = function_definition_body_scope->variable_definitions->size-1; i > 0; i--)
    {
        dynamic_list_remove(
            function_definition_body_scope->variable_definitions,
            function_definition_body_scope->variable_definitions->items[i],
            _ast_free
        );

        function_definition_body_scope->variable_definitions->size = 0;
    }

    for (int x = 0; x < fcall->function_call_arguments->size; x++)
    {
        AST_T* ast_arg = (AST_T*) fcall->function_call_arguments->items[x];

        if (x > fdef->function_definition_arguments->size - 1)
        {
            printf("Error: [Line %d] Too many arguments\n", ast_arg->line_n);
	    exit(1);
            break;
        }

        AST_T* ast_fdef_arg = (AST_T*) fdef->function_definition_arguments->items[x];
        char* arg_name = ast_fdef_arg->variable_name;

        AST_T* new_variable_def = init_ast(AST_VARIABLE_DEFINITION);

        if (ast_arg->type == AST_VARIABLE)
        {
            AST_T* vdef = get_variable_definition_by_name(
                runtime,
                (hermes_scope_T*) get_scope(runtime, ast_arg),
                ast_arg->variable_name
            );
            
            if (vdef)
            {
                new_variable_def->variable_value = vdef->variable_value;
            }
        }
        
        if (new_variable_def->variable_value == (void*)0)
            new_variable_def->variable_value = runtime_visit(runtime, ast_arg);

        new_variable_def->variable_name = arg_name;

        dynamic_list_append(function_definition_body_scope->variable_definitions, new_variable_def);
    }

    return runtime_visit(runtime, fdef->function_definition_body);
}

runtime_T* init_runtime()
{
    runtime_T* runtime = calloc(1, sizeof(struct RUNTIME_STRUCT));
    runtime->scope = init_hermes_scope(1);
    runtime->list_methods = init_dynamic_list(sizeof(struct AST_STRUCT*));
    runtime->stdout_buffer = (void*)0;

    INITIALIZED_NOOP = init_ast(AST_NOOP);

    // GLOBAL FUNCTIONS
    
    AST_T* INCLUDE_FUNCTION_DEFINITION = init_ast(AST_FUNCTION_DEFINITION);
    INCLUDE_FUNCTION_DEFINITION->function_name = create_str("include");
    INCLUDE_FUNCTION_DEFINITION->fptr = hermes_builtin_function_include;
    dynamic_list_append(runtime->scope->function_definitions, INCLUDE_FUNCTION_DEFINITION);

    AST_T* WAD_FUNCTION_DEFINITION = init_ast(AST_FUNCTION_DEFINITION);
    WAD_FUNCTION_DEFINITION->function_name = create_str("wad");
    WAD_FUNCTION_DEFINITION->fptr = hermes_builtin_function_wad;
    dynamic_list_append(runtime->scope->function_definitions, WAD_FUNCTION_DEFINITION);

    AST_T* LAD_FUNCTION_DEFINITION = init_ast(AST_FUNCTION_DEFINITION);
    LAD_FUNCTION_DEFINITION->function_name = create_str("lad");
    LAD_FUNCTION_DEFINITION->fptr = hermes_builtin_function_lad;
    dynamic_list_append(runtime->scope->function_definitions, LAD_FUNCTION_DEFINITION);

    AST_T* PRINT_FUNCTION_DEFINITION = init_ast(AST_FUNCTION_DEFINITION);
    PRINT_FUNCTION_DEFINITION->function_name = create_str("print");
    PRINT_FUNCTION_DEFINITION->fptr = hermes_builtin_function_print;
    dynamic_list_append(runtime->scope->function_definitions, PRINT_FUNCTION_DEFINITION);

    AST_T* PPRINT_FUNCTION_DEFINITION = init_ast(AST_FUNCTION_DEFINITION);
    PPRINT_FUNCTION_DEFINITION->function_name = create_str("aprint");
    PPRINT_FUNCTION_DEFINITION->fptr = hermes_builtin_function_aprint;
    dynamic_list_append(runtime->scope->function_definitions, PPRINT_FUNCTION_DEFINITION);

    AST_T* FOPEN_FUNCTION_DEFINITION = init_ast(AST_FUNCTION_DEFINITION);
    FOPEN_FUNCTION_DEFINITION->function_name = create_str("fopen");
    FOPEN_FUNCTION_DEFINITION->fptr = hermes_builtin_function_fopen;
    dynamic_list_append(runtime->scope->function_definitions, FOPEN_FUNCTION_DEFINITION);

    AST_T* FCLOSE_FUNCTION_DEFINITION = init_ast(AST_FUNCTION_DEFINITION);
    FCLOSE_FUNCTION_DEFINITION->function_name = create_str("fclose");
    FCLOSE_FUNCTION_DEFINITION->fptr = hermes_builtin_function_fclose;
    dynamic_list_append(runtime->scope->function_definitions, FCLOSE_FUNCTION_DEFINITION);

    AST_T* FPUTS_FUNCTION_DEFINITION = init_ast(AST_FUNCTION_DEFINITION);
    FPUTS_FUNCTION_DEFINITION->function_name = create_str("fputs");
    FPUTS_FUNCTION_DEFINITION->fptr = hermes_builtin_function_fputs;
    dynamic_list_append(runtime->scope->function_definitions, FPUTS_FUNCTION_DEFINITION);

    AST_T* INPUT_FUNCTION_DEFINITION = init_ast(AST_FUNCTION_DEFINITION);
    INPUT_FUNCTION_DEFINITION->function_name = create_str("input");
    INPUT_FUNCTION_DEFINITION->fptr = hermes_builtin_function_input;
    dynamic_list_append(runtime->scope->function_definitions, INPUT_FUNCTION_DEFINITION);

    AST_T* CHAR_TO_BIN_FUNCTION_DEFINITION = init_ast(AST_FUNCTION_DEFINITION);
    CHAR_TO_BIN_FUNCTION_DEFINITION->function_name = create_str("char_to_bin");
    CHAR_TO_BIN_FUNCTION_DEFINITION->fptr = hermes_builtin_function_char_to_bin;
    dynamic_list_append(runtime->scope->function_definitions, CHAR_TO_BIN_FUNCTION_DEFINITION);
    
    AST_T* CHAR_TO_OCT_FUNCTION_DEFINITION = init_ast(AST_FUNCTION_DEFINITION);
    CHAR_TO_OCT_FUNCTION_DEFINITION->function_name = create_str("char_to_oct");
    CHAR_TO_OCT_FUNCTION_DEFINITION->fptr = hermes_builtin_function_char_to_oct;
    dynamic_list_append(runtime->scope->function_definitions, CHAR_TO_OCT_FUNCTION_DEFINITION);

    AST_T* CHAR_TO_DEC_FUNCTION_DEFINITION = init_ast(AST_FUNCTION_DEFINITION);
    CHAR_TO_DEC_FUNCTION_DEFINITION->function_name = create_str("char_to_dec");
    CHAR_TO_DEC_FUNCTION_DEFINITION->fptr = hermes_builtin_function_char_to_dec;
    dynamic_list_append(runtime->scope->function_definitions, CHAR_TO_DEC_FUNCTION_DEFINITION);

    AST_T* CHAR_TO_HEX_FUNCTION_DEFINITION = init_ast(AST_FUNCTION_DEFINITION);
    CHAR_TO_HEX_FUNCTION_DEFINITION->function_name = create_str("char_to_hex");
    CHAR_TO_HEX_FUNCTION_DEFINITION->fptr = hermes_builtin_function_char_to_hex;
    dynamic_list_append(runtime->scope->function_definitions, CHAR_TO_HEX_FUNCTION_DEFINITION);

    // LIST FUNCTIONS

    AST_T* LIST_ADD_FUNCTION_DEFINITION = init_ast(AST_FUNCTION_DEFINITION);
    LIST_ADD_FUNCTION_DEFINITION->function_name = create_str("add");
    LIST_ADD_FUNCTION_DEFINITION->fptr = list_add_fptr;
    dynamic_list_append(runtime->list_methods, LIST_ADD_FUNCTION_DEFINITION);

    AST_T* LIST_REMOVE_FUNCTION_DEFINITION = init_ast(AST_FUNCTION_DEFINITION);
    LIST_REMOVE_FUNCTION_DEFINITION->function_name = create_str("remove");
    LIST_REMOVE_FUNCTION_DEFINITION->fptr = list_remove_fptr;
    dynamic_list_append(runtime->list_methods, LIST_REMOVE_FUNCTION_DEFINITION);

    return runtime;
}

AST_T* runtime_visit(runtime_T* runtime, AST_T* node)
{
    if (!node)
        return (void*) 0;

    switch (node->type)
    {
        case AST_OBJECT: return runtime_visit_object(runtime, node); break;
        case AST_ENUM: return runtime_visit_enum(runtime, node); break;
        case AST_VARIABLE: return runtime_visit_variable(runtime, node); break;
        case AST_VARIABLE_DEFINITION: return runtime_visit_variable_definition(runtime, node); break;
        case AST_VARIABLE_ASSIGNMENT: return runtime_visit_variable_assignment(runtime, node); break;
        case AST_VARIABLE_MODIFIER: return runtime_visit_variable_modifier(runtime, node); break;
        case AST_FUNCTION_DEFINITION: return runtime_visit_function_definition(runtime, node); break;
        case AST_FUNCTION_CALL: return runtime_visit_function_call(runtime, node); break;
        case AST_NULL: return runtime_visit_null(runtime, node); break;
        case AST_STRING: return runtime_visit_string(runtime, node); break;
        case AST_CHAR: return runtime_visit_char(runtime, node); break;
        case AST_FLOAT: return runtime_visit_float(runtime, node); break;
        case AST_LIST: return runtime_visit_list(runtime, node); break;
        case AST_BOOLEAN: return runtime_visit_boolean(runtime, node); break;
        case AST_INTEGER: return runtime_visit_integer(runtime, node); break;
        case AST_COMPOUND: return runtime_visit_compound(runtime, node); break;
        case AST_TYPE: return runtime_visit_type(runtime, node); break;
        case AST_ATTRIBUTE_ACCESS: return runtime_visit_attribute_access(runtime, node); break;
        case AST_LIST_ACCESS: return runtime_visit_list_access(runtime, node); break;
        case AST_BINOP: return runtime_visit_binop(runtime, node); break;
        case AST_NOOP: return runtime_visit_noop(runtime, node); break;
        case AST_BREAK: return runtime_visit_break(runtime, node); break;
        case AST_CONTINUE: return runtime_visit_continue(runtime, node); break;
        case AST_RETURN: return runtime_visit_return(runtime, node); break;
        case AST_IF: return runtime_visit_if(runtime, node); break;
        case AST_WHILE: return runtime_visit_while(runtime, node); break;
        case AST_NEW: return runtime_visit_new(runtime, node); break;
        case AST_ITERATE: return runtime_visit_iterate(runtime, node); break;
        case AST_ASSERT: return runtime_visit_assert(runtime, node); break;
        default: printf("Uncaught statement %d\n", node->type); exit(1); break;
    }
}

/* ==== helpers ==== */

unsigned int _boolean_evaluation(AST_T* node)
{
    switch (node->type)
    {
        case AST_INTEGER: return node->int_value > 0; break;
        case AST_FLOAT: return node->float_value > 0; break;
        case AST_BOOLEAN: return node->boolean_value; break;
        case AST_STRING: return strlen(node->string_value) > 0; break;
        default: return 0; break;
    }
}

AST_T* get_variable_definition_by_name(runtime_T* runtime, hermes_scope_T* scope, char* variable_name)
{
    if (scope->owner)
    {
        if (strcmp(variable_name, "this") == 0)
        {
            if (scope->owner->parent)
                return scope->owner->parent;

            return scope->owner;
        }
    }

    for (int i = 0; i < scope->variable_definitions->size; i++)
    {
        AST_T* variable_definition = (AST_T*) scope->variable_definitions->items[i];

        if (strcmp(variable_definition->variable_name, variable_name) == 0)
            return variable_definition;
    }

    return (void*) 0;
}

/* ==== end of helpers ==== */

AST_T* runtime_visit_variable(runtime_T* runtime, AST_T* node)
{
    hermes_scope_T* local_scope = (hermes_scope_T*) node->scope;
    hermes_scope_T* global_scope = runtime->scope;

    if (node->object_children != (void*) 0)
    {
        if (node->object_children->size > 0)
        {
            for (int i = 0; i < node->object_children->size; i++)
            {
                AST_T* object_var_def = (AST_T*) node->object_children->items[i];

                if (object_var_def->type != AST_VARIABLE_DEFINITION)
                    continue;

                if (strcmp(object_var_def->variable_name, node->variable_name) == 0)
                {
                    if (!object_var_def->variable_value)
                        return object_var_def;

                    return runtime_visit(runtime, object_var_def->variable_value);
                }
            }
        }
    }
    else
    if (node->enum_children != (void*)0)
    {
        if (node->enum_children->size > 0)
        {
            for (int i = 0; i < node->enum_children->size; i++)
            {
                AST_T* variable = (AST_T*) node->enum_children->items[i];

                if (strcmp(variable->variable_name, node->variable_name) == 0)
                {
                    if (variable->ast != (void*)0)
                    {
                        return variable->ast;
                    }
                    else
                    {
                        AST_T* int_ast = init_ast(AST_INTEGER);
                        int_ast->int_value = i;
                        variable->ast = int_ast;

                        return variable->ast;
                    }
                }
            }
        }
    }

    if (local_scope)
    {
        AST_T* variable_definition = get_variable_definition_by_name(runtime, local_scope, node->variable_name);

        if (variable_definition != (void*) 0)
        {
            if (variable_definition->type != AST_VARIABLE_DEFINITION)
                return variable_definition;

            if (variable_definition)
            {
                return runtime_visit(runtime, variable_definition->variable_value);
            }
        }
    }

    if (!node->is_object_child) // nope
    {
        if (global_scope)
        {
            AST_T* variable_definition = get_variable_definition_by_name(runtime, global_scope, node->variable_name);

            if (variable_definition != (void*) 0)
            {
                if (variable_definition->type != AST_VARIABLE_DEFINITION)
                    return variable_definition;

                if (variable_definition)
                {
                    return runtime_visit(runtime, variable_definition->variable_value);
                }
            }
        }
    }

    printf("Error: [Line %d] Undefined variable %s\n", node->line_n, node->variable_name); exit(1);
}

AST_T* runtime_visit_variable_definition(runtime_T* runtime, AST_T* node)
{ 
    AST_T* vardef_global = get_variable_definition_by_name(
        runtime,
        runtime->scope,
        node->variable_name
    );

    if (vardef_global != (void*) 0)
    {
        _multiple_variable_definitions_error(node->line_n, node->variable_name);
    }
   
    if (node->scope)
    {
        AST_T* vardef_local = get_variable_definition_by_name(
            runtime,
            (hermes_scope_T*) node->scope,
            node->variable_name
        );

        if (vardef_local != (void*) 0)
        {
            _multiple_variable_definitions_error(node->line_n, node->variable_name);
        }
    }

    if (node->saved_function_call != (void*) 0)
    {
        node->variable_value = runtime_visit(runtime, node->saved_function_call);
    }
    else
    {
        if (node->variable_value)
        {
            if(node->is_ternary){
                if (node->variable_value->type == AST_FUNCTION_CALL)
                    node->saved_function_call = node->variable_value;

                AST_T* tmp = runtime_visit(runtime, node->variable_value);

                switch(tmp->type){

                    case AST_OBJECT:
                        if (node->ternary_true->type == AST_FUNCTION_CALL)
                            node->saved_function_call = node->ternary_true;

                        node->variable_value = runtime_visit(runtime, node->ternary_true);
                        break;

                    case AST_STRING:
                        if (node->ternary_true->type == AST_FUNCTION_CALL)
                            node->saved_function_call = node->ternary_true;

                        node->variable_value = runtime_visit(runtime, node->ternary_true);
                        break;

                    case AST_CHAR:
                        if (node->ternary_true->type == AST_FUNCTION_CALL)
                            node->saved_function_call = node->ternary_true;

                        node->variable_value = runtime_visit(runtime, node->ternary_true);
                        break;

                    case AST_FLOAT:
                        if (node->ternary_true->type == AST_FUNCTION_CALL)
                            node->saved_function_call = node->ternary_true;

                        node->variable_value = runtime_visit(runtime, node->ternary_true);
                        break;

                    case AST_LIST:
                        if (node->ternary_true->type == AST_FUNCTION_CALL)
                            node->saved_function_call = node->ternary_true;

                        node->variable_value = runtime_visit(runtime, node->ternary_true);
                        break;

                    case AST_INTEGER:
                        if (node->ternary_true->type == AST_FUNCTION_CALL)
                            node->saved_function_call = node->ternary_true;

                        node->variable_value = runtime_visit(runtime, node->ternary_true);
                        break;

                    case AST_BOOLEAN:
                        if(tmp->boolean_value)
                        {
                            if (node->ternary_true->type == AST_FUNCTION_CALL)
                                node->saved_function_call = node->ternary_true;

                            node->variable_value = runtime_visit(runtime, node->ternary_true);
                            break;
                        }
                        // else use default case
                    case AST_NULL:
                    default:
                        // else it's falsey
                        if (node->ternary_false->type == AST_FUNCTION_CALL)
                            node->saved_function_call = node->ternary_false;

                        node->variable_value = runtime_visit(runtime, node->ternary_false);
                }

            }
            else {

                if (node->variable_value->type == AST_FUNCTION_CALL)
                    node->saved_function_call = node->variable_value;

                node->variable_value = runtime_visit(runtime, node->variable_value);
            }

        }
        else
        {
            node->variable_value = init_ast(AST_NULL);
        }
    }

    dynamic_list_append(get_scope(runtime, node)->variable_definitions, node);

    if (node->variable_value)
    {
        return node->variable_value;
    }
    else
    {
        return node;
    }
}

AST_T* runtime_visit_variable_assignment(runtime_T* runtime, AST_T* node)
{
    AST_T* value = (void*) 0;

    AST_T* left = node->variable_assignment_left;
    hermes_scope_T* local_scope = (hermes_scope_T*) node->scope;
    hermes_scope_T* global_scope = runtime->scope;

    if (node->object_children != (void*) 0)
    {
        if (node->object_children->size > 0)
        {
            for (int i = 0; i < node->object_children->size; i++)
            {
                AST_T* object_var_def = (AST_T*) node->object_children->items[i];

                if (object_var_def->type != AST_VARIABLE_DEFINITION)
                    continue;

                if (strcmp(object_var_def->variable_name, left->variable_name) == 0)
                {
                    value = runtime_visit(runtime, node->variable_value);

                    if (value->type == AST_FLOAT)
                        value->int_value = (int) value->float_value;

                    object_var_def->variable_value = value;
                    return value;
                }
            }
        }
    }

    if (local_scope != (void*) 0)
    {
        AST_T* variable_definition = get_variable_definition_by_name(
            runtime,
            local_scope,
            left->variable_name
        );

        if (variable_definition != (void*) 0)
        {
            value = runtime_visit(runtime, node->variable_value);

            if (value->type == AST_FLOAT)
                value->int_value = (int) value->float_value;

            variable_definition->variable_value = value;
            return value;
        }
    }

    if (global_scope != (void*) 0)
    {
        AST_T* variable_definition = get_variable_definition_by_name(
            runtime,
            global_scope,
            left->variable_name
        );

        if (variable_definition != (void*) 0)
        {

            if(node->is_ternary)
            {

                value = runtime_visit(runtime, node->variable_value);

                switch(value->type){

                    case AST_OBJECT:
                        if (node->ternary_true->type == AST_FUNCTION_CALL)
                            node->saved_function_call = node->ternary_true;

                        variable_definition->variable_value = runtime_visit(runtime, node->ternary_true);
                        break;

                    case AST_STRING:
                        if (node->ternary_true->type == AST_FUNCTION_CALL)
                            node->saved_function_call = node->ternary_true;

                        variable_definition->variable_value = runtime_visit(runtime, node->ternary_true);
                        break;

                    case AST_CHAR:
                        if (node->ternary_true->type == AST_FUNCTION_CALL)
                            node->saved_function_call = node->ternary_true;

                        variable_definition->variable_value = runtime_visit(runtime, node->ternary_true);
                        break;

                    case AST_FLOAT:
                        if (node->ternary_true->type == AST_FUNCTION_CALL)
                            node->saved_function_call = node->ternary_true;

                        variable_definition->variable_value = runtime_visit(runtime, node->ternary_true);
                        break;

                    case AST_LIST:
                        if (node->ternary_true->type == AST_FUNCTION_CALL)
                            node->saved_function_call = node->ternary_true;

                        variable_definition->variable_value = runtime_visit(runtime, node->ternary_true);
                        break;

                    case AST_INTEGER:
                        if (node->ternary_true->type == AST_FUNCTION_CALL)
                            node->saved_function_call = node->ternary_true;

                        variable_definition->variable_value = runtime_visit(runtime, node->ternary_true);
                        break;

                    case AST_BOOLEAN:
                        if(value->boolean_value)
                        {
                            if (node->ternary_true->type == AST_FUNCTION_CALL)
                                node->saved_function_call = node->ternary_true;

                            variable_definition->variable_value = runtime_visit(runtime, node->ternary_true);
                            break;
                        }
                        // else use default case
                    case AST_NULL:
                    default:
                        // else it's falsey
                        if (node->ternary_false->type == AST_FUNCTION_CALL)
                            node->saved_function_call = node->ternary_false;

                        variable_definition->variable_value = runtime_visit(runtime, node->ternary_false);
                }

                return value;
            }
            else
            {

                value = runtime_visit(runtime, node->variable_value);

                if (value->type == AST_FLOAT)
                    value->int_value = (int) value->float_value;

                variable_definition->variable_value = value;
                return value;

            }
        }
    }

    printf("Error: [Line %d] Cant set undefined variable `%s`\n", left->line_n, left->variable_name); exit(1);
}

AST_T* runtime_visit_variable_modifier(runtime_T* runtime, AST_T* node)
{
    AST_T* value = (void*) 0;

    AST_T* left = node->binop_left;
    hermes_scope_T* variable_scope = get_scope(runtime, node);

    for (int i = 0; i < variable_scope->variable_definitions->size; i++)
    {
        AST_T* ast_variable_definition = (AST_T*) variable_scope->variable_definitions->items[i];

        if (node->object_children != (void*) 0)
        {
            for (int i = 0; i < node->object_children->size; i++)
            {
                AST_T* object_var_def = (AST_T*) node->object_children->items[i];

                if (object_var_def->type != AST_VARIABLE_DEFINITION)
                    continue;

                if (strcmp(object_var_def->variable_name, left->variable_name) == 0)
                {
                    ast_variable_definition = object_var_def;
                    break;
                }
            }
        }

        if (strcmp(ast_variable_definition->variable_name, left->variable_name) == 0)
        {
            value = runtime_visit(runtime, node->binop_right);

            switch (node->binop_operator->type)
            {
                case TOKEN_PLUS_EQUALS: {
                    if (strcmp(ast_variable_definition->variable_type->type_value, "int") == 0)
                    {
                        ast_variable_definition->variable_value->int_value += value->int_value ? value->int_value : value->float_value;
                        ast_variable_definition->variable_value->float_value = (float) ast_variable_definition->variable_value->int_value;
                        return ast_variable_definition->variable_value;
                    }
                    else if (strcmp(ast_variable_definition->variable_type->type_value, "float") == 0)
                    {
                        ast_variable_definition->variable_value->float_value += value->float_value ? value->float_value : value->int_value;
                        ast_variable_definition->variable_value->int_value = (int) ast_variable_definition->variable_value->float_value;
                        return ast_variable_definition->variable_value;
                    }
                } break;
                case TOKEN_MINUS_EQUALS: {
                    if (strcmp(ast_variable_definition->variable_type->type_value, "int") == 0)
                    {
                        ast_variable_definition->variable_value->int_value -= value->int_value ? value->int_value : value->float_value;
                        ast_variable_definition->variable_value->float_value = (float) ast_variable_definition->variable_value->int_value;
                        return ast_variable_definition->variable_value;
                    }
                    else if (strcmp(ast_variable_definition->variable_type->type_value, "float") == 0)
                    {
                        ast_variable_definition->variable_value->float_value -= value->float_value ? value->float_value : value->int_value;
                        ast_variable_definition->variable_value->int_value = (int) ast_variable_definition->variable_value->float_value;
                        return ast_variable_definition->variable_value;
                    }
                } break;
                case TOKEN_STAR_EQUALS: {
                    if (strcmp(ast_variable_definition->variable_type->type_value, "int") == 0)
                    {
                        ast_variable_definition->variable_value->int_value *= value->int_value ? value->int_value : value->float_value;
                        ast_variable_definition->variable_value->float_value = (float) ast_variable_definition->variable_value->int_value;
                        return ast_variable_definition->variable_value;
                    }
                    else if (strcmp(ast_variable_definition->variable_type->type_value, "float") == 0)
                    {
                        ast_variable_definition->variable_value->float_value *= value->float_value ? value->float_value : value->int_value;
                        ast_variable_definition->variable_value->int_value = (int) ast_variable_definition->variable_value->float_value;
                        return ast_variable_definition->variable_value;
                    }
                } break;
                default: {printf("Error: [Line %d] `%s` is not a valid operator\n", node->line_n, node->binop_operator->value); exit(1);} break;
            }
        }
    }

    printf("Error: [Line %d] Cant set undefined variable `%s`\n", node->line_n, node->variable_name); exit(1);
}

AST_T* runtime_visit_function_definition(runtime_T* runtime, AST_T* node)
{
    hermes_scope_T* scope = get_scope(runtime, node);
    dynamic_list_append(scope->function_definitions, node);

    return node;
}

AST_T* runtime_function_lookup(runtime_T* runtime, hermes_scope_T* scope, AST_T* node)
{
    AST_T* function_definition = (void*)0;

    /**
     * First, check if there is a variable definition assigned with a 
     * function definition.
     */
    for (int i = 0; i < scope->variable_definitions->size; i++)
    {
        AST_T* vardef = (AST_T*) scope->variable_definitions->items[i];

        if (strcmp(node->function_call_name, vardef->variable_name) == 0)
        {
            if (vardef->variable_value->type == AST_FUNCTION_DEFINITION)
            {
                function_definition = vardef->variable_value;
                break;
            }
        }
    }

    /**
     * If we did not find a variable definition assigned with a function
     * defintion, then keep looking in function definitions instead.
     */
    if (function_definition == (void*) 0)
    {
        for (int i = 0; i < scope->function_definitions->size; i++)
        {
            function_definition = (AST_T*) scope->function_definitions->items[i];
            
            if (strcmp(function_definition->function_name, node->function_call_name) == 0)
            {
                break; 
            }

            function_definition = (void*)0;
        }
    }

    if (function_definition == (void*)0)
        return (void*)0;

    if (function_definition->fptr)
    {
        dynamic_list_T* visited_fptr_args = init_dynamic_list(sizeof(struct AST_STRUCT*));

        for (int x = 0; x < node->function_call_arguments->size; x++)
        {
            AST_T* ast_arg = (AST_T*) node->function_call_arguments->items[x];
            AST_T* visited = (void*)0;

            if (ast_arg->type == AST_VARIABLE)
            {
                AST_T* vdef = get_variable_definition_by_name(
                    runtime,
                    (hermes_scope_T*) get_scope(runtime, ast_arg),
                    ast_arg->variable_name
                );
                
                if (vdef)
                {
                    visited = vdef->variable_value;
                }
            }

            visited = visited != (void*)0 ? visited : runtime_visit(runtime, ast_arg);
            dynamic_list_append(visited_fptr_args, visited);
        }

        AST_T* ret = runtime_visit(runtime, (AST_T*) function_definition->fptr((AST_T*) function_definition, visited_fptr_args));

        free(visited_fptr_args->items);
        free(visited_fptr_args);

        return ret;
    }

    if (function_definition->function_definition_body != (void*)0)
    {
        return _runtime_function_call(runtime, node, function_definition);
    }
    else
    if (function_definition->composition_children != (void*)0)
    {
        AST_T* final_result = init_ast(AST_NULL);
        char* type_value = function_definition->function_definition_type->type_value;

        if (strcmp(type_value, "int") == 0)
        {
            final_result->type = AST_INTEGER;
            final_result->int_value = 0;
        }
        else
        if (strcmp(type_value, "float") == 0)
        {
            final_result->type = AST_FLOAT;
            final_result->float_value = 0.0f;
        }
        else
        if (strcmp(type_value, "string") == 0)
        {
            final_result->type = AST_STRING;
            final_result->string_value = calloc(1, sizeof(char));
            final_result->string_value[0] = '\0';
        }

        dynamic_list_T* call_arguments = init_dynamic_list(sizeof(struct AST_STRUCT*));
        dynamic_list_append(call_arguments, final_result);

        for (int i = 0; i < function_definition->composition_children->size; i++)
        {
            AST_T* comp_child = (AST_T*) function_definition->composition_children->items[i];

            AST_T* result = (void*) 0;

            if (comp_child->type == AST_FUNCTION_DEFINITION)
            {
                if (i == 0)
                    node->function_call_arguments = node->function_call_arguments;
                else
                    node->function_call_arguments = call_arguments;

                result = _runtime_function_call(runtime, node, comp_child);
            }
            else
            {
                AST_T* fcall = init_ast(AST_FUNCTION_CALL);
                fcall->function_call_name = comp_child->variable_name;

                if (i == 0)
                    fcall->function_call_arguments = node->function_call_arguments;
                else
                    fcall->function_call_arguments = call_arguments;

                result = runtime_function_lookup(runtime, scope, fcall);
            }

            switch (result->type)
            {
                case AST_INTEGER: final_result->int_value = result->int_value; break;
                case AST_FLOAT: final_result->float_value = result->float_value; break;
                case AST_STRING: final_result->string_value = realloc(final_result->string_value, (strlen(result->string_value) + strlen(final_result->string_value) + 1) * sizeof(char)); strcat(final_result->string_value, result->string_value); break;
                default: /* silence */; break;
            }

            ast_free(result);
        }

        return final_result;
    }

    return (void*) 0;
}

AST_T* runtime_visit_function_call(runtime_T* runtime, AST_T* node)
{
    hermes_scope_T* scope = get_scope(runtime, node);

    // TODO: put this in the hermes_builtins.c,
    // this cannot be done right now because the function pointers does not
    // support the `scope` argument.
    if (strcmp(node->function_call_name, "dload") == 0)
    {
        AST_T* ast_arg_0 = (AST_T*) node->function_call_arguments->items[0];
        AST_T* visited_0 = runtime_visit(runtime, ast_arg_0);

        AST_T* fdef = INITIALIZED_NOOP;
        
        for (int i = 1; i < node->function_call_arguments->size; i++)
        {
            AST_T* ast_arg = (AST_T*) node->function_call_arguments->items[i];
            AST_T* visited_ast = runtime_visit(runtime, ast_arg);

            fdef = get_dl_function(visited_0->string_value, visited_ast->string_value);
            fdef->scope = (struct hermes_scope_T*) scope;

            runtime_visit(runtime, fdef);
        }

        return fdef;
    }

    if (strcmp(node->function_call_name, "stdoutbuffer") == 0)
    {
        for (int i = 0; i < node->function_call_arguments->size; i++)
        {
            AST_T* ast_arg = (AST_T*) runtime_visit(runtime, node->function_call_arguments->items[i]);
            char* str = ast_to_string(ast_arg);

            if (str == (void*)0)
            {
                hermes_runtime_buffer_stdout(runtime, "(void*)0\n");
            }
            else
            {
                str = realloc(str, (strlen(str) + 2) * sizeof(char));
                strcat(str, "\n");
                hermes_runtime_buffer_stdout(runtime, str);
                free(str);
            }
        }

        return INITIALIZED_NOOP;
    }

    if (strcmp(node->function_call_name, "free") == 0)
    {
        for (int i = 0; i < node->function_call_arguments->size; i++)
        {
            AST_T* arg = (AST_T*) node->function_call_arguments->items[i];

            if (arg->type != AST_VARIABLE)
                continue;

            for (int i = 0; i < scope->variable_definitions->size; i++)
            {
                AST_T* vardef = scope->variable_definitions->items[i];

                if (strcmp(vardef->variable_name, arg->variable_name) == 0)
                {
                    dynamic_list_remove(scope->variable_definitions, vardef, (void*)0);
                    break;
                }
            }
        }

        return INITIALIZED_NOOP;
    }

    if (strcmp(node->function_call_name, "visit") == 0)
    {
        AST_T* arg = (void*)0;

        for (int i = 0; i < node->function_call_arguments->size; i++)
            arg = runtime_visit(runtime, (AST_T*) node->function_call_arguments->items[i]);

        return arg;
    }  

    if (node->scope != (void*) 0)
    {
        AST_T* local_scope_func_def = runtime_function_lookup(
            runtime,
            (hermes_scope_T*) node->scope,
            node
        );

        if (local_scope_func_def)
            return local_scope_func_def;
    }

    AST_T* global_scope_func_def = runtime_function_lookup(
        runtime,
        runtime->scope,
        node
    );

    if (global_scope_func_def)
        return global_scope_func_def;

    printf("Error: [Line %d] Undefined method %s\n", node->line_n, node->function_call_name); exit(1);
}

AST_T* runtime_visit_null(runtime_T* runtime, AST_T* node)
{
    return node;
}

AST_T* runtime_visit_string(runtime_T* runtime, AST_T* node)
{
    return node;
}

AST_T* runtime_visit_char(runtime_T* runtime, AST_T* node)
{
    return node;
}

AST_T* runtime_visit_float(runtime_T* runtime, AST_T* node)
{
    return node;
}

AST_T* runtime_visit_object(runtime_T* runtime, AST_T* node)
{
    //for (int i = 0; i < node->object_children->size; i++)
    //    node->object_children->items[i] = runtime_visit(runtime, node->object_children->items[i]);

    return node;
}

AST_T* runtime_visit_enum(runtime_T* runtime, AST_T* node)
{
    return node;
}

AST_T* runtime_visit_list(runtime_T* runtime, AST_T* node)
{
    node->function_definitions = runtime->list_methods;
    return node;
}

AST_T* runtime_visit_boolean(runtime_T* runtime, AST_T* node)
{
    return node;
}

AST_T* runtime_visit_integer(runtime_T* runtime, AST_T* node)
{
    return node;
}

AST_T* runtime_visit_compound(runtime_T* runtime, AST_T* node)
{
    hermes_scope_T* scope = get_scope(runtime, node);

    dynamic_list_T* old_def_list = init_dynamic_list(sizeof(AST_T*));
        
    for (int i = 0; i < scope->variable_definitions->size; i++)
    {
        AST_T* vardef = scope->variable_definitions->items[i];
        dynamic_list_append(old_def_list, vardef);
    }

    for (int i = 0; i < node->compound_value->size; i++)
    {
        AST_T* child = (AST_T*) node->compound_value->items[i];

        if (!child)
            continue;

        AST_T* visited = runtime_visit(runtime, child);

        if (visited)
        {
            if (visited->type == AST_RETURN)
            {
                if (visited->return_value)
                {
                    AST_T* ret_val = runtime_visit(runtime, visited->return_value);

                    collect_and_sweep_garbage(old_def_list, scope);
                    return ret_val;
                }
                else
                {
                    collect_and_sweep_garbage(old_def_list, scope);
                    return (void*) 0;
                }
            }
            else
            if (visited->type == AST_BREAK || visited->type == AST_CONTINUE)
            {
                return visited;
            }
        }
    } 
    
    collect_and_sweep_garbage(old_def_list, scope);

    return node;
}

AST_T* runtime_visit_type(runtime_T* runtime, AST_T* node)
{
    return node;
}

AST_T* runtime_visit_attribute_access(runtime_T* runtime, AST_T* node)
{
    if (node->object_children)
        node->binop_left->object_children = node->object_children;

    AST_T* left = runtime_visit(runtime, node->binop_left);

    if (left->type == AST_LIST || left->type == AST_STRING)
    {
        if (node->binop_right->type == AST_VARIABLE)
        {
            if (strcmp(node->binop_right->variable_name, "length") == 0)
            {
                AST_T* int_ast = init_ast(AST_INTEGER);

                if (left->type == AST_LIST)
                {
                    int_ast->int_value = left->list_children->size;
                }
                else
                if (left->type == AST_STRING)
                {
                    int_ast->int_value = strlen(left->string_value);
                }

                return int_ast;
            }
        } 
    }
    else
    if (left->type == AST_OBJECT)
    {
        if (node->binop_right->type == AST_VARIABLE || node->binop_right->type == AST_VARIABLE_ASSIGNMENT || node->binop_right->type == AST_VARIABLE_MODIFIER || node->binop_right->type == AST_ATTRIBUTE_ACCESS)
        {
            node->binop_right->object_children = left->object_children;
            node->binop_right->scope = left->scope;
            node->binop_right->is_object_child = 1;
            node->object_children = left->object_children;
            node->scope = left->scope;
        }
    }
    else
    if (left->type == AST_ENUM)
    {
        if (node->binop_right->type == AST_VARIABLE)
        {
            node->binop_right->enum_children = left->enum_children;
            node->binop_right->scope = left->scope;
            // node->binop_right->is_enum_child = 1;
            node->enum_children = left->enum_children;
            node->scope = left->scope;
        }
    }

    // call a function attached to some sort of value.
    if (node->binop_right->type == AST_FUNCTION_CALL)
    {
        if (left->function_definitions != (void*)0)
        {
            for (int i = 0; i < left->function_definitions->size; i++)
            {
                AST_T* _fdef = left->function_definitions->items[i];

                if (strcmp(_fdef->function_name, node->binop_right->function_call_name) == 0)
                {
                    if (_fdef->fptr)
                    {
                        dynamic_list_T* visited_fptr_args = init_dynamic_list(sizeof(struct AST_STRUCT*));

                        for (int x = 0; x < node->binop_right->function_call_arguments->size; x++)
                        {
                            AST_T* ast_arg = (AST_T*) node->binop_right->function_call_arguments->items[x];
                            AST_T* visited = runtime_visit(runtime, ast_arg);
                            dynamic_list_append(visited_fptr_args, visited);
                        }

                        AST_T* ret = runtime_visit(runtime, (AST_T*) _fdef->fptr((AST_T*) left, visited_fptr_args));
                        return ret;
                    }

                }
            }
        }
        
        if (left->object_children != (void*)0)
        {
            for (int i = 0; i < left->object_children->size; i++)
            {
                AST_T* obj_child = (AST_T*) left->object_children->items[i];
                
                if (obj_child->type == AST_FUNCTION_DEFINITION)
                    if (strcmp(obj_child->function_name, node->binop_right->function_call_name) == 0)
                        return _runtime_function_call(runtime, node->binop_right, obj_child);
            }
        }
    }

    node->scope = (struct hermes_scope_T*) get_scope(runtime, left);
    
    AST_T* new = runtime_visit(runtime, node->binop_right);

    return runtime_visit(runtime, new);
}

AST_T* runtime_visit_list_access(runtime_T* runtime, AST_T* node)
{
    AST_T* left = runtime_visit(runtime, node->binop_left);

    if (left->type == AST_LIST)
    {
        return left->list_children->items[runtime_visit(runtime, node->list_access_pointer)->int_value];
    }
    else
    if (left->type == AST_STRING)
    {
        AST_T* ast = init_ast(AST_CHAR);
        ast->char_value = left->string_value[runtime_visit(runtime, node->list_access_pointer)->int_value];
        return ast;
    }

    printf("list_access left value is not iterable.\n");
    exit(1);
}

AST_T* runtime_visit_binop(runtime_T* runtime, AST_T* node)
{
    AST_T* return_value = (void*) 0;
    AST_T* left = runtime_visit(runtime, node->binop_left);
    AST_T* right = node->binop_right;

    if (node->binop_operator->type == TOKEN_DOT)
    {
        char* access_name = (void*) 0;

        switch (right->type)
        {
            case AST_VARIABLE: access_name = right->variable_name; break;
            case AST_FUNCTION_CALL: access_name = right->function_call_name; break;
            default: /* silence */; break;
        }

        if (right->type == AST_BINOP)
            right = runtime_visit(runtime, right);
        
        if (left->type == AST_OBJECT)
        {
            for (int i = 0; i < left->object_children->size; i++)
            {
                AST_T* child = runtime_visit(runtime, (AST_T*) left->object_children->items[i]);

                if (child->type == AST_VARIABLE_DEFINITION && right->type == AST_VARIABLE_ASSIGNMENT)
                {
                    child->variable_value = runtime_visit(runtime, right->variable_value);
                    return child->variable_value;
                }

                if (child->type == AST_VARIABLE_DEFINITION)
                {
                    if (strcmp(child->variable_name, access_name) == 0)
                    {
                        if (child->variable_value)
                        {
                            return runtime_visit(runtime, child->variable_value);
                        }
                        else
                        {
                            return child;
                        }
                    }
                }
                else
                if (child->type == AST_FUNCTION_DEFINITION)
                {
                    if (strcmp(child->function_name, access_name) == 0)
                    {
                        for (int x = 0; x < right->function_call_arguments->size; x++)
                        {
                            AST_T* ast_arg = (AST_T*) right->function_call_arguments->items[x];

                            if (x > child->function_definition_arguments->size - 1)
                            {
                                printf("Error: [Line %d] Too many arguments\n", ast_arg->line_n);
                                break;
                            }

                            AST_T* ast_fdef_arg = (AST_T*) child->function_definition_arguments->items[x];
                            char* arg_name = ast_fdef_arg->variable_name;

                            AST_T* new_variable_def = init_ast(AST_VARIABLE_DEFINITION);
                            new_variable_def->variable_value = runtime_visit(runtime, ast_arg);
                            new_variable_def->variable_name = arg_name;

                            dynamic_list_append(get_scope(runtime, child->function_definition_body)->variable_definitions, new_variable_def);
                        }
                        return runtime_visit(runtime, child->function_definition_body);
                    }
                }
            }
        }
    }

    right = runtime_visit(runtime, right);

    switch (node->binop_operator->type)
    {
        case TOKEN_PLUS: {
            if (left->type == AST_INTEGER && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_INTEGER);
                return_value->int_value = left->int_value + right->int_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_FLOAT);
                return_value->float_value = left->float_value + right->float_value;

                return return_value;
            }
            if (left->type == AST_INTEGER && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_FLOAT);
                return_value->float_value = left->int_value + right->float_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_FLOAT);
                return_value->float_value = left->float_value + right->int_value;

                return return_value;
            }
            if (left->type == AST_STRING && right->type == AST_STRING)
            {
                char* new_str = calloc(strlen(left->string_value) + strlen(right->string_value) + 1, sizeof(char));
                strcat(new_str, left->string_value);
                strcat(new_str, right->string_value);
                return_value = init_ast(AST_STRING);
                return_value->string_value = new_str;

                return return_value;
            }
            if (left->type == AST_STRING && right->type == AST_INTEGER)
            {
                const char* int_str_template = "%d";
                size_t int_padding = sizeof(char) * 4;
                char* int_str = calloc(strlen(int_str_template + 1 + int_padding), sizeof(char));
                sprintf(int_str, int_str_template, right->int_value);


                char* new_str = calloc(strlen(left->string_value) + strlen(int_str) + 1, sizeof(char));
                strcat(new_str, left->string_value);
                strcat(new_str, int_str);
                free(int_str);
                return_value = init_ast(AST_STRING);
                return_value->string_value = new_str;

                return return_value;
            }
            if (left->type == AST_INTEGER && right->type == AST_STRING)
            {
                const char* int_str_template = "%d";
                size_t int_padding = sizeof(char) * 4;
                char* int_str = calloc(strlen(int_str_template + 1 + int_padding), sizeof(char));
                sprintf(int_str, int_str_template, left->int_value);


                char* new_str = calloc(strlen(right->string_value) + strlen(int_str) + 1, sizeof(char));
                strcat(new_str, int_str);
                strcat(new_str, right->string_value);
                free(int_str);
                return_value = init_ast(AST_STRING);
                return_value->string_value = new_str;

                return return_value;
            }
        } break;
        case TOKEN_MINUS: {
            if (left->type == AST_INTEGER && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_INTEGER);
                return_value->int_value = left->int_value - right->int_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_FLOAT);
                return_value->float_value = left->float_value - right->float_value;

                return return_value;
            }
            if (left->type == AST_INTEGER && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_FLOAT);
                return_value->float_value = left->int_value - right->float_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_FLOAT);
                return_value->float_value = left->float_value - right->int_value;

                return return_value;
            }
        } break;
        case TOKEN_DIV: {
            if (left->type == AST_INTEGER && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_INTEGER);
                return_value->int_value = left->int_value / right->int_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_FLOAT);
                return_value->float_value = left->float_value / right->float_value;

                return return_value;
            }
            if (left->type == AST_INTEGER && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_FLOAT);
                return_value->float_value = left->int_value / right->float_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_FLOAT);
                return_value->float_value = left->float_value / right->int_value;

                return return_value;
            }
        } break;
        case TOKEN_STAR: {
            if (left->type == AST_INTEGER && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_INTEGER);
                return_value->int_value = left->int_value * right->int_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_FLOAT);
                return_value->float_value = left->float_value * right->float_value;

                return return_value;
            }
            if (left->type == AST_INTEGER && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_FLOAT);
                return_value->float_value = left->int_value * right->float_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_FLOAT);
                return_value->float_value = left->float_value * right->int_value;

                return return_value;
            }
        } break;
        case TOKEN_AND: {
            if (left->type == AST_INTEGER && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->int_value && right->int_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->float_value && right->float_value;

                return return_value;
            }
            if (left->type == AST_INTEGER && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->int_value && right->float_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->float_value && right->int_value;

                return return_value;
            }
        } break;
        case TOKEN_LESS_THAN: {
            if (left->type == AST_INTEGER && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->int_value < right->int_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->float_value < right->float_value;

                return return_value;
            }
            if (left->type == AST_INTEGER && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->int_value < right->float_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->float_value < right->int_value;

                return return_value;
            }
        } break;
        case TOKEN_LARGER_THAN: {
            if (left->type == AST_INTEGER && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->int_value > right->int_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->float_value > right->float_value;

                return return_value;
            }
            if (left->type == AST_INTEGER && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->int_value > right->float_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->float_value > right->int_value;

                return return_value;
            }
        } break;
        case TOKEN_EQUALS_EQUALS: {
            if (left->type == AST_INTEGER && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->int_value == right->int_value;

                return return_value;
            }
            if (left->type == AST_INTEGER && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->int_value == right->float_value;

                return return_value;
            }
            if (left->type == AST_INTEGER && right->type == AST_NULL)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->int_value == 0;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->float_value == right->float_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->float_value == right->int_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_NULL)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->float_value == 0;

                return return_value;
            }
            if (left->type == AST_STRING && right->type == AST_NULL)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->string_value == (void*) 0;

                return return_value;
            }
            if (left->type == AST_OBJECT && right->type == AST_NULL)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->object_children->size == 0;

                return return_value;
            }
            if (left->type == AST_NULL && right->type == AST_NULL)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = 1;

                return return_value;
            }
        } break;
        case TOKEN_NOT_EQUALS: {
            if (left->type == AST_INTEGER && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->int_value != right->int_value;

                return return_value;
            }
            if (left->type == AST_INTEGER && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->int_value != right->float_value;

                return return_value;
            }
            if (left->type == AST_INTEGER && right->type == AST_NULL)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->int_value != 0;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_FLOAT)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->float_value != right->float_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_INTEGER)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->float_value != right->int_value;

                return return_value;
            }
            if (left->type == AST_FLOAT && right->type == AST_NULL)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->float_value != 0;

                return return_value;
            }
            if (left->type == AST_STRING && right->type == AST_NULL)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->string_value != (void*) 0;

                return return_value;
            }
            if (left->type == AST_OBJECT && right->type == AST_NULL)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = left->object_children->size > 0;

                return return_value;
            }
            if (left->type == AST_NULL && right->type == AST_NULL)
            {
                return_value = init_ast(AST_BOOLEAN);
                return_value->boolean_value = 0;

                return return_value;
            }
        } break;
        default: {printf("Error: [Line %d] `%s` is not a valid operator\n", node->line_n, node->binop_operator->value); exit(1);} break;
    }

    return node;
}

AST_T* runtime_visit_noop(runtime_T* runtime, AST_T* node)
{
    return node;
}

AST_T* runtime_visit_break(runtime_T* runtime, AST_T* node)
{
    return node;
}

AST_T* runtime_visit_continue(runtime_T* runtime, AST_T* node)
{
    return node;
}

AST_T* runtime_visit_return(runtime_T* runtime, AST_T* node)
{
    return node;
}

AST_T* runtime_visit_if(runtime_T* runtime, AST_T* node)
{
    if (!node->if_expr)
        return runtime_visit(runtime, node->if_body);

    if (_boolean_evaluation(runtime_visit(runtime, node->if_expr)))
    {
        return runtime_visit(runtime, node->if_body);
    }
    else
    {
        if (node->if_otherwise)
        {
            return runtime_visit(runtime, node->if_otherwise);
        }

        if (node->else_body) {
            return runtime_visit(runtime, node->else_body);
        }
    }

    return node;
}

AST_T* runtime_visit_while(runtime_T* runtime, AST_T* node)
{
    while(_boolean_evaluation(runtime_visit(runtime, node->while_expr)))
    {
        AST_T* visited = runtime_visit(runtime, node->while_body);

        if (visited->type == AST_BREAK)
            break;
        else
        if (visited->type == AST_CONTINUE)
            continue;
    }

    return node;
}

AST_T* runtime_visit_new(runtime_T* runtime, AST_T* node)
{
    return ast_copy(runtime_visit(runtime, node->new_value));
}

AST_T* runtime_visit_iterate(runtime_T* runtime, AST_T* node)
{
    hermes_scope_T* scope = get_scope(runtime, node);
    AST_T* ast_iterable = runtime_visit(runtime, node->iterate_iterable);

    AST_T* fdef = (void*)0;

    if (node->iterate_function->type == AST_FUNCTION_DEFINITION)
        fdef = node->iterate_function;

    // lookup for function definition
    if (fdef == (void*)0)
    {
        for (int i = 0; i < scope->function_definitions->size; i++)
        {
            fdef = scope->function_definitions->items[i];

            if (strcmp(fdef->function_name, node->iterate_function->variable_name) == 0)
            {
                if (fdef->fptr != (void*)0)
                {
                    /**
                     * This is currently not supported, but it would be nice
                     * to have this working.
                     *
                     * TODO: Make this possible?
                     */
                    printf("Error: [Line %d] Can not iterate with native method `%s`\n", fdef->line_n, fdef->function_name);
                    exit(1);
                }

                break;
            }
        }
    }
    
    hermes_scope_T* fdef_body_scope = (hermes_scope_T*) fdef->function_definition_body->scope;
    char* iterable_varname = ((AST_T*)fdef->function_definition_arguments->items[0])->variable_name;
    int x = 0;

    // Clear all existing arguments to prepare for the new definitions
    for (int z = fdef_body_scope->variable_definitions->size-1; z > 0; z--)
    {
        dynamic_list_remove(
            fdef_body_scope->variable_definitions,
            fdef_body_scope->variable_definitions->items[z],
            _ast_free
        );
    }

    if (ast_iterable->type == AST_STRING)
    {
        AST_T* new_variable_def = init_ast(AST_VARIABLE_DEFINITION);
        new_variable_def->variable_value = init_ast(AST_CHAR);
        new_variable_def->variable_value->char_value = ast_iterable->string_value[x];
        new_variable_def->variable_name = iterable_varname;
        
        dynamic_list_append(fdef_body_scope->variable_definitions, new_variable_def);

        for (;x < strlen(ast_iterable->string_value); x++)
        {
            new_variable_def->variable_value->char_value = ast_iterable->string_value[x];
            runtime_visit(runtime, fdef->function_definition_body);
        }
    }
    else
    if (ast_iterable->type == AST_LIST)
    {
        AST_T* new_variable_def = init_ast(AST_VARIABLE_DEFINITION);
        new_variable_def->variable_value = runtime_visit(runtime, (AST_T*)ast_iterable->list_children->items[x]);
        new_variable_def->variable_name = iterable_varname;
        
        dynamic_list_append(fdef_body_scope->variable_definitions, new_variable_def);

        for (;x < ast_iterable->list_children->size; x++)
        {
            new_variable_def->variable_value = runtime_visit(runtime, (AST_T*)ast_iterable->list_children->items[x]);
            runtime_visit(runtime, fdef->function_definition_body);
        }
    }

    return INITIALIZED_NOOP;
}

AST_T* runtime_visit_assert(runtime_T* runtime, AST_T* node)
{
    if (!_boolean_evaluation(runtime_visit(runtime, node->assert_expr)))
    {
        char* str = (void*)0;

        if (node->assert_expr->type == AST_BINOP)
        {
            const char* template = "ASSERT(%s, %s)";
            char* left = ast_to_string(node->assert_expr->binop_left);
            char* right = ast_to_string(node->assert_expr->binop_right);
            str = calloc(strlen(template) + strlen(left) + strlen(right) + 1, sizeof(char));
            sprintf(str, template, left, right);
        }
        else
        {
            const char* template = "ASSERT(%s)";
            char* value = ast_to_string(node->assert_expr);
            str = calloc(strlen(template) + strlen(value) + 1, sizeof(char));
            sprintf(str, template, value);
        }

        printf("[Line %d] Assertment failed! %s\n", node->line_n, str);
        free(str);
        exit(1);
    }

    return INITIALIZED_NOOP;
}

void runtime_expect_args(dynamic_list_T* in_args, int argc, int args[])
{
    if (in_args->size < argc)
    {
        printf("%d arguments was provided, but expected %d.\n", (int) in_args->size, argc);
        exit(1);
    }

    unsigned int errors = 0;

    for (int i = 0; i < argc; i++)
    {
        if (args[i] == AST_ANY)
            continue;

        AST_T* ast = (AST_T*) in_args->items[i];

        if (ast->type != args[i])
        {
            printf("Got argument of type %d but expected %d.\n", ast->type, args[i]);
            errors += 1;
        }
    }

    if (errors)
    {
        printf("Got unexpected arguments, terminating.\n");
        exit(1);
    }
}

void hermes_runtime_buffer_stdout(runtime_T* runtime, const char* buffer)
{
    if (runtime->stdout_buffer == (void*)0)
    {
        runtime->stdout_buffer = calloc(strlen(buffer) + 1, sizeof(char));
        strcpy(runtime->stdout_buffer, buffer);
    }
    else
    {
        runtime->stdout_buffer = realloc(
            runtime->stdout_buffer,
            (strlen(runtime->stdout_buffer) + strlen(buffer) + 1) * sizeof(char)
        );
        strcat(runtime->stdout_buffer, buffer);
    }
}
