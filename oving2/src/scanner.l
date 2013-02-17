%{
#include "parser.h"
#ifdef DUMP_TOKENS
    #define RETURN(t) do {                                      \
        fprintf ( stderr, "TOKEN ( %d,\t'%s' )\n", t, yytext );    \
        return t;                                               \
    } while ( 0 )
#else
    #define RETURN(t) return t
#endif
%}

%option array
%option noyywrap
%option yylineno

IDENT [a-zA-Z_][a-zA-Z_0-9]*
STRING \"[^\n\"]*\"
DIGIT [0-9]
WHITESPACE [ \t\n]
COMMENT \/\/[^\n]*\n
%%
FUNC        { RETURN( FUNC ); }
PRINT       { RETURN( PRINT ); }
RETURN      { RETURN( RETURN ); }
CONTINUE    { RETURN( CONTINUE ); }
IF          { RETURN( IF ); }
THEN        { RETURN( THEN ); }
ELSE        { RETURN( ELSE ); }
FI          { RETURN( FI ); }
WHILE       { RETURN( WHILE ); }
DO          { RETURN( DO ); }
DONE        { RETURN( DONE ); }
FOR         { RETURN( FOR ); }
TO          { RETURN( TO ); }
VAR         { RETURN( VAR ); }

:=          { RETURN( ASSIGN ); }
\+          { RETURN( '+' ); }
\-          { RETURN( '-' ); }
\*          { RETURN( '*' ); }
\/          { RETURN( '/' ); }
\(          { RETURN( '(' ); }
\)          { RETURN( ')' ); }
\{          { RETURN( '{' ); }
\}          { RETURN( '}' ); }
,           { RETURN( ',' ); }

\<          { RETURN( '<' ); }
\>          { RETURN( '>' ); }
\<=         { RETURN( LEQUAL ); }
\>=         { RETURN( GEQUAL ); }
==          { RETURN( EQUAL ); }
!=          { RETURN( NEQUAL ); }

{STRING}    { RETURN( STRING ); }
{DIGIT}+    { RETURN( NUMBER ); }
{IDENT}     { RETURN( IDENTIFIER ); }

{COMMENT}    /*Ignore comments*/
{WHITESPACE}+  /*IGNORE WHITESPACE*/
.           { RETURN( yytext[0] ); }
%%

/*int main(int argc, char **argv)
    {
    ++argv, --argc;  /* skip over program name */
    /*if ( argc > 0 )
        yyin = fopen( argv[0], "r" );
    else
        yyin = stdin;
    yylex();
    }*/
