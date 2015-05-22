#ifndef __VDHTAPI_INC_H__
#define __VDHTAPI_INC_H__

#include <stdio.h>

#define vassert(exp) do { \
	if (!exp) {\
            printf("{assert} [%s:%d]\n", __FUNCTION__, __LINE__);\
            *(int*)0 = 0; \
        } \
    } while(0)

#ifndef uint8_t
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef signed   short int16_t;
typedef unsigned int   uint32_t;
typedef signed int     int32_t;
#endif

static
inline uint8_t get_uint8(void* addr)
{
    return *(uint8_t*)addr;
}

static
inline void set_uint8(void* addr, uint8_t val)
{
    *(uint8_t*)addr = val;
    return ;
}

static
inline int16_t get_int16(void* addr)
{
    return *(int16_t*)addr;
}

static
inline void set_int16(void* addr, int16_t val)
{
    *(int16_t*)addr = val;
    return ;
}

static
inline uint16_t get_uint16(void* addr)
{
    return *(uint16_t*)addr;
}

static
inline void set_uint16(void* addr, uint16_t val)
{
    *(uint16_t*)addr = val;
    return ;
}

static
inline int32_t get_int32(void* addr)
{
    return *(int32_t*)addr;
}

static
inline void set_int32(void* addr, int32_t val)
{
    *(int32_t*)addr = val;
    return ;
};

static
inline uint32_t get_uint32(void* addr)
{
    return *(uint32_t*)addr;
}

static
inline void set_uint32(void* addr, uint32_t val)
{
    *(uint32_t*)addr = val;
    return ;
}

static
inline char* offset_addr(char* addr, int bytes)
{
    return addr + bytes;
}

static
inline char* unoff_addr(char* addr, int bytes)
{
    return addr - bytes;
}

#endif

