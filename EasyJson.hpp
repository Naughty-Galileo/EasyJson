#ifndef EASYJSON_H__
#define EASYJSON_H__

namespace EasyJson
{
enum type {
    EASYJson_NULL,
    EASYJson_FALSE,
    EASYJson_TRUE,
    EASYJson_NUMBER,
    EASYJson_STRING,
    EASYJson_ARRAY,
    EASYJson_OBJECT
};

typedef struct {
    type type;
    double n;
} value;

enum STATE {
    PARSE_OK = 0,
    PARSE_EXPECT_VALUE,
    PARSE_INVALID_VALUE,
    PARSE_ROOT_NOT_SINGULAR,
    PARSE_NUMBER_TOO_BIG
};

int parse(value* v, const char* json);

type get_type(const value* v);

double get_number(const value* v);

}
#endif