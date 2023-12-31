#include <stddef.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

typedef struct MallocMetadata {
    void* addr; 
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
} Metadata;

static Metadata* head = NULL;

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
    if(size == 0 || size > pow(10, 8)) {
        return NULL;
    }
    if(head == NULL) {
        void* ptr = sbrk(size + sizeof(Metadata));
        if(ptr == (void*)-1) {
            return NULL;
        }
        head = (Metadata*)ptr;
        head->addr = (void*)((size_t)ptr + sizeof(Metadata));
        head->size = size;
        head->is_free = false;
        head->next = NULL;
        head->prev = NULL;
        return head->addr;
    }
    else {
        Metadata* curr = head;
        Metadata* tail = head;
        while(curr != NULL) {
            if(curr->is_free && curr->size >= size) {
                curr->is_free = false;
                return curr->addr;
            }
            curr = curr->next;
            if(tail->next != NULL) {
                tail = tail->next;
            }
        }
        void* ptr = sbrk(size + sizeof(Metadata));
        if(ptr == (void*)-1) {
            return NULL;
        }
        Metadata* new_block = (Metadata*)ptr;
        new_block->addr = (void*)((size_t)ptr + sizeof(Metadata));
        new_block->size = size;
        new_block->is_free = false;
        new_block->next = NULL;
        new_block->prev = tail;
        tail->next = new_block;
        return new_block->addr;
    }
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
        return NULL;
    }
    void* ptr = smalloc(num * size);
    if(ptr == NULL) {
        return NULL;
    }
    memset(ptr, 0, num * size);
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
    if(p == NULL) {
        return NULL;
    }
    Metadata* curr = (Metadata*)((size_t)p - sizeof(Metadata));
    if(!curr->is_free) {
        curr->is_free = true;
    }
    return NULL;
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
        return NULL;
    }
    if(oldp == NULL) {
        return smalloc(size);
    }
    if(size <= ((Metadata*)((size_t)oldp - sizeof(Metadata)))->size) {
        return oldp;
    }
    void* new_ptr = smalloc(size);
    if(new_ptr == NULL) {
        return NULL;
    }
    memmove(new_ptr, oldp, ((Metadata*)((size_t)oldp - sizeof(Metadata)))->size);
    sfree(oldp);
    return new_ptr;
}

size_t _num_free_blocks() {
    size_t count = 0;
    Metadata* curr = head;
    while(curr != NULL) {
        if(curr->is_free) {
            count++;
        }
        curr = curr->next;
    }
    return count;
}

size_t _num_free_bytes() {
    size_t count = 0;
    Metadata* curr = head;
    while(curr != NULL) {
        if(curr->is_free) {
            count += curr->size;
        }
        curr = curr->next;
    }
    return count;
}

size_t _num_allocated_blocks() {
    size_t count = 0;
    Metadata* curr = head;
    while(curr != NULL) {
        count++;
        curr = curr->next;
    }
    return count;
}

size_t _num_allocated_bytes() {
    size_t count = 0;
    Metadata* curr = head;
    while(curr != NULL) {
        count += curr->size;
        curr = curr->next;
    }
    return count;
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * sizeof(Metadata);
}

size_t _size_meta_data() {
    return sizeof(Metadata);
}
