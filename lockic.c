#include"h/lockic.h"
#include"h/Strukture.h"

void lock(HANDLE handle) {
	WaitForSingleObject(handle, INFINITE);
}


void unlock(HANDLE handle) {
	ReleaseMutex(handle);
}

HANDLE create_mutex() {
	return CreateMutex(NULL, FALSE, "mutex");
}

