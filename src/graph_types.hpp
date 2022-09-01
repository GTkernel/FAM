#ifndef __PROJ_GRAPH_TYPES_H__
#define __PROJ_GRAPH_TYPES_H__
// file for basic graph structs

#include <string>//for vertex table factory
#include <bitset>//remove later and replace with concurrent version
#include <algorithm>//for swap

// save diagnostic state
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"

#include <tbb/concurrent_vector.h>
#include <tbb/concurrent_queue.h>

// turn the warnings back on
#pragma GCC diagnostic pop

namespace famgraph {

// struct sparse frontier
// struct dense frontier


// struct edge_buffer {
//     uint64_t const num_edges;
//     uint32_t const* adj_array;

//     //disallow copying and moving
//     edge_buffer & operator=(const edge_buffer&) = delete;
//     edge_buffer(const edge_buffer&) = delete;

//     edge_buffer (uint64_t const t_num_out_edges)
//         : num_edges(t_num_out_edges) {
//         adj_array = new uint32_t[t_num_out_edges];
//     }

//     ~edge_buffer () {
//         delete adj_array;
//     }
// };

// basic state for any vertex table
struct vertex
{
  uint64_t edge_offset;// offset into remote edge array
  // vertex name can be computed with a subtraction from base
  // vertex outdegree can be computed by subtracting adjacent edge_offsets

  // any application specific state can be specified in a child struct
};

// struct active_vertex { //right now only has edges for this vertex
//     //A descriptor that includes a vertex # as well as a pointer
//     // to one or more edge buffers
//     uint32_t const vertex_number; //which vertex this is referring to
//     uint64_t const begin_idx;
//     struct edge_buffer my_edges; //all out edges for this vertex

//     active_vertex (uint32_t const t_vertex_number, uint64_t const t_begin_idx, uint64_t
//     const t_num_out_edges)
//         : vertex_number(t_vertex_number), begin_idx(t_begin_idx),
//         my_edges(t_num_out_edges) {}

//     //disallow copying and moving -- don't need b/c edge_buffer not copyable!
//     active_vertex & operator=(const active_vertex&) = delete;
//     active_vertex(const active_vertex&) = delete;


// }; //NOTE: This type is exclusively produced by NIC threads and consumed by workers

struct application
{
  // global application vars that are common to all applications

  uint32_t const num_vertices;
  uint64_t const num_edges;

  // int round {0};
  volatile bool should_stop{ false };

  // tbb::concurrent_vector<struct active_vertex*> frontierA;//frontier front buffer
  // --start with concrete concurrent vector, move to generic container
  // tbb::concurrent_vector<struct active_vertex*> frontierB;//frontier back buffer

  // tbb::concurrent_vector<struct active_vertex*>* front_buffer; //owned by worker
  // threads tbb::concurrent_vector<struct active_vertex*>* back_buffer;  //owned by nic
  // threads

  // tbb::concurrent_queue<active_vertex*> active_queue; //produced by app, consumed by
  // comm workers

  // // std::bitset active_vertices; //ctor needs to init this to size #verts
  // std::vector<bool> active_vertices; //ctor needs to init this to size #verts

  application(uint32_t const t_num_vertices, uint64_t const t_num_edges)
    : num_vertices(t_num_vertices), num_edges(t_num_edges)
  // active_vertices(t_num_vertices, false)
  {
    // front_buffer = &frontierA;
    // back_buffer  = &frontierB;
  }

  // explicitly ban copy, move, etc..

  // void end_iteration () {
  //     //swap front and back pointers
  //     std::swap<>(front_buffer, back_buffer);
  //     //increment round number
  //     ++round;
  //     //clear queue counter -- this is in app thread for now
  //     //clear back buffer
  //     back_buffer->clear(); //up to client to delete active_vert after use

  //     //clear active verts bitset
  //     // for (auto& bit : active_vertices){ bit = false;} //make parallel later
  //     for (uint32_t i = 0; i < active_vertices.size(); ++i){active_vertices[i] =
  //     false;}
  // }

  // bool check_round_done(uint64_t const outstanding_verts) {
  //     //read counters from each thread... or vector size and compare to # of
  //     outstanding verts return outstanding_verts == back_buffer->size();
  // }

  // template <typename V>
  // static void queue_vertex (uint32_t const v, V* const table, application* const app){
  //     uint64_t const start_range = table[v].edge_offset;
  //     uint64_t const max_exclusive = v == (app->num_vertices - 1) ? app->num_edges :
  //     table[v+1].edge_offset;

  //     app->queue_vertex_internal(v, start_range, max_exclusive);
  // }

  // atomic test and set on bit vector
  // new active_vertex
  // determine remote address using vertex_table + watch out for last vertex //maybe add a
  // fake last vert add ptr to concurrent queue ultimately the comm thread will transfer
  // ptr to back buffer
  //     void queue_vertex_internal(uint32_t const v, uint64_t const begin_idx, uint64_t
  //     const end_idx){
  //         if (active_vertices[v]){
  //             return; //vert already in back buffer
  //         } else {
  //             active_vertices[v] = true; //this needs to be atomic with the above check
  //             active_vertex* av = new active_vertex{v,begin_idx,(end_idx - begin_idx)};
  //             active_queue.push(av);//add to nic queue
  //         }
  //     }

  //     application & operator=(const application&) = delete;
  //     application(const application&) = delete;
  // };

  // template <typename V>
  // void get_vertex_table (std::string index_file_name) {

  //     uint32_t num_vertices;

  //     //do file I/O to get data
  //     //allocate table with MMAP
  //     //register memory for RDMA
  //     //use new to create array<V>
  //     //figure out deleter function situation
  //     //return unique ptr
  // }

  // //first imp... concrete class -> parametrize with vertex type later...
  // template <typename V>
  // struct vertex_table { //In memory vertex state for the application
};
}// namespace famgraph

#endif// __PROJ_GRAPH_TYPES_H__
