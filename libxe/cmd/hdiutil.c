//
//  cmd_hdiutil.c
//  xe
//
//  Created by admin on 11/26/21.
//

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/errno.h>

#include "cmd/hdiutil.h"


int xe_cmd_hdiutil_attach_nomount(const char* image, char* dev_out, size_t dev_size) {
    char buffer[1024];
    if (snprintf(buffer, sizeof(buffer), "hdiutil attach -nomount \"%s\"", image) >= sizeof(buffer)) {
        return E2BIG;
    }

    FILE* stream = popen(buffer, "r");
    if (!stream) {
        return errno;
    }

    const char* expected_suffix = "Microsoft Basic Data";
    const int expected_suffix_len = (int)strlen(expected_suffix);

    while (1) {
        size_t char_count;
        char* line = fgetln(stream, &char_count);

        if (!char_count) {
            break;
        }

        while (line[char_count - 1] == '\n' || line[char_count - 1] == '\r' || line[char_count - 1] == ' ' || line[char_count - 1] == '\t') {
            char_count--;
        }

        if (char_count <= expected_suffix_len) {
            continue;
        }

        if (memcmp(&line[char_count - expected_suffix_len], expected_suffix, expected_suffix_len)) {
            continue;
        }

        for (int i=0; i<(char_count - sizeof(expected_suffix) + 1); i++) {
            if (i >= dev_size) {
                return ENOBUFS;
            }

            if (line[i] == ' ') {
                dev_out[i] = '\0';
                return 0;
            } else {
                dev_out[i] = line[i];
            }
        }
    }

    return ENOTRECOVERABLE;
}


int xe_cmd_hdiutil_detach(const char* dev_path) {
    char buffer[PATH_MAX];
    
    if (snprintf(buffer, sizeof(buffer), "hdiutil detach \"%s\"", dev_path) >= sizeof(buffer)) {
        return E2BIG;
    }
    
    return system(buffer);
}
