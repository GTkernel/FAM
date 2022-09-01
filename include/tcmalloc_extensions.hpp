#ifndef __PROJ_TCMALLOC_EXTENSIONS_H__
#define __PROJ_TCMALLOC_EXTENSIONS_H__

#include <client_runtime.hpp> //for struct client_context

void init_rdma_heap(struct client_context * ctx);
#endif // __PROJ_TCMALLOC_EXTENSIONS_H__
