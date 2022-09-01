#ifndef __PROJ_RDMA_ARRAY_H__
#define __PROJ_RDMA_ARRAY_H__

// #include <string> //for vertex table factory

// #include <memory>
// #include <iostream>
// #include <string.h>
// #include <sys/mman.h>

// #include <infiniband/verbs.h>

// #include <boost/log/trivial.hpp>

// namespace famgraph{

//     template <typename V>
//     void get_edge_table (std::string index_file_name) { //also take connection context
//     info

//         uint32_t num_vertices;

//         //do file I/O to get data
//         //allocate table with MMAP
//         //register memory for RDMA
//         //use new to create array<V>
//         //figure out deleter function situation
//         //return unique ptr
//     }

//     constexpr auto PROT_RW = PROT_READ | PROT_WRITE;
//     constexpr auto MAP_ALLOC = MAP_PRIVATE | MAP_ANONYMOUS;
//     // constexpr auto MR_FLAGS  = ;

//     //first imp... concrete class -> parametrize with vertex type later...
//     template <typename V>
//     struct RDMA_array { //In memory vertex state for the application
//         std::size_t region_size;
//         V *arr;
//         struct ibv_mr *arr_mr;

//         RDMA_array (uint32_t t_num_verts, struct ibv_pd *pd)
//             : region_size(t_num_verts * sizeof(V)) {

//             //mmap and register memory
//             arr = static_cast<V*>(mmap(NULL, region_size, PROT_RW, MAP_ALLOC, -1, 0));
//             if (!arr){
//                 BOOST_LOG_TRIVIAL(fatal) << "mmap failed";
//             }

//             arr_mr = ibv_reg_mr(pd, arr, region_size, )
//         }
//         ~RDMA_array () {
//             if (ibv_dereg_mr(arr_mr)){
//                 BOOST_LOG_TRIVIAL(info) << "ibv_dereg failed";
//             }
//             if (munmap(arr, region_size)) {
//                 BOOST_LOG_TRIVIAL(fatal) << "vert table unmap failed!!!";
//             }
//         }

//         RDMA_array & operator=(const RDMA_array&) = delete;
//         RDMA_array(const RDMA_array&) = delete;
//     };
// }

#endif// __PROJ_RDMA_ARRAY_H__
