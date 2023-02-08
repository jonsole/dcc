/*
 * operator.cpp
 *
 * Created: 26/12/2021 15:25:37
 *  Author: jonso
 */ 

#include <cstdlib>
#include <new>
#include "mem.h"

void* operator new(size_t size) noexcept
{
	return MEM_Alloc(size);
}

void operator delete(void *p, unsigned int) noexcept
{
	MEM_Free(p);
}

void* operator new[](size_t size) noexcept
{
	return operator new(size); // Same as regular new
}

void operator delete[](void *p) noexcept
{
	operator delete(p); // Same as regular delete
}

void* operator new(size_t size, std::nothrow_t) noexcept
{
	return operator new(size); // Same as regular new
}

void operator delete(void *p,  std::nothrow_t) noexcept
{
	operator delete(p); // Same as regular delete
}

void* operator new[](size_t size, std::nothrow_t) noexcept
{
	return operator new(size); // Same as regular new
}

void operator delete[](void *p,  std::nothrow_t) noexcept
{
	operator delete(p); // Same as regular delete
}