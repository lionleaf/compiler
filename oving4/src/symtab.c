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
    //Dynamically allocate 3 tables with an initial size.
    
}


void
symtab_finalize ( void )
{
    //Remove both tables and their contents
}


//Add str to a list of strictly growing table of all strings encountered
//And returns the index of the newly added string.
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
    //dump all strings in the table for a given output stream.
    //Format:
    //.data    //fixed
    //.INTEGER: .string "%d "
    //.STRING0: .string "Hello, world!"
    //.STRING1: .string "Some other thing.."
    //.STRING<index>: .string <the text>
    //(etc.)
    //.globl main  //fixed
}


void
scope_add ( void )
{
    //add hash table to stack of tables representing nested scopes.
}


void
scope_remove ( void )
{
    //remove hash table from stack of tables representing nested scopes.
}


void
symbol_insert ( char *key, symbol_t *value )
{
    //insert symbol to current table

// Keep this for debugging/testing
#ifdef DUMP_SYMTAB
fprintf ( stderr, "Inserting (%s,%d)\n", key, value->stack_offset );
#endif

}


symbol_t *
symbol_get ( char *key )
{
    symbol_t* result = NULL;

    //resolve symbol entries in the stack
// Keep this for debugging/testing
#ifdef DUMP_SYMTAB
    if ( result != NULL )
        fprintf ( stderr, "Retrieving (%s,%d)\n", key, result->stack_offset );
#endif
}
