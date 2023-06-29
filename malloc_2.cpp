#include <stddef.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

struct MallocMetadata {
    unsigned long* addr;
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};

static MallocMetadata* head = nullptr;
static MallocMetadata* tail = nullptr;

/**
 * @brief Searches for a free block with at least ‘size’ bytes or allocates (sbrk()) one if none are
            found.
 * 
 * @param size 
 * @return void* 
 *          Success – returns pointer to the first byte in the allocated block (excluding the meta-data of
                        course)
            ii. Failure –
            a. If size is 0 returns NULL.
            b. If ‘size’ is more than 10^8, return NULL.
            c. If sbrk fails in allocating the needed space, return NULL. 

 */
void* smalloc(size_t size) {
    if(size == 0 || size > pow(10, 8)) {
        return NULL;
    }
    if(head == nullptr) {
        void* ptr = sbrk(size);
        if(ptr == (void*)-1) {
            return nullptr;
        }
        head = (MallocMetadata*)ptr;
        head->addr = (unsigned long*)ptr;
        head->size = size;
        head->is_free = false;
        head->next = nullptr;
        head->prev = nullptr;
        tail = head;
        return ptr;
    }
    else {
        MallocMetadata* curr = head;
        while(curr != nullptr) {
            if(curr->is_free && curr->size >= size) {
                curr->is_free = false;
                return curr->addr;
            }
            curr = curr->next;
        }
        void* ptr = sbrk(size);
        if(ptr == (void*)-1) {
            return nullptr;
        }
        MallocMetadata* new_block = (MallocMetadata*)ptr;
        new_block->addr = (unsigned long*)ptr;
        new_block->size = size;
        new_block->is_free = false;
        new_block->next = nullptr;
        new_block->prev = tail;
        tail->next = new_block;
        tail = new_block;
        return ptr;
    }
}

/**
 * @brief Searches for a free block of at least ‘num’ elements, each ‘size’ bytes that are all set to 0
            or allocates if none are found. In other words, find/allocate size * num bytes and set all
            bytes to 0.
 * 
 * @param num 
 * @param size 
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
    if(ptr == nullptr) {
        return nullptr;
    }
    memset(ptr, 0, num * size);
    return ptr;
}

/**
 * @brief Releases the usage of the block that starts with the pointer ‘p’.
 * 
 * @param p 
 * @return void* 
 *          If ‘p’ is NULL or already released, simply returns.
            Presume that all pointers ‘p’ truly points to the beginning of an allocated block.
 */
void* sfree(void* p) {
    if(p == nullptr) {
        return nullptr;
    }
    MallocMetadata* curr = head;
    while(curr != nullptr) {
        if(curr->addr == p) {
            curr->is_free = true;
            return nullptr;
        }
        curr = curr->next;
    }
    return nullptr;
}

/**
 * @brief If ‘size’ is smaller than or equal to the current block’s size, reuses the same block.
            Otherwise, finds/allocates ‘size’ bytes for a new space, copies content of oldp into the
            new allocated space and frees the oldp.

 * 
 * @param oldp 
 * @param size 
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
    MallocMetadata* curr = head;
    while(curr != nullptr) {
        if(curr->addr == oldp) {
            if(curr->size >= size) { //reuse same block
                return oldp;
            }
            else {
                void* ptr = smalloc(size);
                if(ptr == nullptr) {
                    return nullptr;
                }
                memmove(ptr, oldp, curr->size);
                return ptr; //edge case - oldp isn't a valid address
            }
        }
        curr = curr->next;
    }
    return nullptr;
}

size_t _num_free_blocks() {
    size_t count = 0;
    MallocMetadata* curr = head;
    while(curr != nullptr) {
        if(curr->is_free) {
            count++;
        }
        curr = curr->next;
    }
    return count;
}

size_t _num_free_bytes() {
    size_t count = 0;
    MallocMetadata* curr = head;
    while(curr != nullptr) {
        if(curr->is_free) {
            count += curr->size;
        }
        curr = curr->next;
    }
    return count;
}

size_t _num_allocated_blocks() {
    size_t count = 0;
    MallocMetadata* curr = head;
    while(curr != nullptr) {
        count++;
        curr = curr->next;
    }
    return count;
}

size_t _num_allocated_bytes() {
    size_t count = 0;
    MallocMetadata* curr = head;
    while(curr != nullptr) {
        count += curr->size;
        curr = curr->next;
    }
    return count;
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * sizeof(MallocMetadata);
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}