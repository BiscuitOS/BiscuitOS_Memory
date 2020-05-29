#include <linux/kernel.h>
#include <linux/string.h>
#include "biscuitos/slab.h"

/**
 * kzalloc - allocate memory. The memory is set to zero.
 * @size: how many bytes of memory are required.
 * @flags: the type of memory to allocate.
 */
void *kzalloc_bs(size_t size, gfp_t_bs flags)
{
        void *ret = kmalloc_bs(size, flags);
        if (ret)
                memset(ret, 0, size);
        return ret;
}
EXPORT_SYMBOL(kzalloc_bs);

/*
 * kstrdup - allocate space for and copy an existing string
 *
 * @s: the string to duplicate
 * @gfp: the GFP mask used in the kmalloc() call when allocating memory
 */
char *kstrdup_bs(const char *s, gfp_t_bs gfp)
{
        size_t len;
        char *buf;

        if (!s)
                return NULL;

        len = strlen(s) + 1;
        buf = kmalloc_bs(len, gfp);
        if (buf)
                memcpy(buf, s, len);
        return buf;
}
EXPORT_SYMBOL(kstrdup_bs);
