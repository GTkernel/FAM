#ifndef __PROJ_VERTEX_TABLE_H__
#define __PROJ_VERTEX_TABLE_H__

#include <iostream>
#include <fstream>
#include <stdexcept>

#include <string.h>
#include <assert.h>

#include <boost/log/trivial.hpp>
#include <boost/filesystem.hpp>//Remove when refactor edgelist init
#include <boost/filesystem/fstream.hpp>//Remove when refactor edgelist init

#include <client_runtime.hpp>

#include "mmap_util.hpp"

namespace famgraph {
uint32_t get_num_verts(std::string const &file);

uint32_t get_num_edges(uint32_t const v,
  famgraph::vertex const *const table,
  uint32_t const total_verts,
  uint64_t const total_edges) noexcept;

uint32_t get_max_out_degree(famgraph::vertex *const vtable,
  uint32_t const n_vert,
  uint64_t const n_edges) noexcept;

template<typename V>
auto get_vertex_table(std::string const &file,
  uint64_t const num_vertices,
  bool const use_HP)
{
  namespace fs = boost::filesystem;
  fs::path p(file);
  if (fs::exists(p) && fs::is_regular_file(p)) {
    std::ifstream input(p.c_str(), std::ios::binary);
    if (input) {
      uint64_t a;
      auto pt = mmap_unique<famgraph::vertex>(num_vertices, use_HP);
      auto pt2 = mmap_unique<V>(num_vertices, use_HP);
      auto *vp = pt.get();
      for (uint64_t i = 0; i < num_vertices; ++i) {
        input.read(reinterpret_cast<char *>(&a), sizeof(uint64_t));
        // std::cout << a << std::endl;
        vp[i].edge_offset = a;
        if (static_cast<unsigned long>(input.gcount()) != sizeof(uint64_t)) {
          throw std::runtime_error("can't read index data");
        }
      }
      return std::make_pair(std::move(pt), std::move(pt2));
    }
    throw std::runtime_error("stream open error");
  }
  throw std::runtime_error("file not found");
  // do file I/O to get data
  // allocate table with MMAP
  // register memory for RDMA
  // use new to create array<V>
  // figure out deleter function situation
  // return unique ptr
}
}// namespace famgraph

#endif// __PROJ_VERTEX_TABLE_H__
