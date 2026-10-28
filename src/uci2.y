%code top{
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>

    #include "parser.h"
    #include "lexer.h"

    // external functions
    extern int yylex(YYSTYPE *lvalp, yyscan_t scanner);
    extern void yyerror(yyscan_t scanner, ast_t *ast, const char *string);
}

%code requires {
    #include "utils/memory.h"

    #include "ast.h"

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
    typedef void *yyscan_t;
#endif
}

// bison options
%output  "parser.c"
%defines "parser.h"
%define api.pure full
%define parse.error verbose
%lex-param   { yyscan_t *scanner }
%parse-param { yyscan_t *scanner }
%parse-param { ast_t *ast }

// initial action when parser starts
%initial-action {
    ast_init(ast);
}

// token types
%union {
    char *string;
    ast_node_t *node;
}

// terminal symbols (tokens)
%token <string>    VALUE
%token          CONFIG OPTION LIST PACKAGE

// non terminal symbol types
%type <node>    config options option lines line root package

// free string memory for discarded symbols (xcalloc in lex)
%destructor { XFREE($$); } VALUE

// set root node
%start root


%%
// root node
root : lines {
                 $$ = ast_root_new(ast, NULL, $1);
                 if ($$ == NULL) {
                     yyerror(scanner, ast, "section redefined with a different type");
                     YYABORT;
                 }
             }
     | package lines {
                         $$ = ast_root_new(ast, $1, $2);
                         if ($$ == NULL) {
                             yyerror(scanner, ast, "section redefined with a different type");
                             YYABORT;
                         }
                     }
     | package {
                   $$ = ast_root_new(ast, $1, NULL);
               }
     | %empty {
                  // only comments and whitespace
                  $$ = ast_root_new(ast, NULL, NULL);
              }
     ;

package : PACKAGE VALUE {
                            $$ = ast_node_new(ast, ANT_PACKAGE, xstrdup(AST_NODE_PACKAGE_NAME), $2);
                        }
        ;

// lines, recursive
lines : line {
                 // Use node type ANT_SENTINEL because this node is a temporary node
                 // whose children are going to be added to the node type ANT_CONFIG in the next step.
                 $$ = ast_node_new(ast, ANT_SENTINEL, NULL, NULL);
                 ast_node_add($$, $1);
             }
      | lines line {
                       ast_node_add($1, $2);
                   }
      ;

// line
line : config {
                  $$ = $1;
              }
     ;

// config line, sections with an empty name are unnamed like in libuci
config : CONFIG VALUE {
                          $$ = ast_section_new(ast, $2, NULL, NULL);
                      }
       | CONFIG VALUE VALUE {
                                $$ = ast_section_new(ast, $2, $3, NULL);
                            }
       | CONFIG VALUE options {
                                  $$ = ast_section_new(ast, $2, NULL, $3);
                              }
       | CONFIG VALUE VALUE options {
                                        $$ = ast_section_new(ast, $2, $3, $4);
                                    }
       ;

// options, recursive
options : option {
                     // Use node type ANT_SENTINEL because this node is a temporary node
                     // whose children are going to be added to the node type ANT_SECTION_NAME in the next step.
                     $$ = ast_node_new(ast, ANT_SENTINEL, NULL, NULL);
                     ast_node_add($$, $1);
                 }
        | options option {
                             ast_node_add($1, $2);
                         }
        ;

// option or list, duplicates are resolved by ast_root_new() like libuci does
option : OPTION VALUE VALUE {
                                $$ = ast_node_new(ast, ANT_OPTION, $2, $3);
                            }
       | OPTION VALUE {
                          // an option without value removes the option
                          $$ = ast_node_new(ast, ANT_OPTION, $2, NULL);
                      }
       | LIST VALUE {
                        // a list without value adds an empty element
                        $$ = ast_node_new(ast, ANT_LIST, $2, NULL);
                        ast_node_add($$, ast_node_new(ast, ANT_LIST_ITEM, xstrdup(""), NULL));
                    }
       | LIST VALUE VALUE {
                              $$ = ast_node_new(ast, ANT_LIST, $2, NULL);
                              // add list value as new node
                              ast_node_add($$, ast_node_new(ast, ANT_LIST_ITEM, $3, NULL));
                          }
       ;

%%
