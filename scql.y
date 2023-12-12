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


%start start

%%

start:            pipeline END {
                    scql::result = std::move($1);
                    YYACCEPT;
                  }
                ;

pipeline:         pipeline_list {
                    auto lloc = yylloc;
                    if ($1) {
                      lloc.first_line = std::min(lloc.first_line, $1->lloc.first_line);
                      lloc.first_column = std::min(lloc.first_column, $1->lloc.first_column);
                      lloc.last_line = std::min(lloc.last_line, $1->lloc.last_line);
                      lloc.last_column = std::min(lloc.last_column, $1->lloc.last_column);
                    }
                    $$ = scql::pipeline::alloc(std::move($1), lloc);
                  }
                | pipeline_list '|' pipeline {
                    if ($1) {
                      $3->lloc.first_line = $1->lloc.first_line;
                      $3->lloc.first_column = $1->lloc.first_column;
                    }
                    scql::as<scql::pipeline>($3)->prepend(std::move($1));
                    $$ = std::move($3);
                  }
                | pipeline_list '|' error {
                    $$ = scql::pipeline::alloc(std::move($1), nullptr, $1 ? $1->lloc : yylloc);
                  }
                ;

pipeline_list:    stage {
                    auto lloc = yylloc;
                    if ($1) {
                      lloc.first_line = std::min(lloc.first_line, $1->lloc.first_line);
                      lloc.first_column = std::min(lloc.first_column, $1->lloc.first_column);
                      lloc.last_line = std::min(lloc.last_line, $1->lloc.last_line);
                      lloc.last_column = std::min(lloc.last_column, $1->lloc.last_column);
                    }
                    $$ = scql::statements::alloc(std::move($1), lloc);
                  }
                | stage ';' pipeline_list {
                    if ($1) {
                      $3->lloc.first_line = $1->lloc.first_line;
                      $3->lloc.first_column = $1->lloc.first_column;
                    }
                    scql::as<scql::statements>($3)->prepend(std::move($1));
                    $$ = std::move($3);
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
                | fname '[' ']' {
                    auto lloc = yylloc;
                    if ($1) {
                      lloc.first_line = $1->lloc.first_line;
                      lloc.first_column = $1->lloc.first_column;
                    }
                    $$ = scql::fcall::alloc(std::move($1), lloc);
                  }
                | fname '[' arglist ']' {
                    if ($1) {
                      $3->lloc.first_line = $1->lloc.first_line;
                      $3->lloc.first_column = $1->lloc.first_column;
                    }
                    scql::as<scql::fcall>($3)->set_fname(std::move($1));
                    $$ = std::move($3);
                  }
                | fname '[' error {
                    auto lloc = yylloc;
                    if ($1) {
                      lloc.first_line = $1->lloc.first_line;
                      lloc.first_column = $1->lloc.first_column;
                    }
                    auto n = scql::fcall::alloc(std::move($1), nullptr, lloc);
                    n->missing_close = true;
                    $$ = std::move(n);
                  }
                | '(' pipeline ')' {
                    $2->lloc.first_line = $1->lloc.first_line;
                    $2->lloc.first_column = $1->lloc.first_column;
                    $2->lloc.last_line = $3->lloc.last_line;
                    $2->lloc.last_column = $3->lloc.last_column;
                    $$ = std::move($2);
                  }
                ;

fname:            CODECELL {
                    $$ = std::move($1);
                  }
                | IDENT {
                    $$ = std::move($1);
                  }
                | ATOM {
                    $$ = nullptr;
                  }
                ;

arglist:          arg {
                    $$ = scql::fcall::alloc(nullptr, std::move($1), yylloc);
                  }
                | arg ',' arglist {
                    $3->lloc.first_line = $1->lloc.first_line;
                    $3->lloc.first_column = $1->lloc.first_column;
                    scql::as<scql::fcall>($3)->prepend(std::move($1));
                    $$ = std::move($3);
                  }
                | error ',' arglist {
                    $3->lloc.first_line = $2->lloc.first_line;
                    $3->lloc.first_column = $2->lloc.first_column;
                    scql::as<scql::fcall>($3)->prepend(nullptr);
                    $$ = std::move($3);
                  }
                | arg ',' error {
                    auto lloc = $1->lloc;
                    lloc.last_line = $2->lloc.last_line;
                    lloc.last_column = $2->lloc.last_column;
                    $$ = scql::fcall::alloc(nullptr, std::move($1), nullptr, lloc);
                  }
                | error ',' error {
                    $$ = scql::fcall::alloc(nullptr, nullptr, nullptr, $2->lloc);
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
                ;


%%

#include <iostream>

void yyerror(const YYLTYPE*, const char*)
{
  // std::cout << "yyerror s=\"" << s << "\"\n";
}
