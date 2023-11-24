%code requires {

#include "scql.hh"
}


%define api.pure full
%defines
%locations
%define parse.error verbose
%define parse.lac full
%define api.value.type {scql::part::cptr_type}
%define api.token.prefix {scql}


%code provides {
#define YY_DECL int scqllex(YYSTYPE* yylval_param, YYLTYPE* yylloc_param)
YY_DECL;
extern void yyerror(const YYLTYPE* l, const char* s);
}

%code {
#define yylex scqllex
#include "scql-scan.hh"
}


%token ATOM
%token IDENT
%token END


%%

start:            END {
                    scql::result = nullptr;
                  }
                | ATOM END {
                    scql::result = std::move($1);
                  }
                | pipeline END {
                    scql::result = std::move($1);
                  }
                ;

pipeline:         stage {
                    $$ = std::move($1);
                  }
                | stage '|' pipeline {
                    if ($3->is(scql::id_type::pipeline)) {
                      scql::as<scql::pipeline>($3)->prepend(std::move($1));
                      $$ = std::move($3);
                    } else
                      $$ = scql::pipeline::alloc(std::move($1), std::move($3), yylloc);
                  }
                | stage '|' error {
                    $$ = scql::pipeline::alloc(std::move($1), nullptr  , yylloc);
                  }
                ;

stage:            IDENT {
                     $$ = std::move($1);
                  }
                | IDENT '[' arglist_opt ']' {
                    auto lloc = yylloc;
                    lloc.first_line = $1->lloc.first_line;
                    lloc.first_column = $1->lloc.first_column;
                    if ($3 != nullptr) {
                      $3->lloc.first_line = $1->lloc.last_line;
                      $3->lloc.first_column = $1->lloc.last_column;
                    }
                    $$ = scql::fcall::alloc(std::move($1), std::move($3), lloc);
                  }
                | IDENT '[' arglist error {
                    auto n = scql::fcall::alloc(std::move($1), std::move($3), yylloc);
                    n->missing_close = true;
                    $$ = std::move(n);
                  }
                | IDENT '[' error {
                    auto n = scql::fcall::alloc(std::move($1), nullptr, yylloc);
                    n->missing_close = true;
                    $$ = std::move(n);
                  }
                | '(' pipeline_list ')' {
                    $$ = std::move($1);
                  }
                ;

pipeline_list:    %empty {
                    $$ = nullptr;
                  }
                | pipeline {
                    $$ = scql::list::alloc(std::move($1), yylloc);
                  }
                | pipeline ',' pipeline_list {
                    scql::as<scql::list>($3)->prepend(std::move($1));
                    $$ = std::move($3);
                  }
                ;

arglist_opt:      %empty {
                    $$ = nullptr;
                  }
                | arglist {
                    $$ = std::move($1);
                  }
                ;

arglist:          arg {
                    $$ = scql::list::alloc(std::move($1), yylloc);
                  }
                | arg ',' arglist {
                    scql::as<scql::list>($3)->prepend(std::move($1));
                    $$ = std::move($3);
                  }
                ;

arg:              ATOM {
                   $$ = std::move($1);
                  }
                | IDENT {
                    $$ = std::move($1);
                  }
                ;


%%

#include <iostream>

void yyerror(const YYLTYPE*, const char*)
{
  // std::cout << "yyerror s=\"" << s << "\"\n";
}
