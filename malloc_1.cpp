#include <stddef.h>
#include <unistd.h>
#include <math.h>

/** 
 * @brief Tries to allocate size bytes.
 * 
 * @param size The size of the block to allocate.
 * @return void*
*           i. Success: a pointer to the first allocated byte within the allocated block.
            ii. Failure:
                a. If size is 0 returns NULL.
                b. If size is more than 10^8, return NULL.
                c. If sbrk fails, return NULL. 
 */
void* smalloc(size_t size) {
    if(size == 0 || size > pow(10, 8)) {
        return nullptr;
    }
    void* ptr = sbrk(size);
    if(ptr == (void*)-1) {
        return nullptr;
    }
    return ptr;
}
