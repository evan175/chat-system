#ifndef JSON_FUNCTIONS_H
#define JSON_FUNCTIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

char *json_read_file(char *filename);

void json_set_val(char *key, int val);

int json_get_val(char *key);

#endif