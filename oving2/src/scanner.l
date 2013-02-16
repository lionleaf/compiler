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

%%
.           { RETURN( yytext[0] ); }
%%
