#pragma once
#include<Windows.h>
#include <stdlib.h>

void lock(HANDLE);
void unlock(HANDLE);
HANDLE create_mutex();

