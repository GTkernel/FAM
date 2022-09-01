#ifndef __PROJ_CLIENT_RUNTIME_H__
#define __PROJ_CLIENT_RUNTIME_H__

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <rdma/rdma_cma.h>
#include <boost/program_options.hpp> //For vm

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "../src/graph_types.hpp" //Could probably just forward declare struct application
#include "../src/stats.hpp"

void run_client(boost::program_options::variables_map& vm);

struct client_context
{
    famgraph::FG_stats stats;
    
    struct ibv_pd *pd;
    
    struct message *tx_msg;
    struct ibv_mr *tx_msg_mr;

    struct message *rx_msg;
    struct ibv_mr *rx_msg_mr;

    struct ibv_mr *heap_mr;

    uint64_t peer_addr;
    uint32_t peer_rkey;

    std::string index_file;
    std::string kernel;
    std::string ofile;

    struct addrinfo *addr;
    rdma_cm_id* base_id;
    std::vector<rdma_cm_id*> cm_ids;
    unsigned long conns_established {0};
    unsigned long const connections;
    bool const print_vtable;
    boost::program_options::variables_map * const vm;
    
    std::thread app_thread;
    std::vector<std::thread> comm_threads;
    
    std::unique_ptr<famgraph::application> app;
    uint64_t num_edges{0};

    client_context(std::string const& t_file, unsigned long const t_num_conns, std::string const& t_kernel,
                   std::string const& t_ofile, bool const t_print_vtable, boost::program_options::variables_map * const t_vm)
        :index_file(t_file), kernel(t_kernel), ofile(t_ofile), cm_ids(t_num_conns), connections(t_num_conns), print_vtable(t_print_vtable), vm(t_vm) {}

    void finish_application();
    
    client_context & operator=(const client_context&) = delete;
    client_context(const client_context&) = delete;

};

#endif // __PROJ_CLIENT_RUNTIME_H__
