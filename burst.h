#ifndef BURST_H
#define BURST_H

#include <omp-tools.h>   /* needed for omp_get_tool_symbol() */

#ifdef __cplusplus
extern "C" {
#endif

static inline void burst_set_id(int id)
{
    typedef void (*burst_set_id_t)(int);
    static burst_set_id_t fn = 0;

    if (!fn) {
        void *sym = omp_get_tool_symbol("burst_set_id_tool");
        if (sym)
            fn = (burst_set_id_t)sym;
    }

    if (fn)
        fn(id);
}

#ifdef __cplusplus
}
#endif

#endif
