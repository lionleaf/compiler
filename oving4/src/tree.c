#include "tree.h"
#include "symtab.h"


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

//recursivly walk a tree looking for VARIABLE nodes
int32_t
declare_tree(node_t *node, int32_t last_stack_offset){
    int32_t stack_offset = last_stack_offset;
    if(node->type.index == VARIABLE){
        stack_offset -= 4;
        declare(node,stack_offset);   
    }
    for(int i = 0; i < node->n_children; i++){
        node_t* child = node->children[i];
        stack_offset = declare_tree(child, stack_offset); 
    }
    return stack_offset;
}

void
declare(node_t* node, int32_t offset){

    symbol_t symbol; 
    symbol.label = (char*)node->data;
    symbol.depth = strlen(symbol.label);
    symbol.stack_offset = offset;

    symbol_insert(symbol.label, &symbol);
}


    void
bind_names ( node_t *root )
{

    if(root == NULL) return;
    if(root->type.index == DECLARATION){
        declare_tree(root, 0);
    }else if(root->type.index == BLOCK){
        scope_add();
    }else if(root->type.index == FUNCTION){
        //The first child of a FUNCTION is always a variable.
        declare(root->children[0],0);
        node_t* parlist = root->children[1];
        if(parlist != NULL){
            int32_t offset = 4 + parlist->n_children*4;
            for(int i = 0; i < parlist->n_children; i++){
                declare(parlist->children[i], offset);
                offset -= 4;
            }
        }
    }

    for(int i = 0; i < root->n_children; i++){
        node_t* child = root->children[i];
        bind_names(child); 
    }

    if(root->type.index == BLOCK){
        scope_remove();
    }
}
