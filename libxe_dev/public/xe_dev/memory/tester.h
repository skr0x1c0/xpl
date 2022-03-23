//
//  kmem_tester.h
//  kmem_msdosfs
//
//  Created by admin on 11/29/21.
//

#ifndef xe_dev_kmem_tester_h
#define xe_dev_kmem_tester_h

#include <stdio.h>

void xe_kmem_tester_run(int count, size_t min_size, size_t max_size, double* read_bps, double* write_bps);

#endif /* xe_dev_kmem_tester_h */
