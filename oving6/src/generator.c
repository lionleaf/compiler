#include <tree.h>
#include <generator.h>

bool peephole = false;


/* Elements of the low-level intermediate representation */

/* Instructions */
typedef enum {
    STRING, LABEL, PUSH, POP, MOVE, CALL, SYSCALL, LEAVE, RET,
    ADD, SUB, MUL, DIV, JUMP, JUMPZERO, JUMPNONZ, DECL, CLTD, NEG, CMPZERO, NIL,
    CMP, SETL, SETG, SETLE, SETGE, SETE, SETNE, CBW, CWDE,JUMPEQ
} opcode_t;

/* Registers */
static char
*eax = "%eax", *ebx = "%ebx", *ecx = "%ecx", *edx = "%edx",
    *ebp = "%ebp", *esp = "%esp", *esi = "%esi", *edi = "%edi",
    *al = "%al", *bl = "%bl";

/* A struct to make linked lists from instructions */
typedef struct instr {
    opcode_t opcode;
    char *operands[2];
    int32_t offsets[2];
    struct instr *next;
} instruction_t;

/* Start and last element for emitting/appending instructions */
static instruction_t *start = NULL, *last = NULL;

/*
 * Track the scope depth when traversing the tree - init. value may depend on
 * how the symtab was built
 */ 
static int32_t depth = 1;

//Used to make sure labels used in conditionals are unique.
//Should be incremented after each use
static int32_t label_id = 0;

/* Prototypes for auxiliaries (implemented at the end of this file) */
static void instruction_add ( opcode_t op, char *arg1, char *arg2, int32_t off1, int32_t off2 );
static void instructions_print ( FILE *stream );
static void instructions_finalize ( void );


/*
 * Convenience macro to continue the journey through the tree - just to save
 * on duplicate code, not really necessary
 */
#define RECUR() do {\
    for ( int32_t i=0; i<root->n_children; i++ )\
    generate ( stream, root->children[i] );\
} while(false)

/*
 * These macros set implement a function to start/stop the program, with
 * the only purpose of making the call on the first defined function appear
 * exactly as all other function calls.
 */
#define TEXT_HEAD() do {\
    instruction_add ( STRING,       STRDUP("main:"), NULL, 0, 0 );      \
    instruction_add ( PUSH,         ebp, NULL, 0, 0 );                  \
    instruction_add ( MOVE,         esp, ebp, 0, 0 );                   \
    instruction_add ( MOVE,         esp, esi, 8, 0 );                   \
    instruction_add ( DECL,         esi, NULL, 0, 0 );                  \
    instruction_add ( JUMPZERO,     STRDUP("noargs"), NULL, 0, 0 );     \
    instruction_add ( MOVE,         ebp, ebx, 12, 0 );                  \
    instruction_add ( STRING,       STRDUP("pusharg:"), NULL, 0, 0 );   \
    instruction_add ( ADD,          STRDUP("$4"), ebx, 0, 0 );          \
    instruction_add ( PUSH,         STRDUP("$10"), NULL, 0, 0 );        \
    instruction_add ( PUSH,         STRDUP("$0"), NULL, 0, 0 );         \
    instruction_add ( PUSH,         STRDUP("(%ebx)"), NULL, 0, 0 );     \
    instruction_add ( SYSCALL,      STRDUP("strtol"), NULL, 0, 0 );     \
    instruction_add ( ADD,          STRDUP("$12"), esp, 0, 0 );         \
    instruction_add ( PUSH,         eax, NULL, 0, 0 );                  \
    instruction_add ( DECL,         esi, NULL, 0, 0 );                  \
    instruction_add ( JUMPNONZ,     STRDUP("pusharg"), NULL, 0, 0 );    \
    instruction_add ( STRING,       STRDUP("noargs:"), NULL, 0, 0 );    \
} while ( false )

#define TEXT_TAIL() do {\
    instruction_add ( LEAVE, NULL, NULL, 0, 0 );            \
    instruction_add ( PUSH, eax, NULL, 0, 0 );              \
    instruction_add ( SYSCALL, STRDUP("exit"), NULL, 0, 0 );\
} while ( false )




void generate ( FILE *stream, node_t *root )
{
    char* funcname;
    int elegant_solution;
    if ( root == NULL )
        return;

    switch ( root->type.index )
    {   
        int32_t n_variables;
        int32_t string_nr; 
        node_t* temp_node;
        //Buffer used for representing numbers and the like
        //TODO: Check implications of the size of it.
        //20 is MAGIC for now!!
        char buffer[20];

        case PROGRAM:
            /* Output the data segment */
            strings_output ( stream );
            instruction_add ( STRING, STRDUP( ".text" ), NULL, 0, 0 );
            
            RECUR();
            TEXT_HEAD();
            

            //program->function_list->function->variable
            char* first_func = root->children[0]->children[0]->children[0]->entry->label;
            instruction_add(CALL, STRDUP(first_func), NULL,0,0);
            TEXT_TAIL();

            instructions_print ( stream );
            instructions_finalize ();
            break;

        case FUNCTION:
            //First child of a function is a variable with it's name.
            funcname =  root->children[0]->entry->label;
            //Add label.
            instruction_add(LABEL, STRDUP(funcname),NULL,0,0);
            //Execute the expression of the function
            //Push EBP
            instruction_add ( PUSH, ebp, NULL, 0, 0 );
            
            //Set EBP to top of stack.
            instruction_add ( MOVE, esp, ebp, 0, 0 );
            depth++;
            generate(stream, root->children[2]);
            depth--;


            //This will probably be dead code,
            //but it seems VSL should compile
            //functions without a return.
            //So let's return 0 at the end
            //of every function, even though there 
            //might be a return above it.
            instruction_add(MOVE, STRDUP("$0"), eax, 0, 0);
            instruction_add(LEAVE, NULL, NULL, 0, 0 );
            instruction_add(RET, NULL, NULL,0,0);
            break;

        case BLOCK:
            /*
             * Blocks:
             * Set up/take down activation record, no return value
             */
            //THIS SEGFAULTS TODO
            //Push EBP
            instruction_add ( PUSH, ebp, NULL, 0, 0 );
            
            //Set EBP to top of stack.
            instruction_add ( MOVE, esp, ebp, 0, 0 );
            depth++;
            //Fill in the block
            RECUR();
            depth--;
            //Restore EBP
            instruction_add ( LEAVE, NULL, NULL, 0, 0 );
            
            break;

        case DECLARATION:
            /*
             * Declarations:
             * Add space on local stack
             */
            n_variables = root->children[0]->n_children;
            //This buffer limits the number variables in a scope
            //TODO: Use logarithm to determine required size dynamically.
            sprintf(buffer,"$%d",n_variables*4);
            instruction_add(SUB, STRDUP(buffer),esp, 0,0);
            break;

        case PRINT_LIST:
            
            //Recursively print the list
            RECUR();

            //print a newline
            //10 is ascii code for newline
            instruction_add(PUSH, STRDUP("$10"),NULL,0,0); 
            instruction_add(SYSCALL, STRDUP("putchar"),NULL,0,0);
            break;

        case PRINT_ITEM:
            switch(root->children[0]->type.index){
                case TEXT:
                    string_nr =*((int32_t*)root->children[0]->data);
                    //push string on stack
                    sprintf(buffer,"$.STRING%d",string_nr);
                    instruction_add(PUSH, STRDUP(buffer),NULL, 0, 0 );
                    break;
                case EXPRESSION:
                case VARIABLE:
                    generate(stream,root->children[0]);
                    instruction_add(PUSH, STRDUP("$.INTEGER"), NULL, 0, 0);
                    break;
                case INTEGER:
                    sprintf(buffer, "$%d",*((int32_t*)root->children[0]->data));
                    instruction_add(PUSH,STRDUP(buffer),NULL,0,0);
                    instruction_add(PUSH, STRDUP("$.INTEGER"), NULL, 0, 0);
                    break;
            }
            
            instruction_add(SYSCALL, STRDUP("printf"), NULL, 0, 0);
            break;

        case EXPRESSION:
            switch(root->n_children){
                case 1:
                    if(*((char*)root->data) == '-'){
                        RECUR();
                        instruction_add(POP,ebx,NULL,0,0);
                        instruction_add(MOVE,STRDUP("$0"),eax,0,0);
                        instruction_add(SUB,ebx,eax,0,0);
                        instruction_add(PUSH,eax,NULL,0,0);
                    }else{
                        RECUR();
                    }
                    break;
                case 2:
                    //Check whether it's a function call
                    if(strncmp((char*)root->data, "F", 2) == 0){

                        //Because VSL and my compiler never need
                        //to keep the state of registres between
                        //function calls, I will not store
                        //the registers on the stack.

                        //Push parameters
                        if(root->children[1]){ 
                            generate(stream, root->children[1]);
                        }
                        //Push return address and jump to called function
                        instruction_add(CALL, STRDUP((char*) root->children[0]->data), NULL,0,0);
                        if(root->children[1]){
                            //remove parameters, I'll modify esp instead of multiple pops
                            sprintf(buffer,"$%d",root->children[1]->n_children*4);
                            instruction_add(ADD, STRDUP(buffer), esp, 0, 0);
                        }

                        //push result to stack, as this is an expression
                        instruction_add(PUSH, eax, NULL, 0,0);
                    }else{
                        switch(*((char*)root->data)){
                            case '+':
                                RECUR();
                                instruction_add(POP,ebx,NULL,0,0);
                                instruction_add(POP,eax,NULL,0,0);
                                instruction_add(ADD,ebx,eax,0,0);
                                instruction_add(PUSH,eax,NULL,0,0);
                                break;
                            case '-':
                                RECUR();
                                instruction_add(POP,ebx,NULL,0,0);
                                instruction_add(POP,eax,NULL,0,0);
                                instruction_add(SUB,ebx,eax,0,0);
                                instruction_add(PUSH,eax,NULL,0,0);
                                break;
                            case '*':
                                RECUR();
                                instruction_add(POP,ebx,NULL,0,0);
                                instruction_add(POP,eax,NULL,0,0);
                                instruction_add(MUL,ebx,NULL,0,0);
                                instruction_add(PUSH,eax,NULL,0,0);
                                break;
                            case '/':
                                RECUR();
                                instruction_add(POP,ebx,NULL,0,0);
                                instruction_add(POP,eax,NULL,0,0);
                                instruction_add(STRING,STRDUP("\tcdq"),NULL,0,0);
                                instruction_add(DIV,ebx,NULL,0,0);
                                instruction_add(PUSH,eax,NULL,0,0);
                                break;
                            case '<':
                            case '>':
                            case '=':
                            case '!':
                                RECUR();
                                instruction_add(POP,ebx,NULL,0,0);
                                instruction_add(POP,eax,NULL,0,0);
                                instruction_add(CMP,ebx,eax,0,0);
                                
                                if(strncmp((char*)root->data,"<=",2) == 0){
                                    instruction_add(SETLE,al,NULL,0,0);
                                }else if(strncmp((char*)root->data,"<",2)==0){
                                    instruction_add(SETL,al,NULL,0,0);
                                }else if(strncmp((char*)root->data,">=",2)==0){
                                    instruction_add(SETGE,al,NULL,0,0);
                                }else if(strncmp((char*)root->data,">",2)==0){
                                    instruction_add(SETG,al,NULL,0,0);
                                }else if(strncmp((char*)root->data,"==",2)==0){
                                    instruction_add(SETE,al,NULL,0,0);
                                }else if(strncmp((char*)root->data,"!=",2)==0){
                                    instruction_add(SETNE,al,NULL,0,0);
                                }


                                instruction_add(CBW,al,NULL,0,0);
                                instruction_add(CWDE,NULL,NULL,0,0);
                                instruction_add(PUSH,eax,NULL,0,0);
                                break;
                            default:
                                RECUR();
                        }

                    }
                    break;
            }
            break;

        case VARIABLE:
            /*
             * Occurrences of variables: (declarations have their own case)
             * - Find the variable's stack offset
             * - If var is not local, unwind the stack to its correct base
             */

            instruction_add(MOVE, ebp, ebx, 0, 0);
            for(int i = 0; i < depth - root->entry->depth; i++){
                instruction_add(STRING,  
                        STRDUP("\tmovl\t(\%ebx), \%ebx"), NULL, 0, 0);
            }
            instruction_add(PUSH, ebx, NULL, root->entry->stack_offset, 0);
            break;

        case INTEGER:
            /*
             * Integers: constants which can just be put on stack
             */
            sprintf(buffer, "$%d",*((int32_t*)root->data));
            instruction_add(PUSH,   STRDUP(buffer), NULL, 0, 0);
            break;

        case ASSIGNMENT_STATEMENT:
            //Start by a recursion to calculate the expression
            //Be careful not to generate the variable, as we don't want it on the stack
            generate(stream, root->children[1]);

            //Store result of right hand expression in eax
            instruction_add(POP, eax,NULL,0,0);
            
            //Move to the correct scope!
            instruction_add(MOVE, ebp, ebx, 0, 0);
            for(int i = 0; i < depth - root->children[0]->entry->depth; i++){
                instruction_add(STRING,  
                        STRDUP("\tmovl\t(\%ebx), \%ebx"), NULL, 0, 0);
            }

            //Store eax into the address of the variable on the LHS
            instruction_add(MOVE, eax, ebx, 0, root->children[0]->entry->stack_offset);
            break;

        case RETURN_STATEMENT:
            /*
             * Return statements:
             * Evaluate the expression and put it in EAX
             */
            //evaluate expression through recursion
            RECUR();

            //Result of expression is top of stack, so pop into eax
            instruction_add(POP, eax, NULL, 0, 0);
            //When we return from a function, we return from depth 1
            for(int i = depth; i > 1; i--){
                instruction_add(LEAVE, NULL, NULL, 0, 0 );
            }
            instruction_add(RET, NULL, NULL,0,0);
            break;
        case IF_STATEMENT:
            //save the current label_id and increment it.
            //Since elegant_solution is only in the function scope, we're safe
            elegant_solution = label_id;
            label_id++;

            //Generate the conditional.
            generate(stream, root->children[0]);
            
            instruction_add(POP, eax, NULL, 0,0);
            // will jump to label if %eax is 0
            instruction_add(STRING, STRDUP("\ttestl\t\%eax,\%eax"),NULL,0,0);
            //Jump past if body if conditional is zero.
            sprintf(buffer, "_end%d",elegant_solution); 
            instruction_add(JUMPZERO, STRDUP(buffer),NULL,0,0);
            //Assembly for the if body
            generate(stream, root->children[1]);

            if(root->n_children > 2){ // we have an else!
                //jump past the else
                sprintf(buffer, "_endelse%d",elegant_solution); 
                instruction_add(JUMP, STRDUP(buffer),NULL,0,0);
            }

            sprintf(buffer, "end%d",elegant_solution); 
            instruction_add(LABEL, STRDUP(buffer),NULL,0,0);

            if(root->n_children > 2){ // we have an else!
                generate(stream, root->children[2]);
                sprintf(buffer, "endelse%d",elegant_solution); 
                instruction_add(LABEL, STRDUP(buffer),NULL,0,0);
            }
            break;
        case WHILE_STATEMENT:
            elegant_solution = label_id;
            label_id++;
            
            sprintf(buffer, "start_while%d", elegant_solution);
            instruction_add(LABEL, STRDUP(buffer), NULL, 0,0);

            //Calculate condition
            generate(stream, root->children[0]);

            instruction_add(POP, eax, NULL, 0,0);
            // will jump to label if %eax is 0
            instruction_add(STRING, STRDUP("\ttestl\t\%eax,\%eax"),NULL,0,0);
            //Jump past if body if conditional is zero.
            sprintf(buffer, "_end_while%d",elegant_solution); 
            instruction_add(JUMPZERO, STRDUP(buffer),NULL,0,0);

            //loop body
            generate(stream, root->children[1]);

            sprintf(buffer, "_start_while%d",elegant_solution);
            instruction_add(JUMP, STRDUP(buffer), NULL, 0, 0);

            sprintf(buffer, "end_while%d",elegant_solution); 
            instruction_add(LABEL, STRDUP(buffer), NULL, 0, 0);
            break;
        case FOR_STATEMENT:
            elegant_solution = label_id;
            label_id++;
             //Execute assignment
            generate(stream, root->children[0]);

            //temp_node is now the variable that we iterate with
            temp_node = root->children[0]->children[0];
           
            sprintf(buffer, "start_for%d", elegant_solution);
            instruction_add(LABEL, STRDUP(buffer), NULL, 0,0);

            //evaluate end-condition expression
            generate(stream, root->children[1]);
            
            //push the variable
            generate(stream, temp_node);
            
            //compare
            instruction_add(POP,ebx,NULL,0,0);
            instruction_add(POP,eax,NULL,0,0);
            instruction_add(CMP,eax,ebx,0,0);
            sprintf(buffer,"\tjge\t_end%d",elegant_solution);
            instruction_add(STRING,STRDUP(buffer),NULL,0,0);

            //loop body
            generate(stream, root->children[2]);

            //increment counter
            instruction_add(MOVE, ebp, ebx, 0, 0);
            for(int i = 0; i < depth - temp_node->entry->depth; i++){
                instruction_add(STRING,  
                        STRDUP("\tmovl\t(\%ebx), \%ebx"), NULL, 0, 0);
            }
            instruction_add(ADD, STRDUP("$1"), ebx, 0, temp_node->entry->stack_offset);


            sprintf(buffer, "_start_for%d",elegant_solution);
            instruction_add(JUMP, STRDUP(buffer), NULL, 0, 0);

            sprintf(buffer, "end%d",elegant_solution); 
            instruction_add(LABEL, STRDUP(buffer), NULL, 0, 0);
            break;
        default:
            /* Everything else can just continue through the tree */
            RECUR();
            break;
    }
}


/* Provided auxiliaries... */


    static void
instruction_append ( instruction_t *next )
{
    if ( start != NULL )
    {
        last->next = next;
        last = next;
    }
    else
        start = last = next;
}


    static void
instruction_add (
        opcode_t op, char *arg1, char *arg2, int32_t off1, int32_t off2 
        )
{
    instruction_t *i = (instruction_t *) malloc ( sizeof(instruction_t) );
    *i = (instruction_t) { op, {arg1, arg2}, {off1, off2}, NULL };
    instruction_append ( i );
}


    static void
instructions_print ( FILE *stream )
{
    instruction_t *this = start;
    while ( this != NULL )
    {
        switch ( this->opcode )
        {
            case PUSH:
                if ( this->offsets[0] == 0 )
                    fprintf ( stream, "\tpushl\t%s\n", this->operands[0] );
                else
                    fprintf ( stream, "\tpushl\t%d(%s)\n",
                            this->offsets[0], this->operands[0]
                            );
                break;
            case POP:
                if ( this->offsets[0] == 0 )
                    fprintf ( stream, "\tpopl\t%s\n", this->operands[0] );
                else
                    fprintf ( stream, "\tpopl\t%d(%s)\n",
                            this->offsets[0], this->operands[0]
                            );
                break;
            case MOVE:
                if ( this->offsets[0] == 0 && this->offsets[1] == 0 )
                    fprintf ( stream, "\tmovl\t%s,%s\n",
                            this->operands[0], this->operands[1]
                            );
                else if ( this->offsets[0] != 0 && this->offsets[1] == 0 )
                    fprintf ( stream, "\tmovl\t%d(%s),%s\n",
                            this->offsets[0], this->operands[0], this->operands[1]
                            );
                else if ( this->offsets[0] == 0 && this->offsets[1] != 0 )
                    fprintf ( stream, "\tmovl\t%s,%d(%s)\n",
                            this->operands[0], this->offsets[1], this->operands[1]
                            );
                break;

            case ADD:
                if ( this->offsets[0] == 0 && this->offsets[1] == 0 )
                    fprintf ( stream, "\taddl\t%s,%s\n",
                            this->operands[0], this->operands[1]
                            );
                else if ( this->offsets[0] != 0 && this->offsets[1] == 0 )
                    fprintf ( stream, "\taddl\t%d(%s),%s\n",
                            this->offsets[0], this->operands[0], this->operands[1]
                            );
                else if ( this->offsets[0] == 0 && this->offsets[1] != 0 )
                    fprintf ( stream, "\taddl\t%s,%d(%s)\n",
                            this->operands[0], this->offsets[1], this->operands[1]
                            );
                break;
            case SUB:
                if ( this->offsets[0] == 0 && this->offsets[1] == 0 )
                    fprintf ( stream, "\tsubl\t%s,%s\n",
                            this->operands[0], this->operands[1]
                            );
                else if ( this->offsets[0] != 0 && this->offsets[1] == 0 )
                    fprintf ( stream, "\tsubl\t%d(%s),%s\n",
                            this->offsets[0], this->operands[0], this->operands[1]
                            );
                else if ( this->offsets[0] == 0 && this->offsets[1] != 0 )
                    fprintf ( stream, "\tsubl\t%s,%d(%s)\n",
                            this->operands[0], this->offsets[1], this->operands[1]
                            );
                break;
            case MUL:
                if ( this->offsets[0] == 0 )
                    fprintf ( stream, "\timull\t%s\n", this->operands[0] );
                else
                    fprintf ( stream, "\timull\t%d(%s)\n",
                            this->offsets[0], this->operands[0]
                            );
                break;
            case DIV:
                if ( this->offsets[0] == 0 )
                    fprintf ( stream, "\tidivl\t%s\n", this->operands[0] );
                else
                    fprintf ( stream, "\tidivl\t%d(%s)\n",
                            this->offsets[0], this->operands[0]
                            );
                break;
            case NEG:
                if ( this->offsets[0] == 0 )
                    fprintf ( stream, "\tnegl\t%s\n", this->operands[0] );
                else
                    fprintf ( stream, "\tnegl\t%d(%s)\n",
                            this->offsets[0], this->operands[0]
                            );
                break;

            case DECL:
                fprintf ( stream, "\tdecl\t%s\n", this->operands[0] );
                break;
            case CLTD:
                fprintf ( stream, "\tcltd\n" );
                break;
            case CBW:
                fprintf ( stream, "\tcbw\n" );
                break;
            case CWDE:
                fprintf ( stream, "\tcwde\n" );
                break;
            case CMPZERO:
                if ( this->offsets[0] == 0 )
                    fprintf ( stream, "\tcmpl\t$0,%s\n", this->operands[0] );
                else
                    fprintf ( stream, "\tcmpl\t$0,%d(%s)\n",
                            this->offsets[0], this->operands[0]
                            );
                break;
            case CMP:
                if ( this->offsets[0] == 0 && this->offsets[1] == 0 )
                    fprintf ( stream, "\tcmpl\t%s,%s\n",
                            this->operands[0], this->operands[1]
                            );
                else if ( this->offsets[0] != 0 && this->offsets[1] == 0 )
                    fprintf ( stream, "\tcmpl\t%d(%s),%s\n",
                            this->offsets[0], this->operands[0], this->operands[1]
                            );
                else if ( this->offsets[0] == 0 && this->offsets[1] != 0 )
                    fprintf ( stream, "\tcmpl\t%s,%d(%s)\n",
                            this->operands[0], this->offsets[1], this->operands[1]
                            );
                break;
            case SETL:
                if ( this->offsets[0] == 0 )
                    fprintf ( stream, "\tsetl\t%s\n", this->operands[0] );
                else
                    fprintf ( stream, "\tsetl\t%d(%s)\n",
                            this->offsets[0], this->operands[0]
                            );
                break;
            case SETG:
                if ( this->offsets[0] == 0 )
                    fprintf ( stream, "\tsetg\t%s\n", this->operands[0] );
                else
                    fprintf ( stream, "\tsetg\t%d(%s)\n",
                            this->offsets[0], this->operands[0]
                            );
                break;
            case SETLE:
                if ( this->offsets[0] == 0 )
                    fprintf ( stream, "\tsetle\t%s\n", this->operands[0] );
                else
                    fprintf ( stream, "\tsetle\t%d(%s)\n",
                            this->offsets[0], this->operands[0]
                            );
                break;
            case SETGE:
                if ( this->offsets[0] == 0 )
                    fprintf ( stream, "\tsetge\t%s\n", this->operands[0] );
                else
                    fprintf ( stream, "\tsetge\t%d(%s)\n",
                            this->offsets[0], this->operands[0]
                            );
                break;
            case SETE:
                if ( this->offsets[0] == 0 )
                    fprintf ( stream, "\tsete\t%s\n", this->operands[0] );
                else
                    fprintf ( stream, "\tsete\t%d(%s)\n",
                            this->offsets[0], this->operands[0]
                            );
                break;
            case SETNE:
                if ( this->offsets[0] == 0 )
                    fprintf ( stream, "\tsetne\t%s\n", this->operands[0] );
                else
                    fprintf ( stream, "\tsetne\t%d(%s)\n",
                            this->offsets[0], this->operands[0]
                            );
                break;

            case CALL: case SYSCALL:
                fprintf ( stream, "\tcall\t" );
                if ( this->opcode == CALL )
                    fputc ( '_', stream );
                fprintf ( stream, "%s\n", this->operands[0] );
                break;
            case LABEL: 
                fprintf ( stream, "_%s:\n", this->operands[0] );
                break;

            case JUMP:
                fprintf ( stream, "\tjmp\t%s\n", this->operands[0] );
                break;
            case JUMPZERO:
                fprintf ( stream, "\tjz\t%s\n", this->operands[0] );
                break;
            case JUMPEQ:
                fprintf ( stream, "\tje\t%s\n", this->operands[0] );
                break;
            case JUMPNONZ:
                fprintf ( stream, "\tjnz\t%s\n", this->operands[0] );
                break;

            case LEAVE: fputs ( "\tleave\n", stream ); break;
            case RET:   fputs ( "\tret\n", stream );   break;

            case STRING:
                        fprintf ( stream, "%s\n", this->operands[0] );
                        break;

            case NIL:
                        break;

            default:
                        fprintf ( stderr, "Error in instruction stream\n" );
                        break;
        }
        this = this->next;
    }
}


    static void
instructions_finalize ( void )
{
    instruction_t *this = start, *next;
    while ( this != NULL )
    {
        next = this->next;
        if ( this->operands[0] != eax && this->operands[0] != ebx &&
                this->operands[0] != ecx && this->operands[0] != edx &&
                this->operands[0] != ebp && this->operands[0] != esp &&
                this->operands[0] != esi && this->operands[0] != edi &&
                this->operands[0] != al && this->operands[0] != bl 
           )
            free ( this->operands[0] );
        free ( this );
        this = next;
    }
}
