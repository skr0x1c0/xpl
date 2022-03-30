
#include <limits.h>
#include <string.h>

#include <xpl/util/misc.h>
#include <xpl/util/log.h>

#include "registry.h"

#include "test_lck_rw.h"
#include "test_pacda.h"
#include "test_large_mem.h"
#include "test_zalloc.h"
#include "test_kfunc.h"
#include "test_vnode.h"
#include "test_sudo.h"


struct test_case {
    void(*test_entry)(void);
    char name[NAME_MAX];
};


const struct test_case test_cases[] = {
    { test_lck_rw, "lck_rw" },
    { test_large_mem, "large_mem" },
    { test_zalloc, "zalloc" },
    { test_pacda, "pacda" },
    { test_kfunc, "kfunc" },
    { test_vnode, "vnode" },
    { test_sudo, "sudo" }
};


void registry_run_tests(const char* filter) {
    for (int i=0; i<xpl_array_size(test_cases); i++) {
        const struct test_case* t = &test_cases[i];
        if (!filter || strlen(filter) == 0 || strcmp(filter, t->name) == 0) {
            xpl_log_info("running test %s", t->name);
            t->test_entry();
            xpl_log_info("test %s completed", t->name);
        }
    }
}
