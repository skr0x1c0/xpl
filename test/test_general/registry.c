//
//  main_common.c
//  test
//
//  Created by admin on 1/11/22.
//

#include <limits.h>
#include <string.h>

#include <xe/util/misc.h>
#include <xe/util/log.h>

#include "registry.h"

#include "test_lck_rw.h"
#include "test_pacda.h"


struct test_case {
    void(*test_entry)(void);
    char name[NAME_MAX];
};


const struct test_case test_cases[] = {
    { test_lck_rw, "lck_rw" },
    { test_pacda, "pacda" },
};


void registry_run_tests(const char* filter) {
    for (int i=0; i<xe_array_size(test_cases); i++) {
        const struct test_case* t = &test_cases[i];
        if (strlen(filter) == 0 || strcmp(filter, t->name) == 0) {
            xe_log_info("running test %s", t->name);
            t->test_entry();
            xe_log_info("test %s completed", t->name);
        }
    }
}
