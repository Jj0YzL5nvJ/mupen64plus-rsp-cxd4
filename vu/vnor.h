#include "vu.h"

INLINE void do_nor(short* VD, short* VS, short* VT)
{
    register int i;

    for (i = 0; i < N; i++)
        VACC_L[i] = ~(VS[i] | VT[i]);
    vector_copy(VD, VACC_L);
    return;
}

static void VNOR(void)
{
    const int vd = inst.R.sa;
    const int vs = inst.R.rd;

    do_nor(VR[vd], VR[vs], ST);
    return;
}
