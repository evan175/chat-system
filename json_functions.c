#include <stdio.h>   
#include <stdlib.h>   
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "cJSON.h"
#include "json_functions.h"

//store file contents in string and return string, make sure to free the returned string after use
char* json_read_file(char* filename) {
    FILE *fptr = fopen(filename, "r");
    if (fptr == NULL) {
        printf("Error opening file\n");
        return NULL;
    }

    fseek(fptr, 0, SEEK_END);
    long file_size = ftell(fptr);
    rewind(fptr);

    char *buffer = (char *)malloc(file_size + 1);
    if (buffer == NULL) {
        perror("Error allocating memory");
        fclose(fptr);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, file_size, fptr);
    if (bytes_read != file_size) {
        perror("Error reading file");
        free(buffer);
        fclose(fptr);
        return NULL;
    }

    buffer[file_size] = '\0';

    fclose(fptr);

    return buffer;
}

//TODO: overide existing value if key already exists.
void json_set_val(char* key, int val) {
    char* json_str = json_read_file("data.txt");

    cJSON *json;

    if (json_str == NULL || json_str[0] == '\0') {
        printf("File not found or empty. Creating new JSON object.\n");
        json = cJSON_CreateObject();
    } else {
        json = cJSON_Parse(json_str);
    }

    if (json == NULL) {
        printf("Error parsing JSON\n");
        free(json_str);
        return;
    }

    cJSON *val_obj = cJSON_CreateNumber(val);
    if(val_obj == NULL) {
        printf("Error creating JSON number\n");
        cJSON_Delete(json);
        free(json_str);
        return;
    }

    //check if key is already in json
    cJSON *existing_item = cJSON_GetObjectItemCaseSensitive(json, key);
    if(existing_item != NULL) {
        printf("Key '%s' already exists. Updating value to %d.\n", key, val);
        existing_item->type = cJSON_Number;
        cJSON_SetNumberValue(existing_item, val);
    } else {
        cJSON_AddItemToObject(json, key, val_obj);
    }

    char* updated_json_str = cJSON_Print(json);
    if (updated_json_str == NULL) {
        printf("Error printing JSON\n");
        cJSON_Delete(json);
        free(json_str);
        return;
    }
    
    FILE *fptr = fopen("data.txt", "w");
    if (fptr == NULL) {
        printf("Error opening file for writing\n");
        free(updated_json_str);
        cJSON_Delete(json);
        return;
    }

    fprintf(fptr, "%s", updated_json_str);
    fclose(fptr);

    printf("Stored key '%s' with value %d in JSON file.\n", key, val);

    free(updated_json_str);
    free(json_str);
    cJSON_Delete(json);
}

JsonError json_get_val(char* key, int* out_val) {
    char* json_str = json_read_file("data.txt");
    if (json_str == NULL || json_str[0] == '\0') {
        printf("File not found or empty.\n");
        return JSON_FILE_ERROR;
    }

    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL) {
        printf("Error parsing JSON\n");
        free(json_str);
        return JSON_PARSE_ERROR;
    }

    cJSON *val_item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (val_item == NULL || !cJSON_IsNumber(val_item)) {
        printf("Key not found or val is not a number.\n");
        cJSON_Delete(json);
        free(json_str);
        return JSON_KEY_NOT_FOUND;
    }

    int val = val_item->valueint;

    cJSON_Delete(json);
    free(json_str);

    *out_val = val;
    return JSON_SUCCESS;
}