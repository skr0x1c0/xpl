# Acquring arbitary read-write locks (`lck_rw_t`)

## Introduction

Before macOS 12, it was relatively easy to acquire arbitary locks. There were several instance in kernel source code where structures have member(s) of type `lck_rw_t*`. This allowed exploits to easily acquire arbitary locks by changing the value of these pointers in their respective structs. But in macOS 12, such struct members were replaced with the value type `lck_rw_t` making the task significantly difficult.

The following section explains the steps that may be followed to acquire / release exclusive lock on an arbitary lock `target_lck_rw` of type `lck_rw_t*` with total control over when the lock will be released 


## Steps followed

### To acquire an arbitary read-write lock, `target_lck_rw`

1. Create a UDP IPv6 socket
2. Open a connection from socket to any address using `connectx` syscall
3. Change the value of pointer `socket->so_pcb->inp_pcbinfo` to `target_lck_rw - offsetof(struct inpcbinfo, ipi_lock)`
4. Create a cycle in tailq `nstat_controls` so that an infinite loop will be created in `nstat_pcb_cache` method
5. Asynchronously trigger socket disconnect using `disconnectx` syscall

### To release the acquired `target_lck_rw`

6. Restore the value of pointer `socket->so_pcb->inp_pcbinfo` to its original value
7. Create a cycle in list `necp_uuid_service_id_list` so that an infinite loop will be created in `necp_uuid_lookup_uuid_with_service_id_locked` method
8. Break the cycle in tailq `nstat_controls` so that loop in `nstat_pcb_cache` stops
9. Wait for 1 second so that the program reaches `necp_uuid_lookup_uuid_with_service_id_locked` method
10. Change the value of pointer `socket->so_pcb->inp_pcbinfo` to `target_lck_rw - offsetof(struct inpcbinfo, ipi_lock)`
11. Break the cycle in `necp_uuid_service_id_list` so that loop in `necp_uuid_lookup_uuid_with_service_id_locked` stops
12. Wait till the asynchornously triggered `disconnectx` method in step 5 returns success
13. Restore the value of pointer `socket->so_pcb->inp_pcbinfo` back to its original value and close the socket


## Working

When a UDP IPv6 socket is disconnected, the method `udp6_disconnect` defined in `udp6_usrreq.c` is triggered. 

``` c
static int
udp6_disconnect(struct socket *so)
{
    struct inpcb *inp;
    inp = sotoinpcb(so);  // so->so_pcb
    ...
    in6_pcbdisconnect(inp);
    ...
}
```

The `udp6_disconnect` method triggers method `in6_pcbdisconnect` with argument `so->so_pcb` (of type `struct inpcb`)

``` c
void
in6_pcbdisconnect(struct inpcb *inp)
{
    struct socket *so = inp->inp_socket;

#if CONTENT_FILTER
    if (so) {
        so->so_state_change_cnt++;
    }
#endif

    if (!lck_rw_try_lock_exclusive(&inp->inp_pcbinfo.ipi_lock)) {
        /* lock inversion issue, mostly with udp multicast packets */
        socket_unlock(so, 0);
        lck_rw_lock_exclusive(&inp->inp_pcbinfo.ipi_lock);
        socket_lock(so, 0);
    }
    if (nstat_collect && SOCK_PROTO(so) == IPPROTO_UDP) {
        nstat_pcb_cache(inp);
    }
    ...
    in_pcbrehash(inp);
    lck_rw_done(&inp->inp_pcbinfo.ipi_lock);
    ...
}
```

This method acquires `so->so_pcb->inp_pcbinfo.ipi_lock` and calls methods `nstat_pcb_cache` and `in_pcbrehash` before releasing the lock. Inorder to make this method acquire an arbitary lock `target_lck_rw`, we can set the value of `inp_pcbinfo` as follows:

``` c
so->so_pcb->inp_pcbinfo = target_lck_rw - offsetof(struct inpcbinfo, ipi_lock);
```

Note that since `so->so_pcb->inp_pcbinfo.ipi_lock` is a value type and not a reference type of `lck_rw_t`, we can't set the address of lock directly. In this case we are setting the value of pointer `so->so_pcb->inp_pcbinfo` such that `&so->so_pcb->inp_pcbinfo.ipi_lock` is equal to `target_lck_rw`. This means that all other members of `so->so_pcb->inp_pcbinfo` (type `struct inpcbinfo*`) are likely invalid. So we need to be careful when other members of `so->so_pcb->inp_pcbinfo` are used.

To control when the `target_lck_rw` is released, we can make use of `nstat_pcb_cache` method which is called between lock acquire and release. The method `nstat_pcb_cache` is defined as follows:

``` c
__private_extern__ void
nstat_pcb_cache(struct inpcb *inp)
{
    ...
    // NOTE: This loop can be made infinite by creating a cycle in `nstat_controls` tailq.
    //       A cycle can be created by setting `last_element->ncs_next = first_element`
    for (state = nstat_controls; state; state = state->ncs_next) {
        lck_mtx_lock(&state->ncs_mtx);
        ...
    }
    ...
}
```

As noted above we can create an infinite loop in `nstat_pcb_cache` by creating a cycle in `nstat_controls` tailq. The infinite loop can be easily stopped by breaking the cycle in tailq. This allows us to have total control over when the `target_lck_rw` lock will be released. 

But there is a problem, the method `in_pcbrehash` uses members `ipi_hashmask` and `ipi_hash_base` of `so->so_pcb->inp_pcbinfo` for computing the variable `head`.

``` c
void
in_pcbrehash(struct inpcb *inp)
{
    struct inpcbhead *head;
    u_int32_t hashkey_faddr;

    if (inp->inp_vflag & INP_IPV6) {
        hashkey_faddr = inp->in6p_faddr.s6_addr32[3] /* XXX */;
    } else {
        hashkey_faddr = inp->inp_faddr.s_addr;
    }

    // NOTE: the field `ipi_hashmask` is read from `inp_pcbinfo` and is used
    //       for computing `inp->inp_hash_element`
    inp->inp_hash_element = INP_PCBHASH(hashkey_faddr, inp->inp_lport,
        inp->inp_fport, inp->inp_pcbinfo->ipi_hashmask);
    
    // NOTE: the field `ipi_hash_base` is read from `inp_pcbinfo`. The value
    //       of variable head is set as the pointer of the element of array 
    //       `inp_pcbinfo->ipi_hash_base` at index `inp->inp_hash_element`.
    head = &inp->inp_pcbinfo->ipi_hashbase[inp->inp_hash_element];

    if (inp->inp_flags2 & INP2_INHASHLIST) {
        LIST_REMOVE(inp, inp_hash);
        inp->inp_flags2 &= ~INP2_INHASHLIST;
    }

    VERIFY(!(inp->inp_flags2 & INP2_INHASHLIST));
    
    // NOTE: `LIST_INSERT_HEAD` macro will call `LIST_CHECK_HEAD` macro. Since
    //        variable `head` is not a valid list head, this would panic
    LIST_INSERT_HEAD(head, inp, inp_hash);
    inp->inp_flags2 |= INP2_INHASHLIST;

#if NECP
    // This call catches updates to the remote addresses
    inp_update_necp_policy(inp, NULL, NULL, 0);
#endif /* NECP */
}
```

Since both `ipi_hashmask` and `ipi_hash_base` are likely to be invalid, the variable `head` is also likely not a valid list head. The variable `head` is checked using `LIST_CHECK_HEAD` which will panic when it is not valid. To avoid the panic, we need to restore the pointer `so->so_pcb->inp_pcbinfo` before stopping the infinite loop in `nstat_pcb_cache`. 

We also have to change the pointer back to `target_lck_rw - offsetof(struct inpcbinfo, ipi_lock)` before the lock is released by `in6_pcbdisconnect`. This can be done by creating another infinite loop in `necp_uuid_lookup_uuid_with_service_id_locked`. At the end of `in_pcbrehash`, the method `inp_update_necp_policy` method is called, which would indirectly call the method `necp_uuid_lookup_uuid_with_service_id_locked`. The call stack is as follows

``` c
necp_uuid_lookup_uuid_with_service_id_locked
necp_socket_find_policy_match
inp_update_necp_policy
in_pcbrehash
```

The method `necp_uuid_lookup_uuid_with_service_id_locked` is defined as follows :
``` c
static struct necp_uuid_id_mapping *
necp_uuid_lookup_uuid_with_service_id_locked(u_int32_t local_id)
{
    struct necp_uuid_id_mapping *searchentry = NULL;
    struct necp_uuid_id_mapping *foundentry = NULL;

    if (local_id == NECP_NULL_SERVICE_ID) {
        return necp_uuid_get_null_service_id_mapping();
    }

    // NOTE: This loop can be made infinite by creating a cycle in `necp_uuid_service_id_list`.
    //       We also have to make sure the if condition `searchentry->id == local_id` never  
    //       become `TRUE` so that the loop will not break 
    LIST_FOREACH(searchentry, &necp_uuid_service_id_list, chain) {
        if (searchentry->id == local_id) {
            foundentry = searchentry;
            break;
        }
    }

    return foundentry;
}
```

As noted above, a cycle in `necp_uuid_service_id_list` will make the `LIST_FOREACH` loop in `necp_uuid_lookup_uuid_with_service_id_locked` infinite. This can be used for changing the value of pointer `so->so_pcb->inp_pcbinfo` to `target_lck_rw - offsetof(struct inpcbinfo, ipi_lock)` before the lock is released

