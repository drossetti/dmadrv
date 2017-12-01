#if defined(__powerpc__)

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

static uint32_t old_regs[CHIP_COUNT][NPU_STACK_COUNT][NPU_NTL_COUNT][NPU_CONFIG_COUNT];

#define SCOM_WRITE(CHID,REGBASE,STKID,NTLID,VAL32) do {                 \
                int rc;                                                 \
                rc = opal_xscom_write(chip_vals[CHID], REGBASE|(STKID*NPU_STACK_FACTOR)|(NTLID*NPU_NTL_FACTOR), VAL32); \
                if(rc) {                                                \
                        printk("scom write error chip:%d reg:%lx rc:%d\n", \
                               chip_vals[chip_indx],                    \
                               REGBASE|(STKID*NPU_STACK_FACTOR)|(NTLID*NPU_NTL_FACTOR), \
                               rc);                                     \
                }                                                       \
        }

#define SCOM_READ(CHID,REGBASE,STKID,NTLID,PVAR32) do {                 \
                int rc;                                                 \
                rc = opal_xscom_read(chip_vals[CHID], REGBASE|(STKID*NPU_STACK_FACTOR)|(NTLID*NPU_NTL_FACTOR), PVAR32); \
                if(rc) {                                                \
                        printk("scom read error chip:%d reg:%lx rc:%d\n", \
                               chip_vals[chip_indx],                    \
                               REGBASE|(STKID*NPU_STACK_FACTOR)|(NTLID*NPU_NTL_FACTOR), \
                               rc);                                     \
                }                                                       \
        }

/*
 * UPDATE relaxed order config 0 and 1 register. When set to the config vals above, enable relaxed ordering from all PECs
 *        NEVER EVER do what is done here, scom writes should only be done for bringup, initialization and debug.
 *        Even if you want to do this, the right way is to go through the device tree, but hopefully this helps with testing.
 */
void enable_relaxed(void)
{
        int rc, chip_indx, stck_indx,ntl_indx;

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

}

void disable_relaxed(void)
{
        int rc, chip_indx, stck_indx,ntl_indx;

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

}

#else

void enable_relaxed(void)
{
}

void disable_relaxed(void)
{
}

#endif
