%{
#include "scql.hh"
#include "scql-tab.hh"
#include <charconv>

yytokentype parse_int(const char* text, size_t leng, YYSTYPE* lval, YYLTYPE* lloc);
yytokentype parse_float(const char* text, size_t leng, YYSTYPE* lval, YYLTYPE* lloc);

#define YY_USER_ACTION \
    yylloc->first_line = yylloc->last_line; \
    yylloc->first_column = yylloc->last_column; \
    for(int i = 0; yytext[i] != '\0'; i++) { \
        if(yytext[i] == '\n') { \
            yylloc->last_line++; \
            yylloc->last_column = 0; \
        } \
        else { \
            yylloc->last_column++; \
        } \
    }

#define yyterminate() return scqlEND
%}

%option 8bit
%option outfile="scql-scan.cc" header-file="scql-scan.hh"
%option bison-locations
%option noyywrap
%option bison-bridge
%option prefix="scql"
%option nounput


ws                      ([ \t]|\xe2\x80\xaf)+

sign_opt                [+-]?
dseq                    [[:digit:]]+
dseq_opt                [[:digit:]]*
daseq                   [[:alnum:]]+
daseq_opt               [[:alnum:]]*
frac                    ({dseq_opt}"."{dseq})|({dseq}".")
exp                     [eE]{sign_opt}{dseq}
exp_opt                 {exp}?

int                     {dseq}
float                   ({frac}{exp_opt})|({dseq}{exp})

string                  "\""[^\"\n]*"\""
incompletestring        "\""[^\"\n]*

ident                   [[:alpha:]][[:alnum:]_]*

%%

{ws}                    /* skip white space */

{int}                   { return parse_int(yytext, yyleng, yylval, yylloc); }

{float}                 { return parse_float(yytext, yyleng, yylval, yylloc); }

"|"                     { *yylval = scql::syntax::alloc(*yylloc); return '|'; }

","                     { *yylval = scql::syntax::alloc(*yylloc); return ','; }

"["                     { *yylval = scql::syntax::alloc(*yylloc); return '['; }

"]"                     { *yylval = scql::syntax::alloc(*yylloc); return ']'; }

"("                     { *yylval = scql::syntax::alloc(*yylloc); return '('; }

")"                     { *yylval = scql::syntax::alloc(*yylloc); return ')'; }

"*"                     { *yylval = scql::syntax::alloc(*yylloc); return '*'; }

{ident}                 { *yylval = scql::ident::alloc(yytext, yyleng, *yylloc); return scqlIDENT; }

"$"{ident}?             { *yylval = scql::datacell::alloc(yytext + 1, yyleng - 1, *yylloc); return scqlATOM; }

"$@"{ident}?            { *yylval = scql::computecell::alloc(yytext + 2, yyleng - 2, *yylloc); return scqlATOM; }

"@"{ident}?             { *yylval = scql::codecell::alloc(yytext + 1, yyleng - 1, *yylloc); return scqlCODECELL; }

{string}                { *yylval = scql::string::alloc(yytext, yyleng, *yylloc); return scqlATOM; }

{incompletestring}      { auto s = scql::string::alloc(yytext, yyleng, *yylloc); s->missing_close = true; *yylval = std::move(s); return scqlATOM; }


%%

yytokentype parse_int(const char* text, size_t leng, YYSTYPE* lval, YYLTYPE* lloc)
{
  intmax_t i;
  std::from_chars(text, text + leng, i);
  *lval = scql::integer::alloc(i, *lloc);
  return scqlATOM;
}


yytokentype parse_float(const char* text, size_t leng, YYSTYPE* lval, YYLTYPE* lloc)
{
  double f;
  std::from_chars(text, text + leng, f);
  *lval = scql::floatnum::alloc(f, *lloc);
  return scqlATOM;
}
