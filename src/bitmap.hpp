/*
  Copyright (c) 2014-2015 Xiaowei Zhu, Tsinghua University

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef BITMAP_H
#define BITMAP_H

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#include <oneapi/tbb.h>
#pragma GCC diagnostic pop

#include <assert.h>
#include <functional>//dont need anymore
#include <time.h>
#include <sys/time.h>

#include "vertex_table.hpp"//move later
#include <connection_utils.hpp>//move later
#include "communication_runtime.hpp"

#include <boost/log/trivial.hpp>//remove later

#define WORD_OFFSET(i) (i >> 6)
#define BIT_OFFSET(i) (i & 0x3f)

namespace famgraph {
class Bitmap
{
private:
  tbb::combinable<uint32_t> frontier_size;

public:
  uint32_t const size;
  tbb::blocked_range<uint32_t> const my_range;
  unsigned long *data;

  Bitmap(uint32_t const t_size) : size{ t_size }, my_range(0, WORD_OFFSET(size) + 1)
  {
    data = new unsigned long[WORD_OFFSET(size) + 1];
  }

  ~Bitmap() { delete[] data; }

  void clear() noexcept
  {
    tbb::parallel_for(my_range, [&](auto const &range) {
      for (uint32_t i = range.begin(); i < range.end(); ++i) { data[i] = 0; }
    });
    frontier_size.clear();
  }
  void set_all() noexcept
  {
    tbb::parallel_for(my_range, [&](auto const &range) {
      for (uint32_t i = range.begin(); i < range.end(); ++i) {
        data[i] = 0xffffffffffffffff;
      }
    });
    frontier_size.local() = size;
  }
  unsigned long get_bit(uint32_t const i) const noexcept
  {
    assert(i < size);
    return data[WORD_OFFSET(i)] & (1ul << BIT_OFFSET(i));
  }
  unsigned long set_bit(uint32_t const i) noexcept
  {
    assert(i < size);
    unsigned long prev = __sync_fetch_and_or(
      data + WORD_OFFSET(i), 1ul << BIT_OFFSET(i));// change sync to atomic intrinsic
    bool const was_unset = !(prev & (1ul << BIT_OFFSET(i)));
    if (was_unset) ++frontier_size.local();
    return was_unset;// true if the bit was not previously set
  }

  auto num_set() noexcept { return frontier_size.combine(std::plus<uint32_t>{}); }

  bool is_empty() noexcept { return !this->num_set(); }
};

inline void prep_wr(std::array<struct ibv_send_wr, famgraph::WR_WINDOW_SIZE> &wr_window,
  std::array<struct ibv_sge, famgraph::WR_WINDOW_SIZE> &sge_window,
  uint32_t const idx,
  struct client_context *ctx,
  void *const buffer,
  uint32_t const length,
  uint64_t const remote_offset) noexcept
{

  struct ibv_send_wr &wr = wr_window[idx];
  struct ibv_sge &sge = sge_window[idx];
  memset(&wr, 0, sizeof(wr));// maybe optimize away

  // wr.wr_id = reinterpret_cast<uintptr_t>(av);
  wr.opcode = IBV_WR_RDMA_READ;
  wr.send_flags = IBV_SEND_SIGNALED;// can change for selective signaling
  wr.wr.rdma.remote_addr = ctx->peer_addr + remote_offset;
  wr.wr.rdma.rkey = ctx->peer_rkey;

  wr.sg_list = &sge;
  wr.num_sge = 1;
  sge.addr = reinterpret_cast<uintptr_t>(buffer);
  sge.length = length;
  sge.lkey = ctx->heap_mr->lkey;

  if (idx > 0) {
    struct ibv_send_wr &prev = wr_window[idx - 1];
    prev.next = &wr;
    wr.send_flags = 0;// send unsignaled
  }
}

struct vertex_range
{
  // interval of form [v_s, v_end] note the [
  uint32_t v_s;
  uint32_t v_e;
};

template<typename V>
auto pack_window(std::array<struct ibv_send_wr, famgraph::WR_WINDOW_SIZE> &wr_window,
  std::array<vertex_range, famgraph::WR_WINDOW_SIZE> &vertex_batch,
  std::array<struct ibv_sge, famgraph::WR_WINDOW_SIZE> &sge_window,
  uint32_t const edge_buf_size,
  V *const vtable,
  uint32_t const range_start,
  uint32_t const range_end,
  Bitmap const &frontier,
  struct client_context *const ctx,
  uint32_t *const edge_buf) noexcept
{
  uint32_t const g_total_verts = ctx->app->num_vertices;
  uint64_t const g_total_edges = ctx->app->num_edges;
  uint32_t total_edges = 0;
  uint32_t batch_size = 0;
  uint32_t wrs = 0;
  uint32_t v = range_start;
  while ((total_edges < edge_buf_size) && (wrs < famgraph::WR_WINDOW_SIZE)
         && (v < range_end)) {
    if (frontier.get_bit(v)) {
      uint32_t const n_out_edge =
        famgraph::get_num_edges(v, vtable, g_total_verts, g_total_edges);
      if (total_edges + n_out_edge <= edge_buf_size) {
        if (n_out_edge > 0) {
          uint32_t *const b = edge_buf + total_edges;
          b[0] = famgraph::NULL_VERT;// sign
          // BOOST_LOG_TRIVIAL(info) << "v: " << v << " total_edges= " << total_edges << "
          // n_out_edge= " << n_out_edge << " edgebuf_suze= " << edge_buf_size;
          b[n_out_edge - 1] = famgraph::NULL_VERT;// sign
          if (famgraph::build_options::vertex_coalescing && batch_size > 0
              && v == vertex_batch[wrs - 1].v_e + 1) {
            vertex_batch[wrs - 1].v_e = v;
            sge_window[wrs - 1].length +=
              n_out_edge * static_cast<uint32_t>(sizeof(uint32_t));
          } else {
            vertex_batch[wrs].v_s = v;
            vertex_batch[wrs].v_e = v;
            prep_wr(wr_window,
              sge_window,
              wrs,
              ctx,
              b,
              n_out_edge * static_cast<uint32_t>(sizeof(uint32_t)),
              vtable[v].edge_offset * sizeof(uint32_t));
            wrs++;
          }

          batch_size++;
          total_edges += n_out_edge;
        }
      } else {// can't fit this, so it is the next one up
        std::get<0>(ctx->stats.wrs_verts_sends.local()) += wrs;
        std::get<1>(ctx->stats.wrs_verts_sends.local()) += batch_size;
        std::get<2>(ctx->stats.wrs_verts_sends.local())++;
        return std::make_pair(v, wrs);
      }
    }
    v++;
  }

  std::get<0>(ctx->stats.wrs_verts_sends.local()) += wrs;
  std::get<1>(ctx->stats.wrs_verts_sends.local()) += batch_size;
  std::get<2>(ctx->stats.wrs_verts_sends.local())++;
  return std::make_pair(v, wrs);
}

template<typename V>
auto pack_window2(std::array<struct ibv_send_wr, famgraph::WR_WINDOW_SIZE> &wr_window,
  std::array<vertex_range, famgraph::WR_WINDOW_SIZE> &vertex_batch,
  std::array<struct ibv_sge, famgraph::WR_WINDOW_SIZE> &sge_window,
  uint32_t &batch_size,
  uint32_t const edge_buf_size,
  V *const vtable,
  uint32_t const range_start,
  uint32_t const range_end,
  struct client_context *const ctx,
  uint32_t *const edge_buf,
  uint32_t &n_wr) noexcept
{
  uint32_t const g_total_verts = ctx->app->num_vertices;
  uint64_t const g_total_edges = ctx->app->num_edges;
  uint32_t total_edges = 0;
  uint32_t wrs = 0;
  uint32_t v = range_start;
  while ((total_edges < edge_buf_size) && (wrs < famgraph::WR_WINDOW_SIZE)
         && (v < range_end)) {
    if (true) {
      uint32_t const n_out_edge =
        famgraph::get_num_edges(v, vtable, g_total_verts, g_total_edges);
      if (total_edges + n_out_edge <= edge_buf_size) {
        if (n_out_edge > 0) {
          uint32_t *const b = edge_buf + total_edges;
          b[0] = famgraph::NULL_VERT;// sign
          b[n_out_edge - 1] = famgraph::NULL_VERT;// sign

          if (famgraph::build_options::vertex_coalescing && batch_size > 0
              && v == vertex_batch[wrs - 1].v_e + 1) {
            vertex_batch[wrs - 1].v_e = v;
            sge_window[wrs - 1].length +=
              n_out_edge * static_cast<uint32_t>(sizeof(uint32_t));
          } else {
            vertex_batch[wrs].v_s = v;
            vertex_batch[wrs].v_e = v;
            prep_wr(wr_window,
              sge_window,
              wrs,
              ctx,
              b,
              n_out_edge * static_cast<uint32_t>(sizeof(uint32_t)),
              vtable[v].edge_offset * sizeof(uint32_t));
            wrs++;
          }
          // prep_wr(wr_window, sge_window, batch_size, ctx, b,
          // n_out_edge*sizeof(uint32_t), vtable[v].edge_offset*sizeof(uint32_t));
          batch_size++;
          total_edges += n_out_edge;
          n_wr++;
        } else {
          // vertex_batch[batch_size] = v;
          // batch_size++;
        }
      } else {// can't fit this, so it is the next one up

        std::get<0>(ctx->stats.wrs_verts_sends.local()) += wrs;
        std::get<1>(ctx->stats.wrs_verts_sends.local()) += batch_size;
        std::get<2>(ctx->stats.wrs_verts_sends.local())++;
        return std::make_pair(v, wrs);
      }
    }
    v++;
  }

  std::get<0>(ctx->stats.wrs_verts_sends.local()) += wrs;
  std::get<1>(ctx->stats.wrs_verts_sends.local()) += batch_size;
  std::get<2>(ctx->stats.wrs_verts_sends.local())++;
  return std::make_pair(v, wrs);
}

namespace single_buffer {
  template<typename F, typename Context>
  void for_each_active_batch(Bitmap const &frontier,
    tbb::blocked_range<uint32_t> const my_range,
    Context &c,
    F const &function) noexcept
  {
    auto const idx = c.p.first.get();
    auto RDMA_area = c.RDMA_window.get();
    auto const edge_buf_size = c.edge_buf_size;
    auto ctx = c.context;

    tbb::parallel_for(my_range, [&](auto const &range) {
      struct timespec t1, t2, res;
      size_t worker_id =
        static_cast<size_t>(tbb::this_task_arena::current_thread_index());
      uint32_t *const edge_buf = RDMA_area + (worker_id * edge_buf_size);
      std::array<struct ibv_send_wr, famgraph::WR_WINDOW_SIZE> my_window;
      std::array<vertex_range, famgraph::WR_WINDOW_SIZE> vertex_batch;
      std::array<struct ibv_sge, famgraph::WR_WINDOW_SIZE> sge_window;
      uint32_t next_range_start = range.begin();
      uint32_t const range_end = range.end();
      while (next_range_start < range_end) {
        auto const [next, wrs] = pack_window<>(my_window,
          vertex_batch,
          sge_window,
          edge_buf_size,
          idx,
          next_range_start,
          range_end,
          frontier,
          ctx,
          edge_buf);

        next_range_start = next;

        struct ibv_send_wr *bad_wr = NULL;
        struct ibv_send_wr &wr = my_window[0];

        if (wrs > 0) {
          TEST_NZ(ibv_post_send((ctx->cm_ids)[worker_id]->qp, &wr, &bad_wr));
          uint32_t volatile *e_buf = edge_buf;
          for (uint32_t i = 0; i < wrs; ++i) {
            for (uint32_t v = vertex_batch[i].v_s; v <= vertex_batch[i].v_e; v++) {
              uint32_t n_edges = famgraph::get_num_edges(
                v, idx, ctx->app->num_vertices, ctx->app->num_edges);
              clock_gettime(CLOCK_MONOTONIC, &t1);
              while (e_buf[0] == famgraph::NULL_VERT) {}
              while (e_buf[n_edges - 1] == famgraph::NULL_VERT) {}
              clock_gettime(CLOCK_MONOTONIC, &t2);
              famgraph::timespec_diff(&t2, &t1, &res);
              ctx->stats.spin_time.local() += res.tv_sec * 1000000000L + res.tv_nsec;
              clock_gettime(CLOCK_MONOTONIC, &t1);
              function(v, const_cast<uint32_t *const>(e_buf), n_edges);
              clock_gettime(CLOCK_MONOTONIC, &t2);
              famgraph::timespec_diff(&t2, &t1, &res);
              ctx->stats.function_time.local() += res.tv_sec * 1000000000L + res.tv_nsec;
              e_buf += n_edges;
            }
          }
        }
      }
    });
    print_stats_round(ctx->stats);
    clear_stats_round(ctx->stats);
  }

  template<typename F>
  void for_each_range(tbb::blocked_range<uint32_t> const my_range,
    uint32_t *const RDMA_area,
    uint32_t const edge_buf_size,
    famgraph::vertex *const vtable,
    struct client_context *const ctx,
    F const &function) noexcept
  {
    tbb::parallel_for(my_range, [&](auto const &range) {
      struct timespec t1, t2, res;
      size_t const worker_id =
        static_cast<size_t>(tbb::this_task_arena::current_thread_index());
      uint32_t *const edge_buf = RDMA_area + (worker_id * edge_buf_size);
      std::array<struct ibv_send_wr, famgraph::WR_WINDOW_SIZE> my_window;
      std::array<vertex_range, famgraph::WR_WINDOW_SIZE> vertex_batch;
      std::array<struct ibv_sge, famgraph::WR_WINDOW_SIZE> sge_window;
      uint32_t next_range_start = range.begin();
      uint32_t range_end = range.end();
      while (next_range_start < range_end) {
        uint32_t batch_size = 0;// number of wr's
        uint32_t n_wr = 0;

        auto const [next, wrs] = pack_window2<>(my_window,
          vertex_batch,
          sge_window,
          batch_size,
          edge_buf_size,
          vtable,
          next_range_start,
          range_end,
          ctx,
          edge_buf,
          n_wr);

        next_range_start = next;

        struct ibv_send_wr *bad_wr = NULL;
        struct ibv_send_wr &wr = my_window[0];
        if (batch_size > 0) {
          TEST_NZ(ibv_post_send((ctx->cm_ids)[worker_id]->qp, &wr, &bad_wr));
          uint32_t volatile *e_buf = edge_buf;
          for (uint32_t i = 0; i < wrs; ++i) {
            for (uint32_t v = vertex_batch[i].v_s; v <= vertex_batch[i].v_e; v++) {
              uint32_t n_edges = famgraph::get_num_edges(
                v, vtable, ctx->app->num_vertices, ctx->app->num_edges);
              clock_gettime(CLOCK_MONOTONIC, &t1);
              while (e_buf[0] == famgraph::NULL_VERT) {}
              while (e_buf[n_edges - 1] == famgraph::NULL_VERT) {}
              clock_gettime(CLOCK_MONOTONIC, &t2);
              famgraph::timespec_diff(&t2, &t1, &res);
              ctx->stats.spin_time.local() += res.tv_sec * 1000000000L + res.tv_nsec;
              clock_gettime(CLOCK_MONOTONIC, &t1);
              function(v, const_cast<uint32_t *const>(e_buf), n_edges);
              e_buf += n_edges;
              clock_gettime(CLOCK_MONOTONIC, &t2);
              famgraph::timespec_diff(&t2, &t1, &res);
              ctx->stats.function_time.local() += res.tv_sec * 1000000000L + res.tv_nsec;
            }
          }
        }
      }
    });

    print_stats_round(ctx->stats);
    clear_stats_round(ctx->stats);
  }
}// namespace single_buffer
}// namespace famgraph
#endif
