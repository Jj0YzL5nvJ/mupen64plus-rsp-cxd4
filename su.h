/******************************************************************************\
* Project:  Basic MIPS R4000 Instruction Set for Scalar Unit Operations        *
* Authors:  Iconoclast                                                         *
* Release:  2015.11.30                                                         *
* License:  CC0 Public Domain Dedication                                       *
*                                                                              *
* To the extent possible under law, the author(s) have dedicated all copyright *
* and related and neighboring rights to this software to the public domain     *
* worldwide. This software is distributed without any warranty.                *
*                                                                              *
* You should have received a copy of the CC0 Public Domain Dedication along    *
* with this software.                                                          *
* If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.             *
\******************************************************************************/

#ifndef _SU_H_
#define _SU_H_

#include <stdio.h>
#include "my_types.h"
#include "rsp.h"

#define EXTERN_COMMAND_LIST_GBI
#define EXTERN_COMMAND_LIST_ABI
#define SEMAPHORE_LOCK_CORRECTIONS
#define WAIT_FOR_CPU_HOST

#if (0)
#define SP_EXECUTE_LOG
#define VU_EMULATE_SCALAR_ACCUMULATOR_READ
#endif

/*
 * Currently, the plugin system this module is written for doesn't notify us
 * of how much RDRAM is installed to the system, so we have to presume 8 MiB.
 */
#define MAX_DRAM_ADDR           0x007FFFFFul
#define MAX_DRAM_DMA_ADDR       (MAX_DRAM_ADDR & ~7)

/*
 * Interact with memory using server-side byte order (MIPS big-endian) or
 * client-side (VM host's) native byte order on a 32-bit boundary.
 *
 * Unfortunately, most op-codes are optimized to require this to be TRUE.
 */
#if (ENDIAN_M == 0)
#define USE_CLIENT_ENDIAN       FALSE
#else
#define USE_CLIENT_ENDIAN       TRUE
#endif

/*
 * Always keep this enabled for faster interpreter CPU.
 *
 * If you disable this, the branch delay slot algorithm will match the
 * documentation found in the MIPS manuals (which is not entirely accurate).
 *
 * Enabled:
 *     while (CPU_running) {
 *         PC = static_delay_slot_adjustments();
 *         switch (opcode) { ... continue; }
 * Disabled:
 *     while (CPU_running) {
 *         switch (opcode) { ... break; }
 *         PC = documented_branch_delay_slot();
 *         continue;
 */
#if 1
#define EMULATE_STATIC_PC
#endif

#if (0 != 0)
#define PROFILE_MODE    static NOINLINE
#else
#define PROFILE_MODE    static INLINE
#endif

typedef enum {
    zero = 0,
    at =  1,
#ifdef TRUE_MIPS_AND_NOT_JUST_THE_RSP_SUBSET
    v0 =  2,
    v1 =  3,

    a0 =  4,
    a1 =  5,
    a2 =  6,
    a3 =  7,

    t0 =  8,
    t1 =  9,
    t2 = 10,
    t3 = 11,
    t4 = 12,
    t5 = 13,
    t6 = 14,
    t7 = 15,
    t8 = 24,
    t9 = 25,

    s0 = 16,
    s1 = 17,
    s2 = 18,
    s3 = 19,
    s4 = 20,
    s5 = 21,
    s6 = 22,
    s7 = 23,

    k0 = 26,
    k1 = 27,

    gp = 28,
#endif
    sp = 29,
    fp = 30, /* new, official MIPS name for it:  "frame pointer" */
    ra = 31,
    S8 = fp
} GPR_specifier;

extern int CPU_running;

extern RSP_INFO RSP_INFO_NAME;
extern pu8 DRAM;
extern pu8 DMEM;
extern pu8 IMEM;

extern u8 conf[32];

/*
 * general-purpose scalar registers
 *
 * based on the MIPS instruction set architecture but without most of the
 * original register names (for example, no kernel-reserved registers)
 */
extern u32 SR[32];

#define FIT_IMEM(PC)    ((PC) & 0xFFFu & 0xFFCu)

#ifdef EMULATE_STATIC_PC
#define JUMP        goto set_branch_delay
#else
#define JUMP        break
#endif

#ifdef EMULATE_STATIC_PC
#define BASE_OFF    0x000
#else
#define BASE_OFF    0x004
#endif

#ifndef EMULATE_STATIC_PC
int stage;
#endif

extern int temp_PC;
#ifdef WAIT_FOR_CPU_HOST
extern short MFC0_count[32];
/* Keep one C0 MF status read count for each scalar register. */
#endif

/*
 * The number of times to tolerate executing `MFC0    $at, $c4`.
 * Replace $at with any register--the timeout limit is per each.
 *
 * Set to a higher value to avoid prematurely quitting the interpreter.
 * Set to a lower value for speed...you could get away with 10 sometimes.
 */
extern int MF_SP_STATUS_TIMEOUT;

#define SLOT_OFF    ((BASE_OFF) + 0x000)
#define LINK_OFF    ((BASE_OFF) + 0x004)
extern void set_PC(unsigned int address);

#if (0x7FFFFFFFul >> 037 != 0x7FFFFFFFul >> ~0U)
#define MASK_SA(sa) (sa & 037)
/* Force masking in software. */
#else
#define MASK_SA(sa) (sa)
/* Let hardware architecture do the mask for us. */
#endif

/* If primary op-code is SPECIAL (000000), we could skip ANDing the rs shift. */
#if (~0U >> 1 == ~0U) || defined(_DEBUG)
#define SPECIAL_DECODE_RS(inst)     (((inst) & 0x03E00000UL) >> 21)
#else
#define SPECIAL_DECODE_RS(inst)     ((inst) >> 21)
#endif

#define SR_B(s, i)      (*(pu8)(((pu8)(SR + (s))) + BES(i)))

                     /* (-(x & (1 << b)) | (x)) */
#define SE(x, b)        (-(x & (1 << b)) | (x & ~(~0 << b)))
#define ZE(x, b)        (+(x & (1 << b)) | (x & ~(~0 << b)))

/*
 * Since RSP vectors are stored 100% accurately as big-endian arrays for the
 * proper vector operation math to be done, LWC2 and SWC2 emulation code will
 * have to look a little different.  zilmar's method is to distort the endian
 * using an array of unions, permitting hacked byte- and halfword-precision.
 */

/*
 * Universal byte-access macro for 16*8 halfword vectors.
 * Use this macro if you are not sure whether the element is odd or even.
 */
#define VR_B(vt,element)    (*(pi8)((pi8)(VR[vt]) + MES(element)))

/*
 * Optimized byte-access macros for the vector registers.
 * Use these ONLY if you know the element is even (or odd in the second).
 */
#define VR_A(vt,element)    (*(pi8)((pi8)(VR[vt]) + element + MES(0x0)))
#define VR_U(vt,element)    (*(pi8)((pi8)(VR[vt]) + element - MES(0x0)))

/*
 * Optimized halfword-access macro for indexing eight-element vectors.
 * Use this ONLY if you know the element is even, not odd.
 *
 * If the four-bit element is odd, then there is no solution in one hit.
 */
#define VR_S(vt,element)    (*(pi16)((pi8)(VR[vt]) + element))

/*** Scalar, Coprocessor Operations (system control) ***/
#define SP_STATUS_HALT          (0x00000001ul <<  0)
#define SP_STATUS_BROKE         (0x00000001ul <<  1)
#define SP_STATUS_DMA_BUSY      (0x00000001ul <<  2)
#define SP_STATUS_DMA_FULL      (0x00000001ul <<  3)
#define SP_STATUS_IO_FULL       (0x00000001ul <<  4)
#define SP_STATUS_SSTEP         (0x00000001ul <<  5)
#define SP_STATUS_INTR_BREAK    (0x00000001ul <<  6)
#define SP_STATUS_SIG0          (0x00000001ul <<  7)
#define SP_STATUS_SIG1          (0x00000001ul <<  8)
#define SP_STATUS_SIG2          (0x00000001ul <<  9)
#define SP_STATUS_SIG3          (0x00000001ul << 10)
#define SP_STATUS_SIG4          (0x00000001ul << 11)
#define SP_STATUS_SIG5          (0x00000001ul << 12)
#define SP_STATUS_SIG6          (0x00000001ul << 13)
#define SP_STATUS_SIG7          (0x00000001ul << 14)

#define NUMBER_OF_CP0_REGISTERS         16
extern pu32 CR[NUMBER_OF_CP0_REGISTERS];

extern void SP_DMA_READ(void);
extern void SP_DMA_WRITE(void);

/*** shared states between the scalar and vector units ***/
extern u16 get_VCO(void);
extern u16 get_VCC(void);
extern u8 get_VCE(void);
extern void set_VCO(u16 VCO);
extern void set_VCC(u16 VCC);
extern void set_VCE(u8 VCE);
extern i16 cf_vce[8];

extern u16 rwR_VCE(void);
extern void rwW_VCE(u16 VCE);

extern void MFC2(unsigned int rt, unsigned int vs, unsigned int e);
extern void MTC2(unsigned int rt, unsigned int vd, unsigned int e);
extern void CFC2(unsigned int rt, unsigned int rd);
extern void CTC2(unsigned int rt, unsigned int rd);

/*** Modern pseudo-operations (not real instructions, but nice shortcuts) ***/
extern void ULW(unsigned int rd, u32 addr);
extern void USW(unsigned int rs, u32 addr);

/*
 * The scalar unit controls the primary R4000 operations implementation,
 * which inherently includes interfacing with the vector unit under COP2.
 *
 * Although no scalar unit operations are computational vector operations,
 * several of them will access machine states shared with the vector unit.
 *
 * We will need access to the vector unit's vector register file and its
 * vector control register file used mainly for vector select instructions.
 */
#include "vu/select.h"

NOINLINE extern void res_S(void);

extern void SP_CP0_MF(unsigned int rt, unsigned int rd);

/*
 * example syntax (basically the same for all LWC2/SWC2 ops):
 * LTWV    $v0[0], -64($at)
 * SBV     $v0[9], 0xFFE($0)
 */
typedef void(*mwc2_func)(
    unsigned int vt,
    unsigned int element,
    signed int offset,
    unsigned int base
);

extern mwc2_func LWC2[2 * 8*2];
extern mwc2_func SWC2[2 * 8*2];

extern void res_lsw(
    unsigned int vt,
    unsigned int element,
    signed int offset,
    unsigned int base
);

/*** Scalar, Coprocessor Operations (vector unit, scalar cache transfers) ***/
extern void LBV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void LSV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void LLV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void LDV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void SBV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void SSV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void SLV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void SDV(unsigned vt, unsigned element, signed offset, unsigned base);

/*
 * Group II vector loads and stores:
 * PV and UV (As of RCP implementation, XV and ZV are reserved opcodes.)
 */
extern void LPV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void LUV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void SPV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void SUV(unsigned vt, unsigned element, signed offset, unsigned base);

/*
 * Group III vector loads and stores:
 * HV, FV, and AV (As of RCP implementation, AV opcodes are reserved.)
 */
extern void LHV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void LFV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void SHV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void SFV(unsigned vt, unsigned element, signed offset, unsigned base);

/*
 * Group IV vector loads and stores:
 * QV and RV
 */
extern void LQV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void LRV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void SQV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void SRV(unsigned vt, unsigned element, signed offset, unsigned base);

/*
 * Group V vector loads and stores
 * TV and SWV (As of RCP implementation, LTWV opcode was undesired.)
 */
extern void LTV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void SWV(unsigned vt, unsigned element, signed offset, unsigned base);
extern void STV(unsigned vt, unsigned element, signed offset, unsigned base);

NOINLINE extern void run_task(void);

#endif
