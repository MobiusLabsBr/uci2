%{
	#include <stdio.h>

	#include "utils/memory.h"

	#include "parser.h"
	char *uci_unquote(char *string, int string_size);
%}

%option nounput noinput noyywrap reentrant bison-bridge
%option outfile="lexer.c" header-file="lexer.h"

newline              \n
/* whitespace as defined by isspace(), like in libuci, so CRLF line endings work */
ws                   [ \t\r\f\v]
comment              "#".*
line_continuation    \\\r?\n
/* a value is a concatenation of quoted and unquoted parts, like in libuci: 'it'\''s'
 * - '#' starts a comment and ';' ends the statement, unless quoted or escaped
 * - a backslash at the end of a line continues the value on the next line */
squoted              '[^']*'
dquoted              \"([^\"\\]|{line_continuation}|\\(.|\n))*\"
unquoted             ([^ \t\r\f\v\n'\"\\#;]|{line_continuation}|\\.)+
value                ({squoted}|{dquoted}|{unquoted})+

/* keywords are only recognized at the start of a statement and must be followed by a space or tab,
 * libuci also accepts their first letter */
config               ("config"|"c")
option               ("option"|"o")
package              ("package"|"p")
list                 ("list"|"l")

/* after a keyword, everything up to the end of the statement is a value */
%x ST_VALUE


%%
<INITIAL,ST_VALUE>{newline}     { BEGIN(INITIAL); }
<ST_VALUE>";"                   { BEGIN(INITIAL); }
<INITIAL,ST_VALUE>{comment}     ;
<INITIAL,ST_VALUE>{ws}+         ;
{option}/[ \t]                  { BEGIN(ST_VALUE); return OPTION; }
{list}/[ \t]                    { BEGIN(ST_VALUE); return LIST; }
{config}/[ \t]                  { BEGIN(ST_VALUE); return CONFIG; }
{package}/[ \t]                 { BEGIN(ST_VALUE); return PACKAGE; }
<ST_VALUE>{value}               { yylval->string = uci_unquote(yytext, yyleng); return VALUE; }

<INITIAL,ST_VALUE>.             { return 1; }
%%

// how Flex handles ambiguous patterns (config and value)
// - match the longest possible string every time the scanner matches input
// - in the case of a tie, use the pattern that appears first in the program

// unquote a value the same way libuci does:
// - inside single quotes every character is taken literally
// - inside double quotes and unquoted parts a backslash escapes the next character,
//   a backslash at the end of a line joins the next line
char *uci_unquote(char *string, int string_size)
{
	char *result = NULL;
	size_t length = 0;
	char quote = '\0';

	if (string_size < 0) {
		return NULL;
	}

	result = xcalloc((size_t) string_size + 1, sizeof(char));

	for (int i = 0; i < string_size; i++) {
		if (quote == '\'') {
			if (string[i] == '\'') {
				quote = '\0';
			} else {
				result[length++] = string[i];
			}
		} else if (string[i] == '\\' && i + 1 < string_size && string[i + 1] == '\n') {
			// line continuation, the backslash and the line break are dropped
			i++;
		} else if (string[i] == '\\' && i + 2 < string_size && string[i + 1] == '\r' && string[i + 2] == '\n') {
			i += 2;
		} else if (string[i] == '\\' && i + 1 < string_size) {
			result[length++] = string[++i];
		} else if (quote == '"') {
			if (string[i] == '"') {
				quote = '\0';
			} else {
				result[length++] = string[i];
			}
		} else if (string[i] == '\'' || string[i] == '"') {
			quote = string[i];
		} else {
			result[length++] = string[i];
		}
	}

	result[length] = '\0';

	return result;
}

// yyerror
extern void yyerror(yyscan_t scanner, ast_t *ctx, const char *string)
{
	// print error condition
    fprintf(stderr, "yyerror: %s\n", string);
}
