#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define _PRINT_MSG(prefix, ...) do {              \
    printf(prefix" %s:%d ", __FILE__, __LINE__);  \
    printf(__VA_ARGS__);                          \
    printf("\n");                                 \
} while(0);                                       \

#define DBG(...) _PRINT_MSG("[DBG]", __VA_ARGS__)
#define PANIC(...) do {                           \
    _PRINT_MSG("[PANIC]", __VA_ARGS__);           \
    exit(1);                                      \
} while (0);

typedef enum {
    TOK_EOF = 0,
    TOK_PLUS,
    TOK_MINUS,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_SEMICOLON,
    TOK_EQUALS,
    TOK_NUMBER,
    TOK_IDENT,
    TOK_STRING,
    TOK_LET,
    _TOK_COUNT,
} TokenType;

char *names[] = {
    [TOK_EOF] = "EOF",
    [TOK_PLUS] = "+",
    [TOK_MINUS] = "-",
    [TOK_LPAREN] = "(",
    [TOK_RPAREN] = ")",
    [TOK_LBRACE] = "{",
    [TOK_RBRACE] = "}",
    [TOK_LBRACKET] = "[",
    [TOK_RBRACKET] = "]",
    [TOK_SEMICOLON] = ";",
    [TOK_EQUALS] = "=",
    [TOK_NUMBER] = "NUMBER",
    [TOK_IDENT] = "IDENT",
    [TOK_LET] = "LET",
    [TOK_STRING] = "STRING",
};

static_assert(sizeof(names)/sizeof(*names) == _TOK_COUNT, "Not every token is named");

typedef union {
    int number_value;
    char *string_value;
} TokenValue;

typedef struct {
    TokenType type;
    TokenValue value;
} Token;

TokenType ident_to_token_type(char *ident) {
    if (!strcmp(ident, "let")) return TOK_LET;
    return TOK_IDENT;
}

void remove_chars(char *s, char c) {
    int i = 0;
    for (char *v = s; *v; v++) {
        if (*v != c) {
            s[i++] = *v;
        }
    }
    s[i] = '\0';
}

char take_char(FILE *f) {
    char ret = fgetc(f);
    if (ferror(f)) PANIC("Cannot read file: %m");

    return ret;
}

char peek_char(FILE *f) {
    char ret = take_char(f);
    ungetc(ret, f);
    return ret;
}

char *take_ident(FILE *f)
{
    char buf[256] = { 0 };
    size_t str_len = 0;

    char c;
    while(isalnum(c = peek_char(f)) || c == '_') {
        take_char(f);
        buf[str_len++] = c;
    }

    char *out = malloc(str_len + 1);
    memcpy(out, buf, str_len);
    out[str_len] = '\0';

    return out;
}

int take_num(FILE *f)
{
    int out = 0;
    char c;
    while(isdigit(c = peek_char(f)) || c == '_') {
        take_char(f);
        if (c == '_') continue;
        out *= 10;
        out += c - '0';
    }

    return out;
}

char *take_string(FILE *f, bool single_quote) {
    size_t cap = 1024;
    size_t len = 0;
    char *buf = malloc(cap);

    bool escaping = false;
    char c;
    char target_quote = single_quote ? '\'' : '"';
    while ((c = take_char(f)) != target_quote || escaping) {
        if (escaping) {
            switch (c) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
            }
        }

        if (escaping || c != '\\') {
            if (len >= cap - 1) buf = realloc(buf, cap *= 2);
            buf[len++] = c;
            escaping = false;
        } else {
            escaping = true;
            continue;
        }
    }

    buf[len] = '\0';
    return buf;
}

Token next_token(FILE *f)
{
    char c;
    for (;;) {
        switch (c = take_char(f)) {
            case '+': return (Token){ TOK_PLUS, 0 };
            case '-': return (Token){ TOK_MINUS, 0 };
            case '(': return (Token){ TOK_LPAREN, 0 };
            case ')': return (Token){ TOK_RPAREN, 0 };
            case '{': return (Token){ TOK_LBRACE, 0 };
            case '}': return (Token){ TOK_RBRACE, 0 };
            case '[': return (Token){ TOK_LBRACKET, 0 };
            case ']': return (Token){ TOK_RBRACKET, 0 };
            case ';': return (Token){ TOK_SEMICOLON, 0 };
            case '=': return (Token){ TOK_EQUALS, 0 };
            case '/': {
                if (peek_char(f) == '/') {
                    take_char(f);
                    char p;
                    while ((p = peek_char(f)) != '\n' && p != '\0') {
                        take_char(f);
                    }
                    continue;
                }
            } break;
            case '\'':
            case '"': {
                char *str = take_string(f, c == '\'');
                return (Token) {
                    .type = TOK_STRING,
                    .value = (TokenValue) {
                        .string_value = str
                    },
                };
            } break;
        }
        
        if (isspace(c)) continue;
        else if (isalpha(c) || c == '_') {
            ungetc(c, f);
            char *ident = take_ident(f);
            TokenType type = ident_to_token_type(ident);
            return (Token) {
                .type = type,
                .value = (TokenValue) {
                    .string_value = type == TOK_IDENT ? ident : 0
                },
            };
        } else if (isdigit(c)) {
            ungetc(c, f);
            int num = take_num(f);
            return (Token) {
                .type = TOK_NUMBER,
                .value = (TokenValue) {
                    .number_value = num
                },
            };
        }

        if (c == EOF) return (Token){ 0 };

        PANIC("Unexpected token '%c'", c);
    }
}

void print_tok(Token tok)
{
    printf("%s ", names[tok.type]);

    switch (tok.type) {
        case TOK_NUMBER:
            printf("%d", tok.value.number_value);
            break;
        case TOK_IDENT:
            printf("%s", tok.value.string_value);
            break;
        case TOK_STRING:
            printf("\"%s\"", tok.value.string_value);
            break;
        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_EOF:
            break;
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    assert(argc == 2);

    Token tok;
    FILE *f = fopen(argv[1], "r");

    do {
        tok = next_token(f);
        print_tok(tok);
    } while (tok.type != TOK_EOF);

    return 0;
}
