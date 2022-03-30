
#ifndef xpl_dev_kmem_tester_h
#define xpl_dev_kmem_tester_h

#include <stdio.h>

void xpl_kmem_tester_run(int count, size_t min_size, size_t max_size, double* read_bps, double* write_bps);

#endif /* xpl_dev_kmem_tester_h */
