#pragma once

#define __user

#define NULL		((void *)0)

#define ENOMEM		12
#define EFAULT		14
#define EINVAL		22

#define UINT_MAX	(~0U)

#define GFP_KERNEL	(0x10u | 0x40u | 0x80u)

typedef unsigned short u16;
typedef unsigned short uint16_t;

typedef unsigned int u32;
typedef unsigned int uint32_t;

typedef unsigned long size_t;

typedef unsigned gfp_t;

void *kzalloc(size_t size, gfp_t flags);
void kfree(void *);
long copy_from_user(void *to, const void __user * from, unsigned long n);

void *vmalloc(unsigned long size);
void vfree(const void *addr);
