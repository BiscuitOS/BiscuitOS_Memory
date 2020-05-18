BiscuitOS Memory Manager (MMU) History Project
-------------------------------------------

![](https://gitee.com/BiscuitOS_team/PictureSet/raw/Gitee/RPI/RPI001042.png)

The Project is used module to establish and run a private MMU on
special memory. The private MMU base on legacy Linux MMU (such as
Linux 2.6.12 etc).

On special memory, the project contains ZONE_DMA/ZONE_DMA32, ZONE_NORMAL
and ZONE_HIGHMEM, as follow:

![](https://gitee.com/BiscuitOS_team/PictureSet/raw/Gitee/RPI/RPI000735.png)

For virtual memory, the project contains linear mapping area and non-linear
mapping area which contain VMALLOC area, KMAP area and FIXMAP area. as follow:

![](https://gitee.com/BiscuitOS_team/PictureSet/raw/Gitee/RPI/RPI000737.png)

#### Allocator

###### bootmem

![](https://gitee.com/BiscuitOS_team/PictureSet/raw/Gitee/RPI/RPI000775.png)

On module, the MMU contains an allocator on boot stage named "bootmem", which
used to allocate memory for system initialization.

###### Buddy

![](https://gitee.com/BiscuitOS_team/PictureSet/raw/Gitee/RPI/RPI000827.png)

On Project, the MMU contains an buddy allocator used to allocate physical
memory.

###### PCP

![](https://gitee.com/BiscuitOS_team/PictureSet/raw/Gitee/RPI/RPI000908.png)

On Project, the MMU contains an PCP allocator to allocate and free one
physical page.

###### PERCPU

![](https://gitee.com/BiscuitOS_team/PictureSet/raw/Gitee/HK/HK000236.png)

On Project, the MMU contains an PERCPU allocator used to manager PERCPU
variable.

###### SLAB

![](https://gitee.com/BiscuitOS_team/PictureSet/raw/Gitee/HK/HK000268.png)

On Project, the MMU contains an SLAB allocator to allocate and free small
memory object.

###### VMALLOC

![](https://gitee.com/BiscuitOS_team/PictureSet/raw/Gitee/HK/HK000289.png)

On project, the MMU contains an VMALLOC allocator to allocate and free
VMALLOC virtual memory or continue virtual memory.

###### KMAP

![](https://gitee.com/BiscuitOS_team/PictureSet/raw/Gitee/RPI/RPI000969.png)

On project, the MMU contains an KMAP allocator to allocate and freee
KMAP virtual memory.

###### FIXMAP

![](https://gitee.com/BiscuitOS_team/PictureSet/raw/Gitee/RPI/RPI000984.png)

On project, the MMU contains an FIXMAP allocator to allocate and free
FIXMAP virtual memory.

-----------------------------------

#### Contact me

> - Mail: BuddyZhang1 <buddy.zhang@aliyun.com>
>
> - Wechat: Zhang514981221
