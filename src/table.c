#include "table.h"
#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "object.h"

#define TABLE_MAX_LOAD 0.75

void init_table(Table* const table) {
    table->capacity = 0;
    table->count = 0;
    table->entries = nullptr;
}

void free_table(Table* const table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    init_table(table);
}

[[nodiscard]] static Entry* find_entry(Entry* const entries, int const capacity, ObjString const* const key) {
    auto index = key->hash % (uint32_t)capacity;
    auto tombstone = (Entry*)nullptr;
    for (;;) {
        auto const entry = &entries[index];
        if (entry->key == nullptr) {
            if (IS_NIL(entry->value)) {
                // Empty entry.
                return tombstone != nullptr ? tombstone : entry;
            }
            // We found a tombstone.
            if (tombstone == nullptr) {
                tombstone = entry;
            }
        } else if (entry->key == key) {
            // We found the key.
            return entry;
        }

        index = (index + 1) % (uint32_t)capacity;
    }
}

[[nodiscard]] bool table_get(Table const* const table, ObjString const* const key, Value* const value) {
    if (table->count == 0) {
        return false;
    }

    auto const entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == nullptr) {
        return false;
    }

    *value = entry->value;
    return true;
}

static void adjust_capacity(Table* const table, int const capacity) {
    auto const entries = ALLOCATE(Entry, capacity);
    for (auto i = 0; i < capacity; ++i) {
        entries[i].key = nullptr;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (auto i = 0; i < table->capacity; ++i) {
        auto const entry = &table->entries[i];
        if (entry->key == nullptr) {
            continue;
        }
        auto const dest = find_entry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        ++table->count;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);

    table->entries = entries;
    table->capacity = capacity;
}

bool table_set(Table* const table, ObjString* const key, Value const value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        auto const capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity(table, capacity);
    }

    auto const entry = find_entry(table->entries, table->capacity, key);
    auto const is_new_key = (entry->key == nullptr);
    if (is_new_key && IS_NIL(entry->value)) {
        ++table->count;
    }

    entry->key = key;
    entry->value = value;
    return is_new_key;
}

bool table_delete(Table const* table, ObjString const* key) {
    if (table->count == 0) {
        return false;
    }

    // Find the entry.
    auto const entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == nullptr) {
        return false;
    }

    // Place a tombstone in the entry.
    entry->key = nullptr;
    entry->value = BOOL_VAL(true);
    return true;
}

void table_add_all(Table* const from, Table* const to) {
    for (auto i = 0; i < from->capacity; ++i) {
        auto const entry = &from->entries[i];
        if (entry->key != nullptr) {
            table_set(to, entry->key, entry->value);
        }
    }
}

[[nodiscard]] ObjString const*
    table_find_string(Table const* const table, char const* const chars, int const length, uint32_t const hash) {
    if (table->count == 0) {
        return nullptr;
    }

    auto index = hash % (uint32_t)table->capacity;
    for (;;) {
        auto const entry = &table->entries[index];
        if (entry->key == nullptr) {
            // Stop if we find an empty non-tombstone entry.
            if (IS_NIL(entry->value)) {
                return nullptr;
            }
        } else if (entry->key->length == length and entry->key->hash == hash
                   and memcmp(entry->key->chars, chars, (size_t)length) == 0) {
            // We found it.
            return entry->key;
        }

        index = (index + 1) % (uint32_t)table->capacity;
    }
}
