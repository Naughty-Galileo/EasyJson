#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "EasyJson.hpp"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef PARSE_STACK_INIT_SIZE
#define PARSE_STACK_INIT_SIZE 256
#endif

namespace EasyJson {
#define EXPECT(c, ch) do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch) ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch) ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch) do { *(char*)context_push(c, sizeof(char)) = (ch); } while(0)
#define PUTS(c, s, len) memcpy(context_push(c, len), s, len)

struct context
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

/*
解析十六进制
*/
static const char* parse_hex4(const char* p, unsigned* u) {
    *u = 0;
    for (int i = 0; i<4; i++) {
        char ch = *p++;
        *u <<= 4;
        if      (ch >= '0' && ch <= '9')  *u |= ch - '0';
        else if (ch >= 'A' && ch <= 'F')  *u |= ch - ('A' - 10);
        else if (ch >= 'a' && ch <= 'f')  *u |= ch - ('a' - 10);
        else return nullptr;
    }
    return p;
}

/*
编码utf-8
*/
static void encode_utf8(context* c, unsigned u) {
    if (u <= 0x7F) 
        PUTC(c, u & 0xFF);
    else if (u <= 0x7FF) {
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
        PUTC(c, 0x80 | ( u       & 0x3F));
    }
    else if (u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
    else {
        assert(u <= 0x10FFFF);
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
}

#define STRING_ERROR(err) do{ c->top = head; return err; }while(0)

/*
解析字符串
*/
static int parse_string_raw(context* c, char** str, size_t* len) {
    size_t head = c->top;
    unsigned u, u2;
    const char* p;
    EXPECT(c, '\"');
    p = c->json;
    for(;;) {
        char ch = *p++;
        switch (ch)
        {
        case '\"':
            *len = c->top - head;
            *str = (char *)context_pop(c, *len);
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
                    case 'u':
                        if (!(p = parse_hex4(p, &u))) {
                            STRING_ERROR(PARSE_INVALID_UNICODE_HEX);
                        }
                        if (u >= 0xD800 && u <= 0xDBFF) { /* surrogate pair */
                            if (*p++ != '\\')
                                STRING_ERROR(PARSE_INVALID_UNICODE_SURROGATE);
                            if (*p++ != 'u')
                                STRING_ERROR(PARSE_INVALID_UNICODE_SURROGATE);
                            if (!(p = parse_hex4(p, &u2)))
                                STRING_ERROR(PARSE_INVALID_UNICODE_HEX);
                            if (u2 < 0xDC00 || u2 > 0xDFFF)
                                STRING_ERROR(PARSE_INVALID_UNICODE_SURROGATE);
                            u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                        }
                        encode_utf8(c, u);
                        break;
                    default:
                        STRING_ERROR(PARSE_INVALID_STRING_ESCAPE);
                }
                break;
        case '\0':
            STRING_ERROR(PARSE_MISS_QUOTATION_MARK);
        default:
            if ((unsigned char)ch < 0x20) { 
                STRING_ERROR(PARSE_INVALID_STRING_CHAR);
            }
            PUTC(c, ch);
        }
    }
}

static int parse_string(context* c, value* v) {
    int ret;
    char* s;
    size_t len;
    if ((ret = parse_string_raw(c, &s, &len)) == PARSE_OK)
        set_string(v, s, len);
    return ret;
}

static int parse_value(context* c, value* v);/*前向声明*/

static int parse_array(context* c, value* v) {
    size_t i, size = 0;
    int ret;
    EXPECT(c, '[');
    parse_whitespace(c);

    if (*c->json == ']') {
        c->json++;
        v->type = EASYJson_ARRAY;
        v->u.a.size = 0;
        v->u.a.e = nullptr;
        return PARSE_OK;
    }
    for(;;) {
        value e;
        init(&e);
        if ((ret = parse_value(c ,&e)) != PARSE_OK)
            break;
        memcpy(context_push(c, sizeof(value)), &e, sizeof(value));
        size++;

        parse_whitespace(c);
        if (*c->json == ',') {
            c->json ++;
            parse_whitespace(c);
        }
        else if (*c->json == ']') {
            c->json++;
            v->type = EASYJson_ARRAY;
            v->u.a.size = size;
            size *= sizeof(value);
            memcpy(v->u.a.e = (value*)malloc(size), context_pop(c, size), size);
            return PARSE_OK;
        }
        else {
            ret = PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        }
    }
    for (int i = 0; i < size; i++)
        Free((value*)context_pop(c, sizeof(value)));
    return ret;
}

static int parse_object(context* c, value* v) {
    size_t i, size;
    member m;
    int ret;
    EXPECT(c, '{');
    parse_whitespace(c);
    if (*c->json == '}') {
        c->json++;
        v->type = EASYJson_OBJECT;
        v->u.o.m = nullptr;
        v->u.o.size = 0;
        return PARSE_OK;
    }

    m.k = nullptr;
    size = 0;
    for (;;) {
        char* str;
        init(&m.v);
        /* \todo parse key to m.k, m.klen */
        if (*c->json != '"') {
            ret = PARSE_MISS_KEY;
            break;
        }
        if ((ret = parse_string_raw(c, &str, &m.klen)) != PARSE_OK) {
            break;
        }
        memcpy(m.k = (char*)malloc(m.klen + 1), str, m.klen);
        m.k[m.klen] = '\0';

        parse_whitespace(c);
        /* \todo parse ws colon ws */
        if (*c->json != ':') {
            ret = PARSE_MISS_COLON;
            break;
        }
        c->json++;
        parse_whitespace(c);
        /* parse value */
        if ((ret = parse_value(c, &m.v)) != PARSE_OK)
            break;
        memcpy(context_push(c, sizeof(member)), &m, sizeof(member));
        size++;
        m.k = nullptr; /* ownership is transferred to member on stack */
        /* \todo parse ws [comma | right-curly-brace] ws */
        parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            parse_whitespace(c);
        }
        else if (*c->json == '}') {
            size_t s = sizeof(member) * size;
            c->json++;
            v->type = EASYJson_OBJECT;
            v->u.o.size = size;
            memcpy(v->u.o.m = (member*)malloc(s), context_pop(c, s), s);
            return PARSE_OK;
        }
        else {
            ret = PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
    }
    /* \todo Pop and free members on the stack */
    free(m.k);
    for (i = 0; i < size; i++) {
        member* m = (member*)context_pop(c, sizeof(member));
        free(m->k);
        Free(&m->v);
    }
    v->type = EASYJson_NULL;
    return ret;
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
    case '[':
        return parse_array(c, v);
    case '{':
        return parse_object(c, v);
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

#ifndef PARSE_STRINGIFY_INIT_SIZE
#define PARSE_STRINGIFY_INIT_SIZE 256
#endif

static void stringify_string(context* c, const char* s, size_t len) {
    static const char hex_digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    size_t i, size;
    char* head, *p;
    assert(s != nullptr);
    p = head = (char*)context_push(c, size = len * 6 + 2); /* "\u00xx..." */
    *p++ = '"';
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        switch (ch) {
            case '\"': *p++ = '\\'; *p++ = '\"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\b': *p++ = '\\'; *p++ = 'b';  break;
            case '\f': *p++ = '\\'; *p++ = 'f';  break;
            case '\n': *p++ = '\\'; *p++ = 'n';  break;
            case '\r': *p++ = '\\'; *p++ = 'r';  break;
            case '\t': *p++ = '\\'; *p++ = 't';  break;
            default:
                if (ch < 0x20) {
                    *p++ = '\\'; *p++ = 'u'; *p++ = '0'; *p++ = '0';
                    *p++ = hex_digits[ch >> 4];
                    *p++ = hex_digits[ch & 15];
                }
                else
                    *p++ = s[i];
        }
    }
    *p++ = '"';
    c->top -= size - (p - head);
}

static void stringify_value(context* c, const value* v) {
    size_t i;
    switch (v->type) {
        case EASYJson_NULL:   PUTS(c, "null",  4); break;
        case EASYJson_FALSE:  PUTS(c, "false", 5); break;
        case EASYJson_TRUE:   PUTS(c, "true",  4); break;
        case EASYJson_NUMBER: c->top -= 32 - sprintf((char *)context_push(c, 32), "%.17g", v->u.n); break;
        case EASYJson_STRING: stringify_string(c, v->u.s.s, v->u.s.len); break;
        case EASYJson_ARRAY:
            PUTC(c, '[');
            for (i = 0; i < v->u.a.size; i++) {
                if (i > 0)
                    PUTC(c, ',');
                stringify_value(c, &v->u.a.e[i]);
            }
            PUTC(c, ']');
            break;
        case EASYJson_OBJECT:
            PUTC(c, '{');
            for (i = 0; i < v->u.o.size; i++) {
                if (i > 0)
                    PUTC(c, ',');
                stringify_string(c, v->u.o.m[i].k, v->u.o.m[i].klen);
                PUTC(c, ':');
                stringify_value(c, &v->u.o.m[i].v);
            }
            PUTC(c, '}');
            break;
        default: assert(0 && "invalid type");
    }
}

char* stringify(const value* v, size_t* length) {
    context c;
    assert(v != nullptr);
    c.stack = (char*)malloc(c.size = PARSE_STRINGIFY_INIT_SIZE);
    c.top = 0;
    stringify_value(&c, v);
    if (length)
        *length = c.top;
    PUTC(&c, '\0');
    return c.stack;
}

void Free(value* v) {
    size_t i;
    assert(v != NULL);
    switch (v->type)
    {
    case EASYJson_STRING:
        free(v->u.s.s);
        break;
    case EASYJson_ARRAY:
        for (i = 0; i < v->u.a.size; i++) Free(&v->u.a.e[i]);
        free(v->u.a.e);
        break;
    case EASYJson_OBJECT:
        for (i = 0; i < v->u.o.size; i++) {
            free(v->u.o.m[i].k);
            Free(&v->u.o.m[i].v);
        }
        free(v->u.o.m);
        break;
    default:
        break;
    }
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

size_t get_array_size(const value* v) {
    assert(v != NULL && v->type == EASYJson_ARRAY);
    return v->u.a.size;
}

value* get_array_element(const value* v, size_t index) {
    assert(v != NULL && v->type == EASYJson_ARRAY);
    assert(index < v->u.a.size);
    return &v->u.a.e[index];
}

size_t get_object_size(const value* v) {
    assert(v != NULL && v->type == EASYJson_OBJECT);
    return v->u.o.size;
}

const char* get_object_key(const value* v, size_t index) {
    assert(v != nullptr && v->type == EASYJson_OBJECT);
    assert(index < v->u.o.size);
    return v->u.o.m[index].k;
}

size_t get_object_key_length(const value* v, size_t index) {
    assert(v != nullptr && v->type == EASYJson_OBJECT);
    assert(index < v->u.o.size);
    return v->u.o.m[index].klen;
}
value* get_object_value(const value* v, size_t index) {
    assert(v != nullptr && v->type == EASYJson_OBJECT);
    assert(index < v->u.o.size);
    return &(v->u.o.m[index].v);
}
}