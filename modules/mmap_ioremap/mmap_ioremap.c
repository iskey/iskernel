#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
//#include <linux/wrapper.h>/*For mem_map_(un)reserve*/
#include <asm/io.h>/*For virt_to_phys*/
#include <linux/slab.h>/*For kmalloc and kfree*/


static int mem_start_phys= -1;
static int mem_size= 100;

//MODULE_PARM(mem_start_phys, "i");
module_param(mem_start_phys, int, 0);
MODULE_PARM_DESC(mem_start_phys, "Memory physical address.");
//MODULE_PARM(mem_size, "i");
module_param(mem_size, int, 0);
MODULE_PARM_DESC(mem_size, "Memory size to be map from mem_start_phys.");

static char *g_mem;
//Flag for memory allocate.
static int g_mem_alloc_flag = -1;

static int major;

int mmapdrv_open(struct inode *inode, struct file *file);
int mmapdrv_release(struct inode *inode, struct file *file);
int mmapdrv_mmap(struct file *file, struct vm_area_struct *vma);

static struct file_operations mmapdrv_fops={
    owner: THIS_MODULE,
    mmap: mmapdrv_mmap,
    open: mmapdrv_open,
    release: mmapdrv_release,
};

int mmapdrv_init(void)
{
    int err;
    int i;
    
    if((major = register_chrdev(0, "mmap_drv", &mmapdrv_fops))< 0)
    {
        printk(KERN_ERR "mmap_drv: unable to register character device.\n");
        return (-EIO);
    }

    printk(KERN_INFO "mmap_drv device major = %d.\n", major);
    
    //g_mem will be allocated by THIS_MODULE.
    if(mem_start_phys == -1){
        //printk("mem_start is invalid.\n");
        g_mem = __get_free_pages(GFP_KERNEL, get_order(mem_size));
        if(!g_mem){
            printk(KERN_ERR "mmapdrv_init get free pages error.\n");
            err= -ENOMEM;
            goto ERR;
        }

        mem_start_phys = virt_to_phys((void *)g_mem);
        g_mem_alloc_flag = 1;
    }
    //g_mem will be a map of a reversed memory.
    else{
        g_mem = ioremap(mem_start_phys* 1024* 1024, mem_size);
        if(!g_mem){
            printk(KERN_ERR "mmapdrv_init ioremap error.\n");
            err -EIO;
            goto ERR;
        }
    }
    
    printk(KERN_INFO "mmap_drv phys addr=0x%08x, virt addr=0x%08x\n",   \
        (unsigned long)mem_start_phys,
        (unsigned long)g_mem);

   for(i= 0; i< mem_size; i+= 4){
        g_mem[i]= 0x12;
        g_mem[i+1]= 0x34;
        g_mem[i+2]= 0x56;
        g_mem[i+3]= 0x78;
   }

   return 0;
   
ERR:
    return err;
}

void mmapdrv_cleanup(void)
{
    if(g_mem_alloc_flag== 1)
        free_pages(g_mem, get_order(mem_size));
    else
        iounmap(g_mem);

    unregister_chrdev(major, "mmapdrv");

    return;
}

int mmapdrv_open(struct inode *inode, struct file *file)
{
    //MOD_INC_USE_COUNT;
    return 0;
}

int mmapdrv_release(struct inode *inode, struct file *file)
{
    //MOD_DEC_USE_COUNT;
    return 0;
}

int mmapdrv_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long offset = vma->vm_pgoff<< PAGE_SHIFT;
    unsigned long size = vma->vm_end - vma->vm_start;

    if(size > mem_size){
        printk("size too big.\n");
        return -ENXIO;
    }

    offset = offset + mem_start_phys * 1024 * 1024;

    /*
     * we do not want to have this area swapped out, lock it.
     */
    vma->vm_flags|= VM_LOCKED;
    //use remap_page_range() in new kernel.
    if(remap_pfn_range(vma, vma->vm_start, offset, size, PAGE_SHARED))
    {
        printk(KERN_ERR "remap range failed.\n");
        return -ENXIO;
    }

    return 0;
}


module_init(mmapdrv_init);
module_exit(mmapdrv_cleanup);


MODULE_AUTHOR("ISKEY");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Example of mmap/ioremap");
