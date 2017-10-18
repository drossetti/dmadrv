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

#define DEVNAME "sgdrv"

#ifdef msg
#undef msg
#endif
#define msg(KRNLVL, FMT, ARGS...) printk(KRNLVL DEVNAME ":" FMT, ## ARGS)

//-----------------------------------------------------------------------------

static int gdrdrv_major = 0;

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

static int __init sgdrv_init(void)
{
    int result = 0;
    void *addr = (void*)pa;
    size_t size = 1024*64*2;
    int direction = PCI_DMA_BIDIRECTIONAL;
    struct pci_dev *dev = NULL, *from = NULL;

#if 0
    result = register_chrdev(gdrdrv_major, DEVNAME, &gdrdrv_fops);
    if (result < 0) {
        msg(KERN_ERR, "can't get major %d\n", gdrdrv_major);
        return result;
    }
    if (gdrdrv_major == 0) gdrdrv_major = result; /* dynamic */

    msg(KERN_INFO, "device registered with major number %d\n", gdrdrv_major);
#endif

    msg(KERN_ERR, "initializing driver, dbg traces %s, pa=%p\n", dbg_enabled ? "enabled" : "disabled", (void*)pa);

    msg(KERN_INFO, "seaching for venid=%04x devid=%04x\n", venid, devid);
    dev = pci_get_device(venid, devid, from);
    if (!dev) {
        msg(KERN_ERR, "device not found\n");
        result = -ENODEV;
        goto out;
    }
    msg(KERN_INFO, "pci_dev*=%p defn=%u vendor=%04x device=%04x dma_mask=%016llx\n", 
        dev, dev->devfn, dev->vendor, dev->device, dev->dma_mask);

    {
        int ret = 0;
        dma_addr_t dma_handle = 0;
        msg(KERN_INFO, "calling pci_map_single addr=%p size=%zu\n", addr, size);
        dma_handle = pci_map_single(dev, addr, size, direction);
        if (pci_dma_mapping_error(dev, dma_handle)) {
            msg(KERN_ERR, "error %d in pci_map_single\n", ret);
            result = -EINVAL;
            goto out;
        }
        msg(KERN_INFO, "got dma_handle=%llx", dma_handle);
        msg(KERN_INFO, "unmapping dma handle\n");
        pci_unmap_single(dev, dma_handle, size, direction);
    }

    {
        size_t psz = PAGE_SIZE;
        size_t npages = size / psz;
        struct sg_table sg_tbl;
        struct sg_table *sg_head = &sg_tbl;
        int nmap = 0;
        int i;
        int ret;
        struct scatterlist *sg = NULL;
        msg(KERN_INFO, "npages=%zu PAGE_SIZE=%lu but using psz=%zu\n", npages, PAGE_SIZE, psz);
#ifdef CONFIG_NEED_SG_DMA_LENGTH
        msg(KERN_INFO, "CONFIG_NEED_SG_DMA_LENGTH defined\n");
#endif
		ret = sg_alloc_table(sg_head, npages, GFP_KERNEL);
		if (ret) {
            msg(KERN_ERR, "error %d in sg_alloc_table\n", ret);
            result = ret;
			goto out;
		}

        for_each_sg(sg_head->sgl, sg, npages, i) {
	        sg_set_page(sg, addr + i*psz, psz, 0);
            msg(KERN_INFO, "sg[%d]=%p size=%zu\n", i, addr + i*psz, psz);
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

 out:
    if (dev) {
        msg(KERN_INFO, "deref pci dev\n");
        pci_dev_put(dev);
    }

    msg(KERN_ERR, "returning result=%d\n", result);
    return result;
}

//-----------------------------------------------------------------------------

static void __exit sgdrv_cleanup(void)
{
    msg(KERN_INFO, "cleaning up driver\n");

}

//-----------------------------------------------------------------------------

module_init(sgdrv_init);
module_exit(sgdrv_cleanup);

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */

