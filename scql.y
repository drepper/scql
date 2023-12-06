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

// XYZ Debugging
//#include <iostream>
}


%token ATOM
%token CODECELL
%token IDENT
%token END


%%

start:            pipeline END {
                    scql::result = std::move($1);
                  }
                ;

pipeline:         pipeline_list {
                    $$ = std::move($1);
                  }
                | pipeline_list '|' pipeline {
                    if ($3 && $3->is(scql::id_type::pipeline)) {
                      scql::as<scql::pipeline>($3)->prepend(std::move($1));
                      $3->lloc.first_line = $1->lloc.first_line;
                      $3->lloc.first_column = $1->lloc.first_column;
                      $$ = std::move($3);
                    } else {
                      auto lloc = $1->lloc;
                      if ($3) {
                        lloc.last_line = $3->lloc.last_line;
                        lloc.last_column = $3->lloc.last_column;
                      }
                      $$ = scql::pipeline::alloc(std::move($1), std::move($3), lloc);
                    }
                  }
                | pipeline_list '|' error {
                    $$ = scql::pipeline::alloc(std::move($1), nullptr, $1 ? $1->lloc : yylloc);
                  }
                ;

pipeline_list:    stage {
                    $$ = std::move($1);
                  }
                | stage ';' pipeline_list {
                    if ($3 && $3->is(scql::id_type::list)) {
                      scql::as<scql::list>($3)->prepend(std::move($1));
                      $3->lloc.first_line = $1->lloc.first_line;
                      $3->lloc.first_column = $1->lloc.first_column;
                      $$ = std::move($3);
                    } else {
                      auto lloc = $1 ? $1->lloc : yylloc;
                      if ($3) {
                        lloc.last_line = $3->lloc.last_line;
                        lloc.last_column = $3->lloc.last_column;
                      }
                      $$ = scql::list::alloc(std::move($1), std::move($3), lloc);
                    }
                  }
                ;

stage:            %empty {
                    $$ = nullptr;
                  }
                | ATOM {
                    $$ = std::move($1);
                  }
                | IDENT {
                    $$ = std::move($1);
                  }
                | CODECELL {
                    scql::as<scql::codecell>($1)->missing_brackets = true;
                    $$ = std::move($1);
                  }
                | fname '[' arglist ']' {
                    auto lloc = yylloc;
                    lloc.first_line = $1->lloc.first_line;
                    lloc.first_column = $1->lloc.first_column;
                    if ($3 != nullptr) {
                      $3->lloc.first_line = $1->lloc.last_line;
                      $3->lloc.first_column = $1->lloc.last_column;
                    }
                    $$ = scql::fcall::alloc(std::move($1), std::move($3), lloc);
                  }
                | fname '[' error {
                    auto n = scql::fcall::alloc(std::move($1), nullptr, yylloc);
                    n->missing_close = true;
                    $$ = std::move(n);
                  }
                | '(' pipeline ')' {
                    $$ = std::move($2);
                  }
                ;

fname:            CODECELL {
                    $$ = std::move($1);
                  }
                | IDENT {
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
                | error ',' arglist {
                    scql::as<scql::list>($3)->prepend(nullptr);
                    $$ = std::move($3);
                  }
                | arg ',' error {
                    auto r = scql::list::alloc(std::move($1), yylloc);
                    r->add(nullptr);
                    $$ = std::move(r);
                  }
                ;

arg:              ATOM {
                   $$ = std::move($1);
                  }
                | IDENT {
                    $$ = std::move($1);
                  }
                | '*' {
                    $$ = scql::glob::alloc(yyloc);
                  }
                | %empty {
                    $$ = nullptr;
                  }
                ;


%%

#include <iostream>

void yyerror(const YYLTYPE*, const char*)
{
  // std::cout << "yyerror s=\"" << s << "\"\n";
}
