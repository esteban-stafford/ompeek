#ifndef BURST_H
#define BURST_H

#ifdef __cplusplus
extern "C" {
#endif

/* Weak reference to a function that may exist in an OMPT tool */
extern void burst_set_id_tool(int,int) __attribute__((weak));
extern void burst_get_id_tool(int*,int*) __attribute__((weak));

static inline void burst_set_id(int id, int level) {
  if (burst_set_id_tool)
    burst_set_id_tool(id, level);
}

static inline void burst_get_id(int *id, int *level) {
  if (burst_get_id_tool)
    burst_get_id_tool(id, level);
  else {
    *id = -1;
    *level = -1;
  }
}

#ifdef __cplusplus
}
#endif

#endif
