#pragma once

/*
 * object.h - Kernel objects for the capability system (docs/capabilities.md)
 *
 * One object_t per thing in the system (the console, the RAMDISK directory,
 * each RAMDISK file, each process), kept in a static pool of OBJ_MAX boxes
 * and freed by reference count (each capability holds one reference).
 *
 * OBJ_FREE is 0 so the zeroed pool in .bss starts out all free.
 * There is no offset here: the read position belongs to the capability
 * (cap_t), so two processes reading the same file don't move each other.
 */

#include <stdint.h>

#define OBJ_MAX 128

typedef enum { OBJ_FREE = 0, OBJ_CONSOLE, OBJ_DIR, OBJ_FILE, OBJ_PROCESS}obj_type_t;

typedef struct{
    obj_type_t type;
    int refcount;
    const uint8_t* data;
    uint32_t size;
}object_t;

object_t* object_alloc(obj_type_t type);
void reference_take(object_t* object);
void reference_drop(object_t* object);
object_t* get_file_object(const uint8_t *data, uint32_t size);

