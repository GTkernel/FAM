#ifndef __PROJ_GRAPH_KERNEL_H__
#define __PROJ_GRAPH_KERNEL_H__

#include <utility>
#include <memory>
#include <client_runtime.hpp>
#include "graph_types.hpp"//TODO: move defs to here and delete this file.
#include "vertex_table.hpp"
#include "bitmap.hpp"
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

namespace famgraph {

enum class Buffering { SINGLE = 1, DOUBLE = 2 };

template<typename V> struct Generic_ctx
{
  struct client_context *const context;
  uint32_t const num_vertices;
  uint64_t const num_edges;
  std::pair<std::unique_ptr<famgraph::vertex, famgraph::mmap_deleter>,
    std::unique_ptr<V, famgraph::mmap_deleter>>
    p;
  unsigned long const num_workers;
  uint32_t const max_out_degree;
  uint32_t const edge_buf_size;
  std::unique_ptr<unsigned int, famgraph::RDMA_mmap_deleter> RDMA_window;
  famgraph::Bitmap frontierA;
  famgraph::Bitmap frontierB;

  Generic_ctx(struct client_context &ctx, Buffering const b)
    : context{ &ctx }, num_vertices{ famgraph::get_num_verts(ctx.index_file) },
      num_edges{ ctx.num_edges }, p{ famgraph::get_vertex_table<V>(ctx.index_file,
                                    num_vertices,
                                    ctx.vm->count("hp") ? true : false) },
      num_workers{ (*ctx.vm)["threads"].as<unsigned long>() },
      max_out_degree{
        famgraph::get_max_out_degree(p.first.get(), num_vertices, num_edges)
      },
      edge_buf_size{ (*ctx.vm)["edgewindow"].as<uint32_t>() * max_out_degree },
      RDMA_window{ famgraph::RDMA_mmap_unique<uint32_t>(
        edge_buf_size * num_workers * static_cast<uint32_t>(b),
        ctx.pd,
        ctx.vm->count("HP")) },
      frontierA{ num_vertices }, frontierB{ num_vertices }
  {
    ctx.heap_mr = this->RDMA_window.get_deleter().mr;
    // bool const use_HP = ctx.vm->count("hp") ? true : false;
    // this->index = std::move(p.first);
    // this->vertex_table = std::move(p.second);
    // auto const window_factor = (*ctx.vm)["edgewindow"].as<uint32_t>();
    // auto const edge_buf_size = max_out_degree * window_factor;
    // this->RDMA_window = famgraph::RDMA_mmap_unique<uint32_t>(this->edge_buf_size *
    // num_workers * static_cast<uint32_t>(b), &ctx); //TODO: convert to ref
  }
};

template<class KERNEL> void run_kernel(struct client_context &ctx)
{
  auto kernel = KERNEL{ ctx };
  tbb::global_control c(
    tbb::global_control::max_allowed_parallelism, kernel.c.num_workers);
  tbb::tick_count t0 = tbb::tick_count::now();
  kernel();
  tbb::tick_count t1 = tbb::tick_count::now();
  BOOST_LOG_TRIVIAL(info) << "Running Time(s): " << (t1 - t0).seconds();
  kernel.print_result();
  ctx.finish_application();
}
}// namespace famgraph
#endif// __PROJ_GRAPH_KERNEL_H__
