#ifndef CC_H_
#define CC_H_

#include <stdint.h>
#include <stdlib.h>

typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;

typedef int8_t s8_t;
typedef int16_t s16_t;
typedef int32_t s32_t;

typedef u32_t mem_ptr_t;
//typedef int sys_prot_t;

#define PACK_STRUCT_FIELD(x) x __attribute__((packed))
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#define BYTE_ORDER LITTLE_ENDIAN

#define LWIP_PROVIDE_ERRNO 1

#define LWIP_PLATFORM_DIAG(x)
#define LWIP_PLATFORM_ASSERT(x)

#define LWIP_RAND rand

#endif /* CC_H_ */
