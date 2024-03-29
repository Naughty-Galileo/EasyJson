#ifndef EASYJSON_H__
#define EASYJSON_H__
#include <stddef.h>

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

typedef struct value value;
typedef struct member member;

struct value{
    type type;
    union
    {
        // 字符串
        struct
        {
            char* s;
            size_t len;
        } s;
        // 数组
        struct 
        {
            value* e;
            size_t size;
        } a;
        //对象
        struct
        {
            member* m;
            size_t size;
        }o;
        // 数字
        double n;
    }u;    
};

struct member
{
    char* k;      // 键
    size_t klen; // 值
    value v;
};


enum STATE {
    PARSE_OK = 0,
    PARSE_EXPECT_VALUE,
    PARSE_INVALID_VALUE,
    PARSE_ROOT_NOT_SINGULAR,
    PARSE_NUMBER_TOO_BIG,
    PARSE_MISS_QUOTATION_MARK,
    PARSE_INVALID_STRING_ESCAPE,
    PARSE_INVALID_STRING_CHAR,
    PARSE_INVALID_UNICODE_HEX,
    PARSE_INVALID_UNICODE_SURROGATE,
    PARSE_MISS_COMMA_OR_SQUARE_BRACKET,
    PARSE_MISS_KEY,
    PARSE_MISS_COLON,
    PARSE_MISS_COMMA_OR_CURLY_BRACKET
};

#define init(v) do { (v)->type = EASYJson_NULL; } while(0)

int parse(value* v, const char* json);
char* stringify(const value* v, size_t* length);

void Free(value* v);

type get_type(const value* v);

#define set_null(v) Free(v);

int get_boolean(const value* v);
void set_boolean(value* v, int b);

double get_number(const value* v);
void set_number(value* v, double n);

const char* get_string(const value* v);
size_t get_string_length(const value* v);
void set_string(value* v, const char* s, size_t len);

size_t get_array_size(const value* v);
value* get_array_element(const value* v, size_t index);

size_t get_object_size(const value* v);
const char* get_object_key(const value* v, size_t index);
size_t get_object_key_length(const value* v, size_t index);
value* get_object_value(const value* v, size_t index);
}

#endif