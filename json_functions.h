#ifndef JSON_FUNCTIONS_H
#define JSON_FUNCTIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

//TODO add doxygyn comments to these functions

typedef enum {
    JSON_SUCCESS = 0,
    JSON_FILE_ERROR = -1,
    JSON_PARSE_ERROR = -2,
    JSON_KEY_NOT_FOUND = -3,
    JSON_NOT_A_NUMBER = -4
} JsonError;

char *json_read_file(char *filename);

void json_set_val(char *key, int val);

JsonError json_get_val(char *key, int *out_val);

#endif