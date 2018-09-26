/* Hash table in C, adapted from https://github.com/jamesroutley/write-a-hash-table 
 * MIT License
 * Copyright (c) 2017 James Routley
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
 * and associated documentation files (the "Software"), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, 
 * subject to the following conditions: 
 * 
 * The above copyright notice and this permission notice shall be included in all copies or substantial 
 * portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT 
 * LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN 
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 **/

typedef struct {
    uint64_t key;
    unsigned long value;
} ht_item;

typedef struct {
    uint64_t key;
    struct heap* value;
} oOS_ht_item;

typedef struct {
    int base_size;
    int size;
    int count;
    ht_item** items;
} ht_hash_table;

typedef struct {
    int base_size;
    int size;
    int count;
    oOS_ht_item** items;
} oOS_ht_hash_table;

ht_hash_table* ht_new();
void ht_insert(ht_hash_table* ht, uint64_t key, unsigned long value);
unsigned long ht_search(ht_hash_table* ht, uint64_t key);
void ht_delete(ht_hash_table* h, uint64_t key);

void oOS_ht_insert(oOS_ht_hash_table* ht, uint64_t key, struct heap* value);
struct heap* oOS_ht_search(oOS_ht_hash_table* ht, uint64_t key);
void oOS_ht_delete(oOS_ht_hash_table* h, uint64_t key);
