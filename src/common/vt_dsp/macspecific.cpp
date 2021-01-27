#if MAC

#include "macspecific.h"

void strupr(char *c)
{
    int i = 0;
    while (c[i])
    {
        if ((c[i] >= 'a') && (c[i] <= 'z'))
            c[i] += 'A' - 'a';
        i++;
    }
}

#endif