#include <stddef.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <sys/mman.h>
#include <stdint.h>

typedef struct MallocMetadata {
    uint32_t cookie;
    void* addr;
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
} Metadata;

uint32_t generateRandomCookie() {
    uint32_t random_number = 0;
    for (int i = 0; i < 4; ++i) {
        random_number <<= 8; // Shift previous bits
        random_number |= rand(); // OR with new random byte
    }
    return random_number;
}

static bool initialized = false;
static Metadata* orders[11] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
static Metadata* mmap_head = nullptr;
static Metadata* allocated_blocks = nullptr;
static uint32_t COOKIE = 0;

void _validate_cookie(Metadata* metadata_ptr) {
    if(metadata_ptr != nullptr) {
        if(metadata_ptr->cookie != COOKIE) {
            exit(0xdeadbeef);
        }
    }
}

void _align_program_break() {
    size_t curr_break = (size_t)sbrk(0);
    size_t aligned_addr = (curr_break + (32 * 128 * 1024 - 1)) & ~((32 * 128 * 1024) - 1);
    size_t diff = aligned_addr - curr_break;
    sbrk(diff);
}

void _add_block_to_free_list(void* metadata_ptr, int order) {
    Metadata* curr = (Metadata*)metadata_ptr;
    _validate_cookie(curr);
    if(orders[order] == nullptr) {
        orders[order] = curr;
        curr->next = nullptr;
        curr->prev = nullptr;
    }
    else {
        Metadata* last = orders[order];
        while(last->next != nullptr && last->next->addr < curr->addr) {
            _validate_cookie(last);
            last = last->next;
        }
        if(last->next == nullptr) {
            last->next = curr;
            curr->prev = last;
            curr->next = nullptr;
        }
        else {
            curr->next = last->next;
            curr->prev = last;
            last->next->prev = curr;
            last->next = curr;
        }
    }
}

void _trim_if_large_enough(void* metadata_ptr, size_t actual_size,  int order) {
    Metadata* curr = (Metadata*)metadata_ptr;
    _validate_cookie(curr);
    int curr_order = order;
    while(actual_size <= curr->size / 2) { //split
        curr_order--;
        if(curr_order < 0) {
            break;
        }
        Metadata* new_block = (Metadata*)((size_t)metadata_ptr + (size_t)(pow(2, curr_order) * 128));
        new_block->cookie = COOKIE;
        new_block->addr = (void*)((size_t)new_block + sizeof(Metadata));
        new_block->size = pow(2, curr_order) * 128;
        new_block->is_free = true;
        new_block->next = nullptr;
        new_block->prev = nullptr;
        _add_block_to_free_list((void*)new_block, curr_order);
        curr->size = pow(2, curr_order) * 128;
    }
}

void _remove_from_list(void* metadata_ptr, int order) {
    Metadata* curr = (Metadata*)metadata_ptr;
    _validate_cookie(curr);
    Metadata* prev = curr->prev;
    Metadata* next = curr->next;
    if(prev == nullptr && next == nullptr) {
        orders[order] = nullptr;
    }
    else {
        if(prev != nullptr) {
            prev->next = next;
        }
        else {
            orders[order] = next;
        }
        if(next != nullptr) {
            next->prev = prev;
        }
    }
}

void _merge_buddy_blocks(void* metadata_ptr, int order) {
    // Metadata* curr = (Metadata*)metadata_ptr;
    // _validate_cookie(curr);
    // Metadata* buddy = nullptr;
    // Metadata* last = nullptr;
    // //Metadata* freed = nullptr;
    // int curr_order = order;
    // while(curr_order < 10) {
    //     buddy = (Metadata*)((size_t)curr ^ (size_t)(pow(2, curr_order) * 128));
    //     _validate_cookie(buddy);
    //     if(buddy->is_free && buddy->size == curr->size) {
    //         if(curr->addr < buddy->addr) {
    //             curr->size = curr->size * 2;
    //             curr->next = buddy->next;
    //             if(buddy->next != nullptr) {
    //                 buddy->next->prev = curr;
    //             }
    //             if(buddy->prev != nullptr) {
    //                 buddy->prev->next = buddy->next;
    //             }
    //             else {
    //                 orders[curr_order] = buddy->next;
    //             }
    //             last = curr;
    //             // if(last != nullptr) {
    //             //     freed = last;
    //             // }
    //             curr_order++;
    //         }
    //         else {
    //             buddy->size = buddy->size * 2;
    //             buddy->next = curr->next;
    //             if(curr->next != nullptr) {
    //                 curr->next->prev = buddy;
    //             }
    //             if(curr->prev != nullptr) {
    //                 curr->prev->next = curr->next;
    //             }
    //             else {
    //                 orders[curr_order] = curr->next;
    //             }
    //             last = buddy;
    //             // if(last != nullptr) {
    //             //     freed = last;
    //             // }
    //             curr_order++;
    //         }
    //     }
    //     else {
    //         break;
    //     }
    // }
    // if(last != nullptr) {
    //     _merge_buddy_blocks((void*)last, curr_order);
    // }
    // _add_block_to_free_list((void*)curr, curr_order);

    Metadata* curr = (Metadata*)metadata_ptr;
    _validate_cookie(curr);
    int curr_order = order;
    Metadata* buddy = (Metadata*)((size_t)curr ^ (size_t)(pow(2, curr_order) * 128));
    _validate_cookie(buddy);
    Metadata* last = nullptr;
    if(curr_order >= 10) {
        return;
    }
    if(buddy == nullptr || !buddy->is_free || buddy->size != curr->size) {
        _add_block_to_free_list((void*)curr, curr_order);
        return;
    }
    _remove_from_list((void*)curr, curr_order);
    _remove_from_list((void*)buddy, curr_order);
    if(curr->addr < buddy->addr) {
        curr_order++;
        curr->size *= 2;
        last = curr;
    }
    else {
        curr_order++;
        buddy->size *= 2;
        last = buddy;
    }
    _merge_buddy_blocks((void*)last, curr_order);
}

typedef struct Initialize {
    Initialize() {
        if(initialized) {
            return;
        }
        COOKIE = generateRandomCookie();
        initialized = true;
        _align_program_break();
        void* curr_bottom = sbrk(32 * 128 * 1024); //allocate 32 * 128KB
        Metadata* curr = nullptr;
        Metadata* last = nullptr;
        last = (Metadata*)curr_bottom;
        last->cookie = COOKIE;
        last->addr = (void*)((size_t)curr_bottom + sizeof(Metadata));
        last->size = 128 * 1024 - sizeof(Metadata);
        last->is_free = true;
        last->next = nullptr;
        last->prev = nullptr;
        orders[10] = last;
        curr_bottom = (void*)((size_t)curr_bottom + 128 * 1024);
        for(int i = 1; i < 32; i++) {
            curr = (Metadata*)curr_bottom;
            curr->cookie = COOKIE;
            curr->addr = (void*)((size_t)curr_bottom + sizeof(Metadata));
            curr->size = 128 * 1024 - sizeof(Metadata);
            curr->is_free = true;
            curr->next = nullptr;
            curr->prev = last;
            last->next = curr;
            last = curr;
            curr = curr->next;
            curr_bottom = (void*)((size_t)curr_bottom + 128 * 1024);
        }
    }
} InitOrders;

int _order(size_t size) {
    if(size/128 < 1) {
        return 0;
    }
    return ceil(log2(size/128));
}

/**
 * @brief Searches for a free block with at least ‘size’ bytes or allocates (sbrk()) one if none are
            found.
 * 
 * @param size The size of the block to allocate.
 * @return void* 
 *          Success – returns pointer to the first byte in the allocated block (excluding the meta-data of
                        course)
            ii. Failure –
            a. If size is 0 returns NULL.
            b. If ‘size’ is more than 10^8, return NULL.
            c. If sbrk fails in allocating the needed space, return NULL. 

 */
void* smalloc(size_t size) {
    static InitOrders init; //initialize first 32 blocks of 128KB
    if(size == 0 || size > pow(10, 8)) {
        return NULL;
    }
    if(size + sizeof(Metadata) >= 128 * 1024) { //use mmap
        void* ptr = mmap(NULL, size + sizeof(Metadata), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if(ptr == MAP_FAILED) {
            return NULL;
        }
        Metadata* new_block = (Metadata*)ptr;
        new_block->cookie = COOKIE;
        new_block->addr = (void*)((size_t)ptr + sizeof(Metadata));
        new_block->size = size;
        new_block->is_free = false;
        new_block->next = nullptr;
        new_block->prev = nullptr;
        if(mmap_head == nullptr) {
            mmap_head = new_block;
        }
        else {
            Metadata* last = mmap_head;
            while(last->next != nullptr) {
                _validate_cookie(last);
                last = last->next;
            }
            last->next = new_block;
            new_block->prev = last;
        }
        return new_block->addr;
    }
    int order = _order(size + sizeof(Metadata));
    for(int i = order; i < 11; i++) { //find lowest order with free blocks that fits
        if(orders[i] == nullptr) {
            continue;
        }
        Metadata* curr = orders[i];
        while(curr != nullptr) { //find free block - first one should be free (might remove while later)
            _validate_cookie(curr);
            if(curr->is_free) {
                curr->is_free = false;
                Metadata* prev = curr->prev;
                Metadata* next = curr->next;
                if(prev == nullptr && next == nullptr) {
                    orders[i] = nullptr;
                }
                else {
                    if(prev != nullptr) {
                        prev->next = next;
                    }
                    else {
                        orders[i] = next;
                    }
                    if(next != nullptr) {
                        next->prev = prev;
                    }
                }
                _trim_if_large_enough((void*)curr, size + sizeof(Metadata), i);
                if(allocated_blocks == nullptr) { //add to used blocks list
                    allocated_blocks = curr;
                    curr->prev = nullptr;
                    curr->next = nullptr;
                }
                else {
                    Metadata* last = allocated_blocks;
                    while(last->next != nullptr) {
                        last = last->next;
                    }
                    last->next = curr;
                    curr->prev = last;
                }
                return curr->addr;
            }
            curr = curr->next;
        }
    }
    return nullptr;
}

/**
 * @brief Searches for a free block of at least ‘num’ elements, each ‘size’ bytes that are all set to 0
            or allocates if none are found. In other words, find/allocate size * num bytes and set all
            bytes to 0.
 * 
 * @param num The number of elements.
 * @param size The size of each element.
 * @return void* 
 *          Success - returns pointer to the first byte in the allocated block.
            ii. Failure –
                a. If size or num is 0 returns NULL.
                b. If ‘size * num’ is more than 10^8, return NULL.
                c. If sbrk fails in allocating the needed space, return NULL. 
 */
void* scalloc(size_t num, size_t size) {
    if(num == 0 || size == 0 || num * size > pow(10, 8)) {
        return nullptr;
    }
    void* ptr = smalloc(num * size);
    _validate_cookie((Metadata*)((size_t)ptr - sizeof(Metadata)));
    if(ptr == nullptr) {
        return nullptr;
    }
    memset(ptr, 0, num * size);
    if(allocated_blocks == nullptr) { //add to used blocks list
        allocated_blocks = (Metadata*)((size_t)ptr - sizeof(Metadata));
    }
    else {
        Metadata* last = allocated_blocks;
        while(last->next != nullptr) {
            _validate_cookie(last);
            last = last->next;
        }
        last->next = (Metadata*)((size_t)ptr - sizeof(Metadata));
        last->next->prev = last;
    }
    return ptr;
}

/**
 * @brief Releases the usage of the block that starts with the pointer ‘p’.
 * 
 * @param p The pointer to the block to release.
 * @return void* 
 *          If ‘p’ is NULL or already released, simply returns.
            Presume that all pointers ‘p’ truly points to the beginning of an allocated block.
 */
void* sfree(void* p) {
    if(p == nullptr) {
        return nullptr;
    }
    Metadata* curr = (Metadata*)((size_t)p - sizeof(Metadata));
    _validate_cookie(curr);
    if(curr->is_free) {
        return nullptr;
    }
    if(curr->size >= 128 * 1024) { //allocated using mmap - use munmap to free
        Metadata* prev = curr->prev;
        Metadata* next = curr->next;
        _validate_cookie(prev);
        _validate_cookie(next);
        if(munmap(curr, curr->size + sizeof(Metadata)) == -1) {
            return nullptr;
        }
        if(prev == nullptr && next == nullptr) {
            mmap_head = nullptr;
        }
        else {
            if(prev != nullptr) {
                prev->next = next;
            }
            else {
                mmap_head = next;
            }
            if(next != nullptr) {
                next->prev = prev;
            }
        }
        return nullptr;
    }
    if(!curr->is_free) {
        curr->is_free = true;
        Metadata* prev = curr->prev;
        Metadata* next = curr->next;
        _validate_cookie(prev);
        _validate_cookie(next);
        if(prev == nullptr && next == nullptr) {
            allocated_blocks = nullptr;
        }
        else {
            if(prev != nullptr) {
                prev->next = next;
            }
            else {
                allocated_blocks = next;
            }
            if(next != nullptr) {
                next->prev = prev;
            }
        }
        _merge_buddy_blocks(curr, _order(curr->size));
    }
    return nullptr;
}

/**
 * @brief If ‘size’ is smaller than or equal to the current block’s size, reuses the same block.
            Otherwise, finds/allocates ‘size’ bytes for a new space, copies content of oldp into the
            new allocated space and frees the oldp.

 * 
 * @param oldp The pointer to the block to reallocate.
 * @param size The new size of the block.
 * @return void* 
 *          i. Success –
                a. Returns pointer to the first byte in the (newly) allocated space.
                b. If ‘oldp’ is NULL, allocates space for ‘size’ bytes and returns a pointer to it.
            ii. Failure –
                a. If size is 0 returns NULL.
                b. If ‘size’ if more than 10^8, return NULL.
                c. If sbrk fails in allocating the needed space, return NULL.
                d. Do not free ‘oldp’ if srealloc() fails. 
 */
void* srealloc(void* oldp, size_t size) {
    if(size == 0 || size > pow(10, 8)) {
        return nullptr;
    }
    if(oldp == nullptr) {
        return smalloc(size);
    }
    _validate_cookie((Metadata*)((size_t)oldp - sizeof(Metadata)));
    if(size <= ((Metadata*)((size_t)oldp - sizeof(Metadata)))->size) { //reuse same block
        return oldp;
    }
    void* new_ptr = smalloc(size);
    if(new_ptr == nullptr) {
        return nullptr;
    }
    _validate_cookie((Metadata*)((size_t)new_ptr - sizeof(Metadata)));
    memmove(new_ptr, oldp, ((Metadata*)((size_t)oldp - sizeof(Metadata)))->size);
    sfree(oldp);
    return new_ptr;
}

size_t _num_free_blocks() {
    static InitOrders init; //initialize first 32 blocks of 128KB
    size_t count = 0;
    for(int i = 0; i < 11; i++) {
        Metadata* curr = orders[i];
        while(curr != nullptr) {
            _validate_cookie(curr);
            count++;
            curr = curr->next;
        }
    }
    return count;
}

size_t _num_free_bytes() {
    static InitOrders init; //initialize first 32 blocks of 128KB
    size_t count = 0;
    for(int i = 0; i < 11; i++) {
        Metadata* curr = orders[i];
        while(curr != nullptr) {
            _validate_cookie(curr);
            count += curr->size;
            curr = curr->next;
        }
    }
    return count;
}

size_t _num_allocated_blocks() {
    static InitOrders init; //initialize first 32 blocks of 128KB
    size_t count = 0;
    Metadata* allocated = allocated_blocks;
    while(allocated != nullptr) {
        _validate_cookie(allocated);
        count++;
        allocated = allocated->next;
    }
    for(int i = 0; i < 11; i++) {
        Metadata* curr = orders[i];
        while(curr != nullptr) {
            _validate_cookie(curr);
            count++;
            curr = curr->next;
        }
    }
    Metadata* mmap = mmap_head;
    while(mmap != nullptr) {
        _validate_cookie(mmap);
        count++;
        mmap = mmap->next;
    }
    return count;
}

size_t _num_allocated_bytes() {
    static InitOrders init; //initialize first 32 blocks of 128KB
    size_t count = 0;
    Metadata* allocated = allocated_blocks;
    while(allocated != nullptr) {
        _validate_cookie(allocated);
        count+= allocated->size;
        allocated = allocated->next;
    }
    for(int i = 0; i < 11; i++) {
        Metadata* curr = orders[i];
        while(curr != nullptr) {
            _validate_cookie(curr);
            count+= curr->size;
            curr = curr->next;
        }
    }
    Metadata* mmap = mmap_head;
    while(mmap != nullptr) {
        _validate_cookie(mmap);
        count+= mmap->size;
        mmap = mmap->next;
    }
    return count;
}

size_t _num_meta_data_bytes() {
    static InitOrders init; //initialize first 32 blocks of 128KB
    size_t count = _num_allocated_blocks() * sizeof(Metadata);
    return count;
}

size_t _size_meta_data() {
    static InitOrders init; //initialize first 32 blocks of 128KB
    return sizeof(Metadata);
}