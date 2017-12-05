#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/compiler.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/timex.h>
#include <linux/timer.h>
#include <linux/pci.h>
#include <linux/pci-dma.h>

/*

[root@dhcp-10-20-62-147 drossetti]# setpci -s 30:1:0.0 VENDOR_ID
15b3
[root@dhcp-10-20-62-147 drossetti]# setpci -s 30:1:0.0 DEVICE_ID
1013

*/

static int dbg_enabled = 1;
static unsigned long pa = 0x3e000000000;
static unsigned int venid = 0x15b3;
static unsigned int devid = 0x1013;

MODULE_AUTHOR("davide.rossetti@gmail.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("simple pci_dma_sg/single driver");
//MODULE_VERSION("1.0");
module_param(dbg_enabled, int, 0000);
MODULE_PARM_DESC(dbg_enabled, "enable debug tracing");
module_param(pa, long, 0);
MODULE_PARM_DESC(pa, "physical address");
module_param(venid, int, 0);
MODULE_PARM_DESC(venid, "PCI vendor id");
module_param(devid, int, 0);
MODULE_PARM_DESC(devid, "PCI device id");

#define DEVNAME "dmadrv"

#ifdef msg
#undef msg
#endif
#define msg(KRNLVL, FMT, ARGS...) printk(KRNLVL DEVNAME ":" "%s:%d - " FMT, __FUNCTION__, __LINE__,  ## ARGS)

void enable_relaxed(void);
void disable_relaxed(void);

//-----------------------------------------------------------------------------

struct file_operations gdrdrv_fops = {
    .owner    = THIS_MODULE,

#ifdef HAVE_UNLOCKED_IOCTL
    .unlocked_ioctl = NULL,
#else
    .ioctl    = NULL,
#endif
    .open     = NULL,
    .release  = NULL,
    .mmap     = NULL
};

//-----------------------------------------------------------------------------

static int __init dmadrv_init(void)
{
    int result = 0;

#if 0
    void *addr = (void*)pa;
    size_t size = 1024*64*2;
    int direction = PCI_DMA_BIDIRECTIONAL;
    struct pci_dev *dev = NULL, *from = NULL;

    msg(KERN_ERR, "initializing driver, dbg traces %s\n", dbg_enabled ? "enabled" : "disabled");

    // memory model check
    msg(KERN_ERR, "phys_addr=0x%016lx\n", pa);
    void *vaddr = phys_to_virt(pa);
    msg(KERN_ERR, "virt_addr(0x%016lx)=0x%p\n", pa, vaddr);
    int is_valid = virt_addr_valid(vaddr);
    msg(KERN_ERR, "virt_addr_valid(0x%p)=%d\n", vaddr, is_valid); 
    phys_addr_t paddr = virt_to_phys(vaddr);
    msg(KERN_ERR, "virt_to_phys(0x%p)=0x%016llx\n", vaddr, paddr);
    struct page *page = virt_to_page(vaddr);
    msg(KERN_ERR, "virt_to_page(0x%p)=0x%p\n", vaddr, page);
    unsigned long pfn = PHYS_PFN(pa);
    msg(KERN_ERR, "PHYS_PFN(0x%016lx)=0x%lx\n", pa, pfn);
    page = pfn_to_page(pfn);
    msg(KERN_ERR, "pfn_to_page(0x%lx)=0x%p\n", pfn, page);
    
    {
        u64 *up = (u64*)vaddr;
        msg(KERN_ERR, "accessing 64bits at %p\n", vaddr);
        udelay(100);
        u64 u = *up;
        msg(KERN_ERR, "[%p]=0x%016llx\n", vaddr, u);
    }

    do {
        void *vaddr = kmalloc(1024, GFP_KERNEL);
        if (!addr) {
            msg(KERN_ERR, "failure in kmalloc(1024)\n");
            break;
        }
        msg(KERN_ERR, "kmalloc(1024)=0x%p\n", vaddr); 
        int is_valid = virt_addr_valid(vaddr);
        msg(KERN_ERR, "virt_addr_valid(0x%p)=%d\n", vaddr, is_valid); 
        phys_addr_t paddr = virt_to_phys(vaddr);
        msg(KERN_ERR, "virt_to_phys(0x%p)=0x%016llx\n", vaddr, paddr);
        struct page *page = virt_to_page(vaddr);
        msg(KERN_ERR, "virt_to_page(0x%p)=0x%p\n", vaddr, page);

        kfree(vaddr);
    } while(0);

    msg(KERN_INFO, "seaching for Mellanox ConnectX-4 PCIe device: venid=%04x devid=%04x\n", venid, devid);
    dev = pci_get_device(venid, devid, from);
    if (!dev) {
        msg(KERN_ERR, "device not found\n");
        result = -ENODEV;
        goto out;
    }
    msg(KERN_INFO, "struct pci_dev*=%p devfn=%u vendor=%04x device=%04x dma_mask=%016llx\n", 
        dev, dev->devfn, dev->vendor, dev->device, dev->dma_mask);

    // test for pci_map_single
    {
        int ret = 0;
        dma_addr_t dma_handle = 0;
        msg(KERN_INFO, "calling pci_map_single vaddr=%p size=%zu\n", vaddr, size);
        dma_handle = pci_map_single(dev, vaddr, size, direction);
        if ((ret = pci_dma_mapping_error(dev, dma_handle))) {
            msg(KERN_ERR, "error %d in pci_map_single\n", ret);
            result = -EINVAL;
            goto out;
        }
        msg(KERN_INFO, "got dma_handle=%llx", dma_handle);
        msg(KERN_INFO, "unmapping dma handle\n");
        pci_unmap_single(dev, dma_handle, size, direction);
    }

    // test for pci_map_sg
    {
        size_t npages = size / PAGE_SIZE;
        struct sg_table sg_tbl;
        struct sg_table *sg_head = &sg_tbl;
        int nmap = 0;
        int i;
        int ret;
        struct scatterlist *sg = NULL;
        msg(KERN_INFO, "npages=%zu PAGE_SIZE=%lu but using PAGE_SIZE=%zu\n", npages, PAGE_SIZE, PAGE_SIZE);
#ifdef CONFIG_NEED_SG_DMA_LENGTH
        msg(KERN_INFO, "CONFIG_NEED_SG_DMA_LENGTH defined\n");
#endif
		ret = sg_alloc_table(sg_head, npages, GFP_KERNEL);
		if (ret) {
            msg(KERN_ERR, "error %d in sg_alloc_table\n", ret);
            result = ret;
			goto out;
		}

        phys_addr_t aligned_addr = pa & PAGE_MASK;
        unsigned long offset = pa & (PAGE_SIZE-1);
        msg(KERN_INFO, "aligned addr=0x%llx offset=%lu\n", aligned_addr, offset);
        for_each_sg(sg_head->sgl, sg, npages, i) {
            phys_addr_t page_addr = aligned_addr + i*PAGE_SIZE;
            unsigned long pfn = PHYS_PFN(page_addr);
            struct page *page = pfn_to_page(pfn);
            sg_set_page(sg, page, PAGE_SIZE, offset);
            msg(KERN_INFO, "sg[%d]=0x%llx offset=%lu page=%p size=%zu\n", i, page_addr, offset, page, PAGE_SIZE);
            offset = 0;
        }
        msg(KERN_INFO, "calling pci_map_sg npages=%zu\n", npages);
        nmap = pci_map_sg(dev, sg_head->sgl, npages, direction);
        if (nmap <= 0) {
            msg(KERN_ERR, "error %d in pci_map_sg\n", nmap);
            result = nmap;
            goto free_sg;
        }
        msg(KERN_INFO, "got nmap=%d from pci_map_sg\n", nmap);
        for_each_sg(sg_head->sgl, sg, npages, i) {
            msg(KERN_INFO, "dma_addr[%d]=%llx len:0x%x\n", i, sg_dma_address(sg), sg_dma_len(sg));
        }
        msg(KERN_INFO, "calling pci_unmap_sg npages=%zu\n", npages);
        pci_unmap_sg(dev, sg_head->sgl, npages, direction);

    free_sg:
        sg_free_table(sg_head);
    }

#if 0
    // test for dma_map_resource
    //   this case triggers BUG_ON(pfn_valid(PHYS_PFN(phys_addr))) in dma_map_resource
    {
        int ret = 0;
        dma_addr_t dma_handle = 0;
        int attrs = 0;
        msg(KERN_INFO, "calling dma_map_resource paddr=0x%lx size=%zu\n", pa, size);
        dma_handle = dma_map_resource(&dev->dev, pa, size, DMA_BIDIRECTIONAL, attrs);
        if ((ret = dma_mapping_error(&dev->dev, dma_handle))) {
            msg(KERN_ERR, "error %d in dma_map_resource\n", ret);
            result = -EINVAL;
            goto out;
        }
        msg(KERN_INFO, "got dma_handle=%llx", dma_handle);
        msg(KERN_INFO, "unmapping dma handle\n");
        dma_unmap_resource(&dev->dev, dma_handle, size, direction, attrs);
    }
#endif

 out:
    if (dev) {
        msg(KERN_INFO, "deref pci dev\n");
        pci_dev_put(dev);
    }
#endif

    enable_relaxed();

    msg(KERN_ERR, "returning result=%d\n", result);
    return result;
}

//-----------------------------------------------------------------------------

static void __exit dmadrv_cleanup(void)
{
    msg(KERN_INFO, "cleaning up driver\n");

    disable_relaxed();
}

//-----------------------------------------------------------------------------

module_init(dmadrv_init);
module_exit(dmadrv_cleanup);

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */

