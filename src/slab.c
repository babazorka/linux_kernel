#include "../h/slab.h"
#include "../h/Strukture.h"

typedef struct kmem_cache_s kmem_cache_t;

#define BLOCK_SIZE (4096)
#define CACHE_L1_LINE_SIZE (64)

void kmem_init(void* space, int block_num)
{
	slab_inic(space, block_num); // static slab je inicijalizovan
}

kmem_cache_t* kmem_cache_create(const char* name, size_t size,
	void (*ctor)(void*),
	void (*dtor)(void*)) // Allocate cache
{
	lock(slab->mutex);
	Kes* kes = kes_alloc(name, size, ctor, dtor);
	unlock(slab->mutex);
	return kes;
}
void kmem_cache_destroy(kmem_cache_t* cachep) // Deallocate cache
{
	lock(slab->mutex);
	kes_free(cachep);
	unlock(slab->mutex);
}
void* kmem_cache_alloc(kmem_cache_t* cachep) // Allocate one object from cache
{
	lock(slab->mutex);
	return obj_type_alloc(cachep);
	unlock(slab->mutex);
}
void kmem_cache_free(kmem_cache_t* cachep, void* objp) // Deallocate one object from cache
{
	lock(slab->mutex);
	obj_free(cachep, objp);
	unlock(slab->mutex);
}
void* kmalloc(size_t size) // Alloacate one small memory buffer
{
	lock(slab->mutex);
	size_t t = buff_alloc(size);
	unlock(slab->mutex);
	return t;
}
void kfree(const void* objp) // Deallocate one small memory buffer
{
	lock(slab->mutex);
	buff_free(objp);
	unlock(slab->mutex);
}
int kmem_cache_shrink(kmem_cache_t* cachep) // Shrink cache
{
	lock(slab->mutex);
	Kes* kes = skupi_kes(cachep);
	return kes;
	unlock(slab->mutex);
}
void kmem_cache_info(kmem_cache_t* cachep) // Print cache info
{
	lock(slab->mutex);
	kes_info(cachep);
	unlock(slab->mutex);
}
int kmem_cache_error(kmem_cache_t* cachep) // Print error message
{
	return kes_error(cachep);
}