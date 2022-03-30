
#ifndef xpl_pipe_h
#define xpl_pipe_h

#include <stdio.h>

typedef struct xpl_pipe* xpl_pipe_t;

/// Creates `xpl_pipe_t` for creating a location in kernel memory with repeatable write access
/// @param size size of memory to be allocated for pipe buffer
xpl_pipe_t xpl_pipe_create(size_t size);

/// Returns to address of memory allocated for pipe buffer
/// @param pipe `xpl_pipe_t` created using `xpl_pipe_create`
uintptr_t xpl_pipe_get_address(xpl_pipe_t pipe);

/// Write provided data to memory allocated for pipe buffer
/// @param pipe `xpl_pipe_t` created using `xpl_pipe_create`
/// @param data source data
/// @param size size of source data. should be less than size of pipe buffer
void xpl_pipe_write(xpl_pipe_t pipe, const void* data, size_t size);

/// Closes the pipe and release resources
/// @param pipe_p pointer to `xpl_pipe_t` created using `xpl_pipe_create`
void xpl_pipe_destroy(xpl_pipe_t* pipe_p);

#endif /* xpl_pipe_h */
