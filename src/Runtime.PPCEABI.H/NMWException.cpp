#include "Runtime.PPCEABI.H/NMWException.h"
#include <stddef.h>

#pragma exceptions on

#define CTORCALL_COMPLETE(ctor, objptr) (((void (*)(void*, int))ctor)(objptr, 1))

class __partial_array_destructor {
private:
    void* p;
    size_t size;
    size_t n;
    void* dtor;

public:
    size_t i;

    __partial_array_destructor(void* array, size_t elem_size, size_t Nelem, void* destructor) {
        p = array;
        size = elem_size;
        n = Nelem;
        dtor = destructor;
        i = 0;
    }

    ~__partial_array_destructor() {
        char* ptr;
        if (i < n && dtor) {
            for (ptr = (char*)p + size * i; i > 0; i--) {
                ptr -= size;
                DTORCALL(dtor, ptr);
            }
        }
    }
};

extern "C" void* __construct_new_array(void* block, void* ctor, void* dtor, size_t size, size_t n) {
    char* ptr;

    if ((ptr = (char*)block) != 0L) {
        *(size_t*)ptr = size;
        *(size_t*)(ptr + sizeof(size_t)) = n;
        ptr += 16;

        if (ctor) {
            __partial_array_destructor pad(ptr, size, n, dtor);
            char* p;

            for (p = ptr; pad.i < n; pad.i++, p += size) {
                CTORCALL_COMPLETE(ctor, p);
            }
        }
    }
    return ptr;
}

extern "C" void __construct_array(void* ptr, void* ctor, void* dtor, size_t size, size_t n) {
    __partial_array_destructor pad(ptr, size, n, dtor);
    char* p;

    for (p = (char*)ptr; pad.i < n; pad.i++, p += size) {
        CTORCALL_COMPLETE(ctor, p);
    }
}

extern "C" void __destroy_arr(void* block, void* dtor, size_t size, size_t n) {
    char* p;

    for (p = (char*)block + size * n; n > 0; n--) {
        p -= size;
        DTORCALL(dtor, p);
    }
}

extern "C" void __destroy_new_array(void* block, void* dtor) {
    if (block) {
        if (dtor) {
            size_t i, objects, objectsize;
            char* p;

            objectsize = *(size_t*)((char*)block - 16);
            objects = *(size_t*)((char*)block - 16 + sizeof(size_t));
            p = (char*)block + objectsize * objects;

            for (i = 0; i < objects; i++) {
                p -= objectsize;
                DTORCALL(dtor, p);
            }
        }
        ::operator delete[]((char*)block - 16);
    }
}
