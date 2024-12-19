#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>




#define GALBA_HASH_DEFAULT_CAPACITY_MAGNITUDE 8
#define GALBA_HASH_MAX_LOAD 0.75



#define GALBA_HASH_EMPTY 0
#define GALBA_HASH_DELETED 1
#define GALBA_HASH_OCCUPIED 2



uint32_t GH_base_hash(uint32_t a)
{
   a = (a+0x7ed55d16) + (a<<12);
   a = (a^0xc761c23c) ^ (a>>19);
   a = (a+0x165667b1) + (a<<5);
   a = (a+0xd3a2646c) ^ (a<<9);
   a = (a+0xfd7046c5) + (a<<3);
   a = (a^0xb55a4f09) ^ (a>>16);
   return a;
}

uint32_t hash_1( uint32_t a){
    a = (a ^ 61) ^ (a >> 16);
    a = a + (a << 3);
    a = a ^ (a >> 4);
    a = a * 0x27d4eb2d;
    a = a ^ (a >> 15);
    return a;
}

//Here's a 5-shift one where you have to use the high bits, hash >> (32-logSize), because the low bits are hardly mixed at all
uint32_t hash_2( uint32_t a)
{
    a = (a+0x479ab41d) + (a<<8);
    a = (a^0xe4aa10ce) ^ (a>>5);
    a = (a+0x9942f0a6) - (a<<14);
    a = (a^0x5aedd67d) ^ (a>>3);
    a = (a+0x17bea992) + (a<<7);
    return a;
}

uint32_t hash_3( uint32_t a)
{
    a -= (a<<6);
    a ^= (a>>17);
    a -= (a<<9);
    a ^= (a<<4);
    a -= (a<<3);
    a ^= (a<<10);
    a ^= (a>>15);
    return a;
}

static inline uint64_t xorshift(const uint64_t n,int i){
  return n^(n>>i);
}
uint64_t some_random_hash(const uint64_t n){// https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
  uint64_t p = 0x5555555555555555ull; // pattern of alternating 0 and 1
  uint64_t c = 17316035218449499591ull;// random uneven integer constant; 
  return c*xorshift(p*xorshift(n,32),32);
}


static inline uint64_t ror64(uint64_t v, int r) {
    return (v >> r) | (v << (64 - r));
}

uint64_t rrxmrrxmsx_0(uint64_t v) {
    v ^= ror64(v, 25) ^ ror64(v, 50);
    v *= 0xA24BAED4963EE407UL;
    v ^= ror64(v, 24) ^ ror64(v, 49);
    v *= 0x9FB21C651E98DF25UL;
    return v ^ v >> 28;
}

typedef struct{
    int32_t key;
    int32_t value;
}GHEntry;

typedef struct{
    size_t capacity;
    size_t cells_loaded;   // deleted + occupied
    size_t cells_deleted;  // just deleted
    size_t max_load;
    uint32_t(*hash_function)(uint32_t);
    GHEntry* entries;
    char* visited_status;

}GalbaHash;



static void INTERNAL_GH_realloc_add_entry(GHEntry* entries, char* visited_status, size_t capacity, GHEntry entry, uint32_t hash){
    size_t current_index = hash & (capacity - 1);

    while(true){
        bool empty = visited_status[current_index] == GALBA_HASH_EMPTY;

        if(empty){
            entries[current_index] = entry;
            visited_status[current_index] = GALBA_HASH_OCCUPIED;
            return;
        }

        current_index += 1;
        current_index &= (capacity - 1);
        // wrap around and start from beginning
    }

}

static bool INTERNAL_GH_reallocate_table(GalbaHash*table){
    size_t old_capacity = table->capacity;
    size_t new_capacity = old_capacity * 2;
    GHEntry* old_entries = table->entries;
    char* old_visited_status = table->visited_status;
    void* buffer = malloc((sizeof(GHEntry) + sizeof(char)) * new_capacity);
    if(!buffer){
        return false;
    }


    table->capacity = new_capacity;
    table->cells_loaded = table->cells_loaded - table->cells_deleted;
    table->cells_deleted = 0;
    table->max_load = (size_t)(new_capacity* GALBA_HASH_MAX_LOAD);
    table->entries = buffer;
    table->visited_status = (char *)(table->entries + new_capacity);
    memset(table->visited_status, 0, new_capacity);

    for(size_t i = 0; i<old_capacity; i++){
        if(old_visited_status[i] == GALBA_HASH_OCCUPIED){
            GHEntry valid_entry = old_entries[i];
            uint32_t hash = table->hash_function((uint32_t)valid_entry.key);
            INTERNAL_GH_realloc_add_entry(table->entries, table->visited_status, new_capacity, valid_entry, hash);
        }
    }


    free(old_entries);
    return true;
}

static GHEntry* INTERNAL_GH_put_overwrite(GalbaHash* table, size_t index, int32_t key, int32_t value){
    table->entries[index].value = value;

    return &(table->entries[index]);
}
static GHEntry* INTERNAL_GH_put_emptywrite(GalbaHash* table, size_t index, int32_t key, int32_t value){
    table->cells_loaded += 1;
    table->visited_status[index] = GALBA_HASH_OCCUPIED;
    table->entries[index] = (GHEntry){.key = key, .value =value};
    return &(table->entries[index]);

}

static void INTERNAL_GH_deletion_traverse(GalbaHash* table, size_t start_index){
    size_t next_index = (start_index + 1 ) & (table->capacity - 1);
    size_t prev_index = (start_index - 1 ) & (table->capacity - 1);
    int direction;
    if (table->visited_status[next_index] == GALBA_HASH_EMPTY){
        direction = -1;
    }else if(table->visited_status[prev_index] == GALBA_HASH_EMPTY){
        direction = 1;
    }else{
        return;
    }

    size_t current_index = start_index;
    // if next/prev cell is empty, to the left/right until a occupied or empty slot is encountered
    // change status of each deleted cell to empty
    while(table->visited_status[current_index] == GALBA_HASH_DELETED ){
        table->visited_status[current_index] =  GALBA_HASH_EMPTY;
        table->cells_deleted -= 1;
        table->cells_loaded -= 1;

        current_index += direction;
        current_index &= (table->capacity - 1);
        // wrap around and start from the end
    }

}

GHEntry* GH_put(GalbaHash*table, int32_t key, int32_t value){
    if (table->cells_loaded == table->max_load){
    bool success = INTERNAL_GH_reallocate_table(table);
        if (!success){
            return NULL;
        }
    }


    uint32_t hash = table->hash_function((uint32_t)key);
    size_t current_index = hash & (table->capacity - 1);



    while(true){
        bool present = 
        (table->visited_status[current_index] == GALBA_HASH_OCCUPIED 
        && 
        table->entries[current_index].key == key);

        if (present){
            return INTERNAL_GH_put_overwrite(table, current_index, key, value);
        }

        bool empty = table->visited_status[current_index] == GALBA_HASH_EMPTY;

        if(empty){
            return INTERNAL_GH_put_emptywrite(table, current_index, key, value);
        }


        current_index += 1;
        current_index &= (table->capacity - 1);
        // wrap around and start from the end

    }
}


GHEntry* GH_find(GalbaHash*table, int32_t key){
    uint32_t hash = table->hash_function((uint32_t)key);
    size_t current_index = hash & (table->capacity - 1);

    while(true){
        bool present = 
        (table->visited_status[current_index] == GALBA_HASH_OCCUPIED 
        && 
        table->entries[current_index].key == key);

        if (present){
            return &(table->entries[current_index]);
        }

        bool empty = table->visited_status[current_index] == GALBA_HASH_EMPTY;

        if(empty){
            return NULL;
        }

        current_index += 1;
        current_index &= (table->capacity - 1);
        // wrap around and start from the end
        }

}


void GH_delete_ptr(GalbaHash*table, GHEntry* entry){
    // deletes entry if it's occupied, otherwise does nothing
    size_t current_index = entry - table->entries;
    if(table->visited_status[current_index] != GALBA_HASH_OCCUPIED){
        return;
    }

    table->visited_status[current_index] = GALBA_HASH_DELETED;
    table->cells_deleted += 1;
    INTERNAL_GH_deletion_traverse(table, current_index);
}


//todo make an implementation that keeps track of entries to delete
GHEntry* GH_delete_key(GalbaHash*table, int32_t key){
    // deletes entry if it's found and returns pointer to it
    // if entry doesn't exist does nothing and returns NULL
    // pointer to deleted entry is guaranteed to be valid until GH_put is called on the hash table  

    GHEntry* found = GH_find(table, key);
    if(found){
        GH_delete_ptr(table, found);
    }
    
    return found;
}

void GH_print_struct(GalbaHash* to_print){
    printf(
        "Capacity:%llu\n"
        "Cells loaded:%llu\n"
        "Cells deleted:%llu\n"
        "Max load:%llu\n",
        to_print->capacity,
        to_print->cells_loaded,
        to_print->cells_deleted,
        to_print->max_load);
}


void GH_print_data_debug(GalbaHash* to_print){
    puts("GH debug status: ");
    static const char* status_tab[3] = {"EMPTY  ", "DELETED", "ACTIVE "};
    static const int max_print = 11;

    for(size_t i=0; i<to_print->capacity; i++){
        int written;
        printf("%s", status_tab[(int)to_print->visited_status[i]]);
        printf("|key: ");
        written = printf("%d", to_print->entries[i].key);
        printf("%*s ", max_print - written, "");
        printf("|val: ");
        written = printf("%d", to_print->entries[i].value);
        printf("%*s |", max_print - written, "");

        if(to_print->visited_status[i] == GALBA_HASH_OCCUPIED){
            uint32_t hash = to_print->hash_function((uint32_t)to_print->entries[i].key);
            printf("id: %llu origin: %llu", i , hash & (to_print->capacity -1 ));

        }
        printf("\n");
    }

}

void GH_print_data(GalbaHash* to_print){
    puts("\nEntries status: ");
    for(size_t i=0; i<to_print->capacity; i++){
        if(to_print->visited_status[i] == GALBA_HASH_OCCUPIED)
        printf("{%d: %d}, ", to_print->entries[i].key, to_print->entries[i].value);
    }
    puts("");
}


bool GH_create(GalbaHash* new_structure, uint8_t capacity_magnitude, uint32_t (*hash_function)(uint32_t)){
    if (! capacity_magnitude){
        capacity_magnitude = GALBA_HASH_DEFAULT_CAPACITY_MAGNITUDE;
    }
    size_t capacity = ((size_t)1) << capacity_magnitude;
    void* buffer = malloc((sizeof(GHEntry) + sizeof(char)) * capacity);
    if(buffer == NULL){
        return false;
    }


    new_structure->capacity = capacity;
    new_structure->cells_loaded = 0;
    new_structure->cells_deleted = 0;
    new_structure->max_load = (size_t)(capacity*GALBA_HASH_MAX_LOAD);
    new_structure->hash_function = hash_function ? hash_function: GH_base_hash;

    new_structure->entries = buffer;
    new_structure->visited_status = (char *)(new_structure->entries + capacity);
    memset(new_structure->visited_status, 0, capacity);

    return true;

}


void GH_destroy(GalbaHash* to_destroy){
    free(to_destroy->entries);
}


int main() {
    GalbaHash table;

    GH_create(&table, 0, NULL);

    GH_print_struct(&table);

    GH_put(&table, 1, 2);
    GH_put(&table, INT_MIN, INT_MAX);

    // GH_print_struct(&table);
    GH_print_data(&table);

    // RNG
    srand((unsigned int)time(NULL));
    int32_t random = rand();

    for(long i=0; i<5000; i++){
        void* success = GH_put(&table, rand(), i);
        GH_delete_key(&table, rand());
    }

    GH_print_struct(&table);
    GH_print_data_debug(&table);

    printf("\n%d", SHRT_MIN);

    return 0;
}


//https://www.reddit.com/r/C_Programming/comments/ievcev/what_is_the_most_effective_set_of_gcc_warning/
//https://www.shital.com/p/how-to-enable-and-use-gcc-strict-mode-compilation/

//https://gist.github.com/badboy/6267743
//https://burtleburtle.net/bob/hash/integer.html
//https://jfdube.wordpress.com/2011/10/12/hashing-strings-and-pointers-avoiding-common-pitfalls/


#undef GALBA_HASH_DEFAULT_CAPACITY_MAGNITUDE

#undef GALBA_HASH_EMPTY
#undef GALBA_HASH_DELETED
#undef GALBA_HASH_OCCUPIED
#undef GALBA_HASH_MAX_LOAD