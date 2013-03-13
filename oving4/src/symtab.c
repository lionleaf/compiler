#include "symtab.h"

// static does not mean the same as in Java.
// For global variables, it means they are only visible in this file.

// Pointer to stack of hash tables 
static hash_t **scopes;

// Pointer to array of values, to make it easier to free them
static symbol_t **values;

// Pointer to array of strings, should be able to dynamically expand as new strings
// are added.
static char **strings;

// Helper variables for manageing the stacks/arrays
static int32_t scopes_size = 16, scopes_index = -1;
static int32_t values_size = 16, values_index = -1;
static int32_t strings_size = 16, strings_index = -1;


void
symtab_init ( void )
{
    scopes  = malloc(sizeof(hash_t*) * scopes_size);
    values  = malloc(sizeof(symbol_t*) * values_size);
    strings = malloc(sizeof(char*) * strings_size);
}


void
symtab_finalize ( void )
{   
    //Lexer buffers are not destroyed elsewhere,
    //and it annoys me when valgrind reports the leak :P
    yylex_destroy();

    for(int i = 0; i <= values_index; i++){
        if(values[i]->label != NULL){
            free(values[i]->label);
        }
        free(values[i]);
    }

    for(int i = 0; i <= strings_index; i++){
        free(strings[i]);
    }

    free(scopes);
    free(values);
    free(strings);


    //SUCCESS, valgrind reports 0 leaks :D
}


int32_t
strings_add ( char *str )
{   
    ++strings_index;
    //Expand if necessary
    if(strings_index == strings_size){
        strings_size *= 2;//Double the size!
        strings = realloc(strings, sizeof(char*) * strings_size);
    }

    strings[strings_index] = str;
    return strings_index;
}


void
strings_output ( FILE *stream )
{
    fprintf(stream, ".data\n");
    fprintf(stream, ".INTEGER: .string \"\%%d \"\n");

    for(int i = 0; i <= strings_index;i++){
        fprintf(stream, ".STRING%d: .string %s\n",i, strings[i]);
    }

    fprintf(stream, ".globl main\n");
}


void
scope_add ( void )
{
    ++scopes_index;
    
    //Expand if it's full
    if(scopes_index == scopes_size){
        scopes_size *= 2;
        scopes = realloc(scopes, sizeof(hash_t*) * scopes_size);
    }
    
    scopes[scopes_index] = ght_create(8);
    
}


void
scope_remove ( void )
{
    ght_finalize(scopes[scopes_index]);
    --scopes_index;
}


void
symbol_insert ( char *key, symbol_t *value )
{
    //NOTE: I assume that the input is well-defined.
    //And that a crash here would simply indicate a compile error.
    //So there is no checks for whether value == NULL etc.

    //Depth of the functions will be set to 0 anyway,
    //so we can set it for every symbol!
    value->depth = scopes_index;

    hash_t* cur_table = scopes[scopes_index];
    int len = strlen(key);
    ght_insert(cur_table, value, len, key);


    //Time to add the symbol to the symbol array
    ++values_index;
    //Expand if necessary
    if(values_index == values_size){
        values_size *= 2;//Double the size!
        values = realloc(values, sizeof(symbol_t*) * values_size);
    }

    values[values_index] = value;
    
// Keep this for debugging/testing
#ifdef DUMP_SYMTAB
fprintf ( stderr, "Inserting (%s,%d)\n", key, value->stack_offset );
#endif

}


symbol_t *
symbol_get ( char *key )
{
    symbol_t* result = NULL;
    //Check all scopes. Start with the current one, and move "away".
    for(int32_t i = scopes_index; i >= 0; i--){
        hash_t* table_i = scopes[i];
        result = (symbol_t*) ght_get(table_i,strlen(key), key);
        if(result != NULL){
            break;
        }
    }
    
// Keep this for debugging/testing
#ifdef DUMP_SYMTAB
    if ( result != NULL )
        fprintf ( stderr, "Retrieving (%s,%d)\n", key, result->stack_offset);
#endif
    
    return result;
}
