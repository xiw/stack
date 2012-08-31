#pragma once

#define __user

#define NULL		((void *)0)

#define ENOMEM		12
#define EFAULT		14
#define EINVAL		22
#define ENOPROTOOPT	92

#define UINT_MAX	(~0U)

#define GFP_KERNEL	(0x10u | 0x40u | 0x80u)

#define BUG_ON(x)	if (x) __builtin_trap();

typedef unsigned char		u8;
typedef u8			uint8_t;

typedef unsigned short		u16;
typedef u16			uint16_t;

typedef unsigned int		u32;
typedef u32			uint32_t;

typedef unsigned long long	u64;
typedef u64			uint64_t;

typedef unsigned long		size_t;
typedef long			off_t;
typedef long long		loff_t;

#define INT_LIMIT(x)		(~((x)1 << (sizeof(x)*8 - 1)))
#define OFFSET_MAX		INT_LIMIT(loff_t)
#define OFFT_OFFSET_MAX		INT_LIMIT(off_t)

typedef unsigned gfp_t;

void *kzalloc(size_t size, gfp_t flags);
void kfree(void *);

void *vmalloc(unsigned long size);
void vfree(const void *addr);

unsigned long copy_from_user(void *to, const void __user *from, unsigned long n);
