/** Hash table in C, adapted from https://github.com/jamesroutley/write-a-hash-table
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


#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "hash-table.h"
#include "min-heap.h"
#include "PacketLoss.h"

static int HT_INITIAL_BASE_SIZE = 997;
static int HT_PRIME_1 = 59;
static int HT_PRIME_2 = 13;
static ht_item HT_DELETED_ITEM = {0, NULL};
static oOS_ht_item OOS_HT_DELETED_ITEM = {0, NULL};


static ht_item* ht_new_item(uint64_t k, struct connStatus* v) {
    ht_item* i = calloc(1, sizeof(ht_item));
    i->key = k;
    i->value = v;
    return i;
}

static ht_hash_table* ht_new_sized(const int base_size) {
    ht_hash_table* ht = calloc(1, sizeof(ht_hash_table));
    ht->base_size = base_size;

    ht->size = next_prime(ht->base_size);

    ht->count = 0;
    ht->items = calloc((size_t)ht->size, sizeof(ht_item*));
    return ht;
}


ht_hash_table* ht_new() {
    return ht_new_sized(HT_INITIAL_BASE_SIZE);
}


static void ht_del_item(ht_item* i) {
    puts("ht del item entered");
    
    int testitemnull = (i->value == NULL);
    printf("test item null : %d; testitem connID: %llx, test item seqNum: %d, test item timestamp %.3f\n", testitemnull, i->key, i->value->seqNum, i->value->timeStamp);
    fflush(stdout);
    free(i->value);
    puts("freed i->value");
    fflush(stdout);
    
    free(i);
    puts("freed i");
    fflush(stdout);
}


void ht_del_hash_table(ht_hash_table* ht) {
    puts("entered del hash table");
    for (int i = 0; i < ht->size; i++) {
        printf("i: %d; size: %d\n", i, ht->size);
        int testitemnull = (ht->items[i] == NULL);
        printf("test item null : %d\n", testitemnull);
        ht_item* item = ht->items[i];
        puts("assigned item");
        if (item != NULL) {
            puts("if statement entered!");
            testitemnull = (item->value == NULL);
            puts("item not null entered!");
            printf("test item null : %d; test item seqNum: %d, test item timestamp %.3f\n", testitemnull, item->value->seqNum, item->value->timeStamp);
        
            ht_del_item(item);
        }
        puts("end if");
    }
    puts("exited for loop");
    free(ht->items);
    puts("freed ht->items");
    free(ht);
}


/* Resizing functions */
static void ht_resize(ht_hash_table* ht, const int base_size) {
    if (base_size < HT_INITIAL_BASE_SIZE) {
        return;
    }
    puts("resizing HT!");
    
    ht_hash_table* new_ht = ht_new_sized(base_size);
    
    for (int i = 0; i < ht->size; i++) {
        // puts("replacing item!");
        // printf("i: %d; size: %d", i, ht->size);
        ht_item* item = ht->items[i];
        if (item != NULL && item != &HT_DELETED_ITEM) {
            ht_insert(new_ht, item->key, item->value);
        }
    }
    ht->base_size = new_ht->base_size;
    ht->count = new_ht->count;

    // To delete new_ht, we give it ht's size and items 
    const int tmp_size = ht->size;
    ht->size = new_ht->size;
    new_ht->size = tmp_size;

    ht_item** tmp_items = ht->items;
    ht->items = new_ht->items;
    new_ht->items = tmp_items;
    puts("before ht_del_hash_table");
    ht_del_hash_table(new_ht);
    puts("after ht_del_hash_table");
}


static void ht_resize_up(ht_hash_table* ht) {
    const int new_size = ht->base_size * 2;
    ht_resize(ht, new_size);
}


static void ht_resize_down(ht_hash_table* ht) {
    const int new_size = ht->base_size / 2;
    ht_resize(ht, new_size);
}

static int ht_hash(uint64_t s, const int a, const int m) {
    unsigned long hash = 0;
    int hexDigit; 
    for (int i = 0; i < 64 / 4; i++) {
        hexDigit = (int) (s % 0x10L);
        hash += (unsigned long) pow(a, hexDigit) * hexDigit; 
        s = s >> 4;
    }
    
    hash = hash % m;
    return (int)hash;
}

static int ht_get_hash(uint64_t s, const int num_buckets, const int attempt) {
    const int hash_a = ht_hash(s, HT_PRIME_1, num_buckets);
    const int hash_b = ht_hash(s, HT_PRIME_2, num_buckets/2); // divide num_buckets to avoid hash_b = num_buckets - 1
    return (hash_a + (attempt * (hash_b + 1))) % num_buckets;
}


void ht_insert(ht_hash_table* ht, uint64_t key, struct connStatus* value) {
    const int load = ht->count * 100 / ht->size;
    if (load > 70) {
        ht_resize_up(ht);
    }
    ht_item* item = ht_new_item(key, value);
    int index = ht_get_hash(item->key, ht->size, 0);
    ht_item* cur_item = ht->items[index];
    int i = 1;
    while (cur_item != NULL) {
        if (cur_item != &HT_DELETED_ITEM) {
            if (cur_item->key == key) {
                ht_del_item(cur_item);
                ht->items[index] = item;
                return;
            }
        }
        index = ht_get_hash(item->key, ht->size, i);
        cur_item = ht->items[index];
        i++;
    } 
    ht->items[index] = item;
    ht->count++;
}

struct connStatus* ht_search(ht_hash_table* ht, uint64_t key) {
    int index = ht_get_hash(key, ht->size, 0);
    ht_item* item = ht->items[index];
    int i = 1;
    while (item != NULL) {
        if (item != &HT_DELETED_ITEM) {
            if (item->key == key) {
                return item->value;
            }
        }
        index = ht_get_hash(key, ht->size, i);
        item = ht->items[index];
        i++;
    } 
    return 0L;
}


void ht_delete(ht_hash_table* ht, uint64_t key) {
    puts("entered ht_delete function!");
    const int load = ht->count * 100 / ht->size;
    if (load < 10) {
        ht_resize_down(ht);
    }
    int index = ht_get_hash(key, ht->size, 0);
    ht_item* item = ht->items[index];
    int i = 1;
    while (item != NULL) {
        if (item != &HT_DELETED_ITEM) {
            if (item->key == key) {
                ht_del_item(item);
                ht->items[index] = &HT_DELETED_ITEM;
                ht->count--;
                return;
            }
        }
        index = ht_get_hash(key, ht->size, i);
        
        //int testitemnull = (ht->items[index] == NULL);
        item = ht->items[index];
        //printf("index: %d; i: %d; ht->items[index] is NULL: %d\n", index, i, testitemnull);
        i++;
    } 
}



// Additional functions for out-of-sequence ("oOS") packets hash table.

static oOS_ht_item* oOS_ht_new_item(uint64_t k, struct heap* v) {
    oOS_ht_item* i = calloc(1, sizeof(oOS_ht_item));
    i->key = k;
    i->value = v;
    return i;
}


static oOS_ht_hash_table* oOS_ht_new_sized(const int base_size) {
    oOS_ht_hash_table* ht = calloc(1, sizeof(oOS_ht_hash_table));
    ht->base_size = base_size;

    ht->size = next_prime(ht->base_size);

    ht->count = 0;
    ht->items = calloc((size_t)ht->size, sizeof(oOS_ht_item*));
    return ht;
}


oOS_ht_hash_table* oOS_ht_new() {
    return oOS_ht_new_sized(HT_INITIAL_BASE_SIZE);
}

static void oOS_ht_del_item(oOS_ht_item* i) {
    puts("ht del item entered");
    int testitemnull = (i->value == NULL);
    printf("test item null : %d; test item seqNum: %d, test item timestamp %.3f\n", testitemnull, heap_front(i->value)->seqNum, heap_front(i->value)->timeStamp);
    heap_term(i->value);
    free(i->value);
    puts("freed i->value");
    free(i);
    puts("freed i");
}


void oOS_ht_del_hash_table(oOS_ht_hash_table* ht) {
    puts("entered del hash table");
    for (int i = 0; i < ht->size; i++) {
        printf("i: %d; size: %d\n", i, ht->size);
        int testitemnull = (ht->items[i] == NULL);
        printf("test item null : %d\n", testitemnull);
        oOS_ht_item* item = ht->items[i];
        if (item != NULL) {
            puts("item not null entered!");
            oOS_ht_del_item(item);
            puts("oOS_ht_del_item exited");
        }
        puts("end if");
    }
    free(ht->items);
    puts("freed oOS_ht->items");
    free(ht);
}


/* Resizing functions */
static void oOS_ht_resize(oOS_ht_hash_table* ht, const int base_size) {
    if (base_size < HT_INITIAL_BASE_SIZE) {
        return;
    }
    puts("resizing oOSHT!");
    
    oOS_ht_hash_table* new_ht = oOS_ht_new_sized(base_size);
    for (int i = 0; i < ht->size; i++) {
        oOS_ht_item* item = ht->items[i];
        if (item != NULL && item != &OOS_HT_DELETED_ITEM) {
            oOS_ht_insert(new_ht, item->key, item->value);
        }
    }

    ht->base_size = new_ht->base_size;
    ht->count = new_ht->count;

    // To delete new_ht, we give it ht's size and items 
    const int tmp_size = ht->size;
    ht->size = new_ht->size;
    new_ht->size = tmp_size;

    oOS_ht_item** tmp_items = ht->items;
    ht->items = new_ht->items;
    new_ht->items = tmp_items;

    oOS_ht_del_hash_table(new_ht);
}


static void oOS_ht_resize_up(oOS_ht_hash_table* ht) {
    const int new_size = ht->base_size * 2;
    oOS_ht_resize(ht, new_size);
}


static void oOS_ht_resize_down(oOS_ht_hash_table* ht) {
    const int new_size = ht->base_size / 2;
    oOS_ht_resize(ht, new_size);
}


static int oOS_ht_hash(uint64_t s, const int a, const int m) {
    unsigned long hash = 0;
    int hexDigit; 
    for (int i = 0; i < 64 / 4; i++) {
        hexDigit = (int) (s % 0x10L);
        hash += (unsigned long) pow(a, hexDigit) * hexDigit; 
        s = s >> 4;
    }
    
    hash = hash % m;
    return (int)hash;
}

static int oOS_ht_get_hash(uint64_t s, const int num_buckets, const int attempt) {
    const int hash_a = ht_hash(s, HT_PRIME_1, num_buckets);
    const int hash_b = ht_hash(s, HT_PRIME_2, num_buckets/2); // divide num_buckets to avoid hash_b = num_buckets - 1
    return (hash_a + (attempt * (hash_b + 1))) % num_buckets;
}


void oOS_ht_insert(oOS_ht_hash_table* ht, uint64_t key, struct heap* value) {
    const int load = ht->count * 100 / ht->size;
    if (load > 70) {
        oOS_ht_resize_up(ht);
    }
    oOS_ht_item* item = oOS_ht_new_item(key, value);
    int index = oOS_ht_get_hash(item->key, ht->size, 0);
    oOS_ht_item* cur_item = ht->items[index];
    int i = 1;
    while (cur_item != NULL) {
        if (cur_item != &OOS_HT_DELETED_ITEM) {
            if (cur_item->key == key) {
                oOS_ht_del_item(cur_item);
                ht->items[index] = item;
                return;
            }
        }
        index = oOS_ht_get_hash(item->key, ht->size, i);
        cur_item = ht->items[index];
        i++;
    } 
    ht->items[index] = item;
    ht->count++;
}

struct heap* oOS_ht_search(oOS_ht_hash_table* ht, uint64_t key) {
    int index = oOS_ht_get_hash(key, ht->size, 0);
    oOS_ht_item* item = ht->items[index];
    int i = 1;
    while (item != NULL) {
        if (item != &OOS_HT_DELETED_ITEM) {
            if (item->key == key) {
                return item->value;
            }
        }
        index = oOS_ht_get_hash(key, ht->size, i);
        item = ht->items[index];
        i++;
    } 
    return NULL;
}


void oOS_ht_delete(oOS_ht_hash_table* ht, uint64_t key) {
    const int load = ht->count * 100 / ht->size;
    if (load < 10) {
        oOS_ht_resize_down(ht);
    }
    int index = oOS_ht_get_hash(key, ht->size, 0);
    oOS_ht_item* item = ht->items[index];
    int i = 1;
    while (item != NULL) {
        if (item != &OOS_HT_DELETED_ITEM) {
            if (item->key == key) {
                oOS_ht_del_item(item);
                ht->items[index] = &OOS_HT_DELETED_ITEM;
                ht->count--;
                return;
            }
        }
        index = oOS_ht_get_hash(key, ht->size, i);
        item = ht->items[index];
        i++;
    } 
    
}