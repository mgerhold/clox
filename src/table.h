#pragma once

#include "common.h"
#include "value.h"

typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
    int count; // total number of elements and tombstones
    int capacity;
    Entry* entries;
} Table;

void init_table(Table* table);
void free_table(Table* table);
[[nodiscard]] bool table_get(Table const* table, ObjString const* key, Value* value);
bool table_set(Table* table, ObjString* key, Value value);
bool table_delete(Table const* table, ObjString const* key);
void table_add_all(Table* from, Table* to);
[[nodiscard]] ObjString const* table_find_string(Table const* table, char const* chars, int length, uint32_t hash);
