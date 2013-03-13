#include "tree.h"
#include "symtab.h"

//To avoid having to hand in the .h file, I'll declare my functions here
void declare(node_t* node, int32_t offset, char function);
void declare_functions(node_t* node);
void recurse_tree(node_t* node);
int32_t declare_tree(node_t* node);
void define_parameters(node_t* node);
void bind_variable(node_t* node);

int32_t next_stack_offset = -4;

#ifdef DUMP_TREES
    void
node_print ( FILE *output, node_t *root, uint32_t nesting )
{
    if ( root != NULL )
    {
        fprintf ( output, "%*c%s", nesting, ' ', root->type.text );
        if ( root->type.index == INTEGER )
            fprintf ( output, "(%d)", *((int32_t *)root->data) );
        if ( root->type.index == VARIABLE || root->type.index == EXPRESSION )
        {
            if ( root->data != NULL )
                fprintf ( output, "(\"%s\")", (char *)root->data );
            else
                fprintf ( output, "%p", root->data );
        }
        fputc ( '\n', output );
        for ( int32_t i=0; i<root->n_children; i++ )
            node_print ( output, root->children[i], nesting+1 );
    }
    else
        fprintf ( output, "%*c%p\n", nesting, ' ', root );
}
#endif


    node_t *
node_init ( node_t *nd, nodetype_t type, void *data, uint32_t n_children, ... )
{
    va_list child_list;
    *nd = (node_t) { type, data, NULL, n_children,
        (node_t **) malloc ( n_children * sizeof(node_t *) )
    };
    va_start ( child_list, n_children );
    for ( uint32_t i=0; i<n_children; i++ )
        nd->children[i] = va_arg ( child_list, node_t * );
    va_end ( child_list );
    return nd;
}


    void
node_finalize ( node_t *discard )
{
    if ( discard != NULL )
    {
        free ( discard->data );
        free ( discard->children );
        free ( discard );
    }
}


    void
destroy_subtree ( node_t *discard )
{
    if ( discard != NULL )
    {
        for ( uint32_t i=0; i<discard->n_children; i++ )
            destroy_subtree ( discard->children[i] );
        node_finalize ( discard );
    }
}

void
bind_names ( node_t *root )
{
    //Initial scope for the functions
    scope_add();

    declare_functions(root);
    recurse_tree(root);
    
    //Remove the function scope
    scope_remove();
}

void
declare_functions(node_t* node){
    //The only possible child.
    node_t* functionlist = node->children[0];
    for(int i = 0;i < functionlist->n_children; i++){
        node_t* function = functionlist->children[i];
        declare(function->children[0],0,TRUE);
    }
}

void 
recurse_tree(node_t* root){
if(root == NULL) return;
    if(root->type.index == DECLARATION){
        declare_tree(root);
    }else if(root->type.index == BLOCK){
        next_stack_offset = -4; //TODO:Do something cooler?
        scope_add();
    }else if(root->type.index == FUNCTION){
        next_stack_offset = -4;
        scope_add();
        //function should've been defined already, but not the parameters.
        define_parameters(root);
    }else if(root->type.index == EXPRESSION
          || root->type.index == ASSIGNMENT_STATEMENT){
        for(int i = 0; i< root->n_children;i++){
            if(root->children[i] == NULL) continue;
            if(root->children[i]->type.index == VARIABLE){
                symbol_t* symbol = symbol_get((char*)root->data);
                root->entry = symbol;
            }
        }
    }else if(root->type.index == TEXT){
        char* text = (char*) root->data;

        //For future ease of use, I will not store 
        //the index directly in the pointer.
        int32_t* intpoint = malloc(sizeof(int32_t));
        *intpoint = strings_add(text); 
        //Store the string index instead of the string
        root->data = intpoint;
    }

    for(int i = 0; i < root->n_children; i++){
        node_t* child = root->children[i];
        recurse_tree(child); 
    }

    if(root->type.index == BLOCK
    || root->type.index == FUNCTION){
        scope_remove();
    }
}

//Recursivly walk a tree looking for VARIABLE nodes
int32_t
declare_tree(node_t *node){
    if(node->type.index == VARIABLE){
        declare(node,next_stack_offset, FALSE);   
        next_stack_offset -= 4;
    }
    for(int i = 0; i < node->n_children; i++){
        node_t* child = node->children[i];
        declare_tree(child); 
    }
}

void
declare(node_t* node, int32_t offset, char function){
    symbol_t* symbol = malloc(sizeof(symbol_t));
    char* key = (char*)node->data;

    //"Variables don't need labels"
    if(function){
        symbol->label = key;
    }
    
    symbol->stack_offset = offset;
    symbol_insert(key, symbol);
}

void
define_parameters(node_t* node){
    node_t* parlist = node->children[1];
    if(parlist != NULL){
        int32_t offset = 4 + parlist->n_children*4;
        for(int i = 0; i < parlist->n_children; i++){
            declare(parlist->children[i], offset, FALSE);
            offset -= 4;
        }
    }

}

