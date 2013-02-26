#include "tree.h"


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


void copy_node(node_t* to_n, node_t* from_n){
    to_n->type = from_n->type;
    to_n->data = from_n->data;
    to_n->entry = from_n->entry;
    to_n->n_children = from_n->n_children;
    to_n->children = from_n->children;
}

void remove_node(node_t* node){
    node_t *child = node->children[0];
    //Copy the child into the parent.
    copy_node(node, child);
    //Finalizing the child node now would be a huge mistake
    //as it would free memory areas now pointed to by node
    free(child);

}

node_t* simplify_tree ( node_t* node ){
    if ( node != NULL ){
        //Let's store this for easier access.
        uint32_t n_children = node->n_children;

        // Recursively simplify the children of the current node
        for ( uint32_t i=0; i<n_children; i++ ){
            node->children[i] = simplify_tree ( node->children[i] );
        }

        // After the children have been simplified, we look at the current node
        // What we do depend upon the type of node
        switch ( node->type.index )
        {
            // These are lists which needs to be flattened. Their structure
            // is the same, so they can be treated the same way.
            case FUNCTION_LIST: case STATEMENT_LIST: case PRINT_LIST:
            case EXPRESSION_LIST: case VARIABLE_LIST:
                if(n_children > 1){
                    //We know that child nr. 1 is the only possible list due to the definition of the language
                    node_t* child = node->children[0];
                     uint32_t grandchildren = node->children[0]->n_children;
                    if(child->type.index == node->type.index){ //Only flatten if the first child is also a list.

                        uint32_t old_n_children = n_children;
                        node->n_children +=  grandchildren - 1;
                        n_children = node->n_children;

                        node->children = realloc(node->children,n_children * sizeof(node_t*));

                        //Let's keep the rightmost child to the right
                        //if(n_children > 1)
                        node->children[n_children-1] = node->children[old_n_children-1];

                        //Now to fill all the left children with our grandchildren
                        //We know that the child has been flattened.
                        for(uint32_t i = 0; i < grandchildren;i++){
                            node->children[i] = child->children[i];
                        }

                        node_finalize(child);
                    }

                }
                break;


            // Declaration lists should also be flattened, but their stucture is sligthly
            // different, so they need their own case
            case DECLARATION_LIST:
                if(n_children > 0) {
                    //We know that child nr. 1 is the only possible list due to the definition of the language
                    node_t* child = node->children[0];
                    if(!child || child->type.index == node->type.index){ //Only flatten if the first child is also a list.

                        uint32_t old_n_children = n_children;
                        uint32_t grandchildren = 0;
                        if(child){ //If the child is NULL, then we let grandchildren be 0
                            grandchildren = child->n_children;
                        }
                        node->n_children += grandchildren - 1;
                        n_children = node->n_children;
                        node->children = realloc(node->children,n_children * sizeof(node_t*));

                        //Let's keep the rightmost child to the right
                        //if(n_children > 1)
                        node->children[n_children-1] = node->children[old_n_children-1];

                        //Now to fill all the left children with our grandchildren
                        //We know that the child has been flattened.
                        for(uint32_t i = 0; i < grandchildren;i++){
                            node->children[i] = child->children[i];
                        }

                        //node_finalize(child);
                    }
                }

                break;

            
            // These have only one child, so they are not needed
            case STATEMENT: case PARAMETER_LIST: case ARGUMENT_LIST:
                remove_node(node);
                break;


            // Expressions where both children are integers can be evaluated (and replaced with
            // integer nodes). Expressions whith just one child can be removed (like statements etc above)
            case EXPRESSION:
                //Expressions with a single child are worthless!
                if(node->n_children == 1){
                    //Fix unary minus
                    if(node->data &&
                            strncmp((char*)node->data, "-",100)==0){
                        if(node->children[0]->type.index == INTEGER){//Put in the negative constant instead. 
                        node->type = integer_n;
                        *((int*)node->data) =  -*((int*)node->children[0]->data);
                        node->n_children = 0;
                        node_finalize(node->children[0]);
                        }else{
                            //Keep the unary minus if the child is not a constant.
                        }
                    }else{//If this is not a unary minus, just remove the node.
                        remove_node(node);
                    }
                }
                
                //Compile time computations. (There has to be a better way to do this?! 
                //Oh wait, this is C. I could use a function for the meat of the else if, 
                //but I think it would net about the same amount of code, since I'd have to else if on the op anyway.)
                if(node->n_children > 1 &&
                        node->children[0]->type.index == INTEGER &&
                        node->children[1]->type.index == INTEGER){
                    if(strncmp((char*)node->data, "+",100)==0){
                        node->type = integer_n;
                        *((int*)node->data) = *((int*)node->children[0]->data) +
                                        *((int*)node->children[1]->data);
                        node->n_children = 0;
                        node_finalize(node->children[0]);
                        node_finalize(node->children[1]);
                    }else if(strncmp((char*)node->data, "-",100)==0){
                        node->type = integer_n;
                        *((int*)node->data) = *((int*)node->children[0]->data) -
                                        *((int*)node->children[1]->data);
                        node->n_children = 0;
                        node_finalize(node->children[0]);
                        node_finalize(node->children[1]);
                    }else if(strncmp((char*)node->data, "*",100)==0){
                        node->type = integer_n;
                        *((int*)node->data) = *((int*)node->children[0]->data) *
                                        *((int*)node->children[1]->data);
                        node->n_children = 0;
                        node_finalize(node->children[0]);
                        node_finalize(node->children[1]);
                    }else if(strncmp((char*)node->data, "/",100)==0){
                        node->type = integer_n;
                        *((int*)node->data) = *((int*)node->children[0]->data) /
                                        *((int*)node->children[1]->data);
                        node->n_children = 0;
                        node_finalize(node->children[0]);
                        node_finalize(node->children[1]);
                    }else if(strncmp((char*)node->data, "<",100)==0){
                        node->type = integer_n;
                        *((int*)node->data) = *((int*)node->children[0]->data) <
                                        *((int*)node->children[1]->data);
                        node->n_children = 0;
                        node_finalize(node->children[0]);
                        node_finalize(node->children[1]);
                    }else if(strncmp((char*)node->data, ">",100)==0){
                        node->type = integer_n;
                        *((int*)node->data) = *((int*)node->children[0]->data) >
                                        *((int*)node->children[1]->data);
                        node->n_children = 0;
                        node_finalize(node->children[0]);
                        node_finalize(node->children[1]);
                    }else if(strncmp((char*)node->data, "==",100)==0){
                        node->type = integer_n;
                        *((int*)node->data) = *((int*)node->children[0]->data) ==
                                        *((int*)node->children[1]->data);
                        node->n_children = 0;
                        node_finalize(node->children[0]);
                        node_finalize(node->children[1]);
                    }else if(strncmp((char*)node->data, "!=",100)==0){
                        node->type = integer_n;
                        *((int*)node->data) = *((int*)node->children[0]->data) !=
                                        *((int*)node->children[1]->data);
                        node->n_children = 0;
                        node_finalize(node->children[0]);
                        node_finalize(node->children[1]);
                    }else if(strncmp((char*)node->data, "<=",100)==0){
                        node->type = integer_n;
                        *((int*)node->data) = *((int*)node->children[0]->data) <=
                                        *((int*)node->children[1]->data);
                        node->n_children = 0;
                        node_finalize(node->children[0]);
                        node_finalize(node->children[1]);
                    }else if(strncmp((char*)node->data, ">=",100)==0){
                        node->type = integer_n;
                        *((int*)node->data) = *((int*)node->children[0]->data) >=
                                        *((int*)node->children[1]->data);
                        node->n_children = 0;
                        node_finalize(node->children[0]);
                        node_finalize(node->children[1]);
                    }
                }
                break;
        }

    }
    return node;
}


