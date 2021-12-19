//
//  main.c
//  kmem_xepg
//
//  Created by admin on 12/18/21.
//

#include <stdio.h>

#include "smb/smb_client.h"

int main(int argc, const char * argv[]) {
    smb_client_load_kext();
    
    return 0;
}
