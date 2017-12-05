#if defined(__powerpc__)

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/compiler.h>
#include <asm/opal.h>

#define CHIP_COUNT  2
unsigned int chip_vals[CHIP_COUNT] = {0,8};
#define NPU_STACK_COUNT               3
#define NPU_STACK_FACTOR              0x200
#define NPU_NTL_COUNT                 4
#define NPU_NTL_FACTOR                0x30
#define ROCONFIG_COUNT                12
#define NPU_CONFIG_COUNT              3
#define SCOM_RO_CONFIG0_BASE          0x000000000501100Aul
#define SCOM_RO_CONFIG1_BASE          0x000000000501100Bul
#define SCOM_RO_CONFIG2_BASE          0x000000000501100Cul
#define ROCONFIG0VAL (0xC3C1F02FC7C1F02Ful)
#define ROCONFIG1VAL (0xC801F03FC901F03Ful)
#define ROCONFIG2VAL (0xFC00000400000000ul)

static int is_enabled = 0;
static __be64 old_regs[CHIP_COUNT][NPU_STACK_COUNT][NPU_NTL_COUNT][NPU_CONFIG_COUNT];

#define SCOM_WRITE(CHID,REGBASE,STKID,NTLID,VAL64) do {                 \
                int64_t rc;                                             \
                rc = opal_xscom_write(chip_vals[CHID], REGBASE|(STKID*NPU_STACK_FACTOR)|(NTLID*NPU_NTL_FACTOR), VAL64); \
                if (rc) {                                                \
                        printk("scom write error chip:%d reg:%lx rc:%lld\n", \
                               chip_vals[CHID],                    \
                               REGBASE|(STKID*NPU_STACK_FACTOR)|(NTLID*NPU_NTL_FACTOR), \
                               rc);                                     \
			err = 1;					\
			goto out;					\
                }                                                       \
  } while(0)

#define SCOM_READ(CHID,REGBASE,STKID,NTLID,PVAR64) do {                 \
                int64_t rc;                                                 \
                rc = opal_xscom_read(chip_vals[CHID], REGBASE|(STKID*NPU_STACK_FACTOR)|(NTLID*NPU_NTL_FACTOR), PVAR64); \
                if (rc) {                                                \
                        printk("scom read error chip:%d reg:%lx rc:%lld\n", \
                               chip_vals[CHID],                    \
                               REGBASE|(STKID*NPU_STACK_FACTOR)|(NTLID*NPU_NTL_FACTOR), \
                               rc);                                     \
			err = 1;					\
			goto out;					\
                }                                                       \
  } while(0)

/*
 * UPDATE relaxed order config 0 and 1 register. When set to the config vals above, enable relaxed ordering from all PECs
 *        NEVER EVER do what is done here, scom writes should only be done for bringup, initialization and debug.
 *        Even if you want to do this, the right way is to go through the device tree, but hopefully this helps with testing.
 */
void enable_relaxed(void)
{
#if 1
  int rc, rc1, chip_indx, stck_indx,ntl_indx;
  unsigned long beval,leval;

  beval=0;

  for (chip_indx=0;chip_indx<CHIP_COUNT;chip_indx++) {
    for(stck_indx=0;stck_indx<NPU_STACK_COUNT;stck_indx++) {
      for(ntl_indx=0;ntl_indx<NPU_NTL_COUNT;ntl_indx++) {
	rc = opal_xscom_write(chip_vals[chip_indx],
			      SCOM_RO_CONFIG0_BASE|(stck_indx*NPU_STACK_FACTOR)|(ntl_indx*NPU_NTL_FACTOR), ROCONFIG0VAL);
	rc1 = opal_xscom_write(chip_vals[chip_indx],
			       SCOM_RO_CONFIG1_BASE|(stck_indx*NPU_STACK_FACTOR)|(ntl_indx*NPU_NTL_FACTOR), ROCONFIG1VAL);

	if( rc || rc1)
	  printk("scom write error chip %d reg %lx %lx rc %d %d\n",
		 chip_vals[chip_indx],
		 SCOM_RO_CONFIG0_BASE|(stck_indx*NPU_STACK_FACTOR)|(ntl_indx*NPU_NTL_FACTOR),
		 SCOM_RO_CONFIG1_BASE|(stck_indx*NPU_STACK_FACTOR)|(ntl_indx*NPU_NTL_FACTOR),
		 rc, rc1);
	opal_xscom_write(chip_vals[chip_indx],
			 SCOM_RO_CONFIG2_BASE|(stck_indx*NPU_STACK_FACTOR)|(ntl_indx*NPU_NTL_FACTOR),ROCONFIG2VAL);
      }
    }
  }
#else

        int chip_indx, stck_indx,ntl_indx;
	int err = 0;

        for (chip_indx=0;chip_indx<CHIP_COUNT;chip_indx++) {
                for(stck_indx=0;stck_indx<NPU_STACK_COUNT;stck_indx++) {
                        for(ntl_indx=0;ntl_indx<NPU_NTL_COUNT;ntl_indx++) {
                                // save old values
                                SCOM_READ(chip_indx, SCOM_RO_CONFIG0_BASE, stck_indx, ntl_indx, &old_regs[chip_indx][stck_indx][ntl_indx][0]);
                                SCOM_READ(chip_indx, SCOM_RO_CONFIG1_BASE, stck_indx, ntl_indx, &old_regs[chip_indx][stck_indx][ntl_indx][1]);
                                SCOM_READ(chip_indx, SCOM_RO_CONFIG2_BASE, stck_indx, ntl_indx, &old_regs[chip_indx][stck_indx][ntl_indx][2]);

                                SCOM_WRITE(chip_indx, SCOM_RO_CONFIG0_BASE, stck_indx, ntl_indx, ROCONFIG0VAL);
                                SCOM_WRITE(chip_indx, SCOM_RO_CONFIG1_BASE, stck_indx, ntl_indx, ROCONFIG1VAL);
                                SCOM_WRITE(chip_indx, SCOM_RO_CONFIG2_BASE, stck_indx, ntl_indx, ROCONFIG2VAL);
                        }
                }
        }
 out:
	if (!err)
	  is_enabled = 1;
#endif
}

void disable_relaxed(void)
{
#if 0
        int chip_indx, stck_indx,ntl_indx;
	int err = 0;

	if (!is_enabled)
	        return;

        for (chip_indx=0;chip_indx<CHIP_COUNT;chip_indx++) {
                for(stck_indx=0;stck_indx<NPU_STACK_COUNT;stck_indx++) {
                        for(ntl_indx=0;ntl_indx<NPU_NTL_COUNT;ntl_indx++) {
                                // write old values
                                SCOM_WRITE(chip_indx, SCOM_RO_CONFIG0_BASE, stck_indx, ntl_indx, old_regs[chip_indx][stck_indx][ntl_indx][0]);
                                SCOM_WRITE(chip_indx, SCOM_RO_CONFIG1_BASE, stck_indx, ntl_indx, old_regs[chip_indx][stck_indx][ntl_indx][1]);
                                SCOM_WRITE(chip_indx, SCOM_RO_CONFIG2_BASE, stck_indx, ntl_indx, old_regs[chip_indx][stck_indx][ntl_indx][2]);
                        }
                }
        }

 out:
	return;
#endif
}

#else

void enable_relaxed(void)
{
}

void disable_relaxed(void)
{
}

#endif
