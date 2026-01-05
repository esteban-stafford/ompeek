#ifndef OMPEEK_H
#define OMPEEK_H

#ifdef __cplusplus
extern "C" {
#endif

/* Weak reference to a function that may exist in an OMPeek tool */
extern void ompeek_burst_set_id(int,int) __attribute__((weak));
extern void ompeek_burst_get_id(int*,int*) __attribute__((weak));

static inline void ompeek_set_id(int id, int level) {
  if (ompeek_burst_set_id)
    ompeek_burst_set_id(id, level);
}

static inline void ompeek_get_id(int *id, int *level) {
  if (ompeek_burst_get_id)
    ompeek_burst_get_id(id, level);
  else {
    *id = -1;
    *level = -1;
  }
}

#ifdef __cplusplus
}
#endif

#endif
