#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "EasyJson.hpp"
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>

#ifndef PARSE_STACK_INIT_SIZE
#define PARSE_STACK_INIT_SIZE 256
#endif

namespace EasyJson {
#define EXPECT(c, ch) do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch) ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch) ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch) do { *(char*)context_push(c, sizeof(char)) = (ch); } while(0)

typedef struct  context
{
    const char* json;
    char* stack;
    size_t size, top;
};

static void* context_push(context* c, size_t size) {
    void* ret;
    assert(size > 0);
    if (c->top + size >= c->size) {
        if (c->size == 0)
            c->size = PARSE_STACK_INIT_SIZE;
        while (c->top + size >= c->size)
            c->size += c->size >> 1;  /* c->size * 1.5 */
        c->stack = (char*)realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

static void* context_pop(context* c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

static void parse_whitespace(context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;    
}

/*
解析null/true/false
*/
static int parse_literal(context* c, EasyJson::value* v, const char* literal, type Type) {
    size_t i;
    EXPECT(c, literal[0]);
    for (i = 0; literal[i+1]; i++)
        if (c->json[i] != literal[i+1])
            return PARSE_INVALID_VALUE;

    c->json += i;
    v->type = Type;
    return PARSE_OK;
}

/*
解析数字
*/
static int parse_number(context* c, value* v) {
    const char* p = c->json;
    if (*p == '-') p++;

    if (*p == '0') p++;
    else {
        if( !ISDIGIT1TO9(*p) ) return PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    
    if (*p == '.') {
        p++;
        if (!ISDIGIT(*p)) return PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!ISDIGIT(*p)) return PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
        return PARSE_NUMBER_TOO_BIG;
    
    v->type = EASYJson_NUMBER;
    c->json = p;
    return PARSE_OK;
}

static int parse_string(context* c, value* v) {
    size_t head = c->top, len;
    const char* p;
    EXPECT(c, '\"');
    p = c->json;
    for(;;) {
        char ch = *p++;
        switch (ch)
        {
        case '\"':
            len = c->top - head;
            set_string(v, (const char*)context_pop(c, len), len);
            c->json = p;
            return PARSE_OK;
        case '\\':
                switch (*p++) {
                    case '\"': PUTC(c, '\"'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/':  PUTC(c, '/' ); break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    default:
                        c->top = head;
                        return PARSE_INVALID_STRING_ESCAPE;
                }
                break;
        case '\0':
            c->top = head;
            return PARSE_MISS_QUOTATION_MARK;
        default:
            if ((unsigned char)ch < 0x20) { 
                c->top = head;
                return PARSE_INVALID_STRING_CHAR;
            }
            PUTC(c, ch);
        }
    }
}

static int parse_value(context* c, value* v) {
    switch (*c->json)
    {
    case 'n':
        return parse_literal(c, v, "null", EASYJson_NULL);
    case 't':
        return parse_literal(c, v, "true", EASYJson_TRUE);
    case 'f':
        return parse_literal(c, v, "false", EASYJson_FALSE);
    default:
        return parse_number(c, v);
    case '"':
        return parse_string(c, v);
    case '\0':
        return PARSE_EXPECT_VALUE;
    }
}

int parse(EasyJson::value* v, const char* json) {
    context c;
    assert(v != NULL);
    int ret;

    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    init(v);
    parse_whitespace(&c);

    if ((ret = parse_value(&c, v)) == PARSE_OK) {
        parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = EASYJson_NULL;
            ret = PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

void Free(value* v) {
    assert(v != NULL);
    if (v->type == EASYJson_STRING)
        free(v->u.s.s);
    v->type = EASYJson_NULL;
}

type get_type(const value* v) {
    assert(v != NULL);
    return v->type;
}

int get_boolean(const value* v) {
    assert(v != NULL && (v->type == EASYJson_TRUE || v->type == EASYJson_FALSE));
    return v->type == EASYJson_TRUE;
}

void set_boolean(value* v, int b) {
    Free(v);
    v->type = b ? EASYJson_TRUE :  EASYJson_FALSE;
}

double get_number(const value* v) {
    assert(v != NULL && v->type == EASYJson_NUMBER);
    return v->u.n;
}

void set_number(value* v, double n) {
    Free(v);
    v->u.n = n;
    v->type = EASYJson_NUMBER;
}

const char* get_string(const value* v) {
    assert(v != NULL && v->type == EASYJson_STRING);
    return v->u.s.s;
}

size_t get_string_length(const value* v) {
    assert(v != NULL && v->type == EASYJson_STRING);
    return v->u.s.len;
}

void set_string(value* v, const char* s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    Free(v);
    v->u.s.s = (char*)malloc(len + 1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = EASYJson_STRING; 
}

}