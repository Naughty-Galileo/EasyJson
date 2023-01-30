#include "EasyJson.hpp"
#include <assert.h>
#include <stdlib.h>
#include  <errno.h>
#include <math.h>

namespace EasyJson {
#define EXPECT(c, ch) do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch) ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch) ((ch) >= '1' && (ch) <= '9')

typedef struct  context
{
    const char* json;
};

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
    v->n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->n == HUGE_VAL || v->n == -HUGE_VAL))
        return PARSE_NUMBER_TOO_BIG;
    
    v->type = EASYJson_NUMBER;
    c->json = p;
    return PARSE_OK;
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
    case '\0':
        return PARSE_EXPECT_VALUE;
    }
}



int parse(EasyJson::value* v, const char* json) {
    context c;
    assert(v != NULL);
    int ret;

    c.json = json;
    v->type = EASYJson_NULL;
    parse_whitespace(&c);

    if ((ret = parse_value(&c, v)) == PARSE_OK) {
        parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = EASYJson_NULL;
            ret = PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

type get_type(const value* v) {
    assert(v != NULL);
    return v->type;
}

double get_number(const value* v) {
    assert(v != NULL && v->type == EASYJson_NUMBER);
    return v->n;
}

}