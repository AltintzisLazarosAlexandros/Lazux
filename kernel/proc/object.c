/*
 * proc/object.c - The kernel object pool (Phase 7b, docs/capabilities.md)
 *
 * Every thing a capability can point at (the console, the RAMDISK directory,
 * a RAMDISK file, a process) is one object_t in the static objects[] pool.
 * Capabilities live per process; objects are shared between them and freed
 * by reference count: each cap_t pointing at an object holds one reference,
 * and the last reference_drop() returns the box to OBJ_FREE.
 */

#include "object.h"

object_t objects[OBJ_MAX];

/* One more capability points at this object. */
void reference_take(object_t *object){
    object->refcount = object->refcount + 1;
}

/*
 * One capability stopped pointing at this object. The last drop frees the box.
 * Dropping a free object does nothing, so the count never goes negative.
 */
void reference_drop(object_t *object){
    if (object->refcount <= 0 ) return;
    object->refcount = object->refcount - 1;
    if(object->refcount == 0){
        object->type = OBJ_FREE;
    }
}

/*
 * object_alloc() - Claim a free box for a new object of any type.
 *
 * Returns it with refcount 1 (the caller's capability is the first
 * reference), or 0 if all OBJ_MAX boxes are in use. Type-specific fields
 * (data/size for a FILE) are filled in by the caller.
 */
object_t* object_alloc(obj_type_t type){
    for(int i = 0; i < OBJ_MAX; i++){
        if(objects[i].type == OBJ_FREE){
            objects[i].type = type;
            objects[i].refcount = 1;
            objects[i].data = 0;
            objects[i].size = 0;
            return &objects[i];
        }
    }
    return 0;
}

/*
 * get_file_object() - The one FILE object for a RAMDISK file, with a new
 * reference taken on it.
 *
 * A file already open somewhere is found and shared (one object per thing);
 * otherwise a new box is made. Returns 0 if the pool is full.
 */
object_t* get_file_object(const uint8_t *data, uint32_t size){
    /*
     * Match on data AND size: mkramdisk gives an empty file the same offset
     * as the file after it, so data alone could match the wrong file. Two
     * empty files in a row still share one box, which is harmless.
     */
    for(int i = 0; i < OBJ_MAX; i++){
        if(objects[i].type == OBJ_FILE && objects[i].data == data &&
           objects[i].size == size){
            reference_take(&objects[i]);
            return &objects[i];
        }
    }

    object_t *object = object_alloc(OBJ_FILE);
    if(object == 0) return 0;
    object->data = data;
    object->size = size;
    return object;
}
