#ifndef _EDGEMAP_H_
#define _EDGEMAP_H_

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#include <oneapi/tbb.h>
#pragma GCC diagnostic pop

#include "bitmap.hpp"
#include "vertex_table.hpp"
#include "communication_runtime.hpp"
#include <vector>
#include <infiniband/verbs.h>
#include <iostream>
#include <atomic>

namespace famgraph {
class next_set
{
  famgraph::Bitmap const &b;
  std::uint32_t from_inclusive;
  std::uint32_t const end_exclusive;

public:
  next_set(famgraph::Bitmap const &t_b, std::uint32_t t_start, std::uint32_t t_end)
    : b{ t_b }, from_inclusive{ t_start }, end_exclusive{ t_end }
  {}
  auto operator()() noexcept
  {
    while (from_inclusive < end_exclusive) {
      auto const prev = from_inclusive++;
      if (this->b.get_bit(prev)) return prev;
    }

    return end_exclusive;
  }
};

struct v_interval
{
  std::uint32_t const v;// name
  std::uint32_t const d;// degree of vertex
  std::uint32_t const start;
  std::uint32_t end;

  v_interval(std::uint32_t t_v,
    std::uint32_t t_d,
    std::uint32_t t_start,
    std::uint32_t t_end)
    : v{ t_v }, d{ t_d }, start{ t_start }, end{ t_end }
  {}
};

class next_batch
{
  famgraph::Bitmap const &b;
  std::uint32_t cur_v;
  std::uint32_t cur_e;
  std::uint32_t const end;
  std::uint32_t const capacity;

public:
  next_batch(famgraph::Bitmap const &t_b,
    std::uint32_t t_start,
    std::uint32_t t_end,
    std::uint32_t t_capacity)
    : b{ t_b }, cur_v{ t_start }, cur_e{ 0 }, end{ t_end }, capacity{ t_capacity }
  {}

  auto is_done() noexcept { return this->cur_v >= this->end; }

  template<typename F> auto operator()(F const &get_degree) noexcept
  {
    std::vector<v_interval> vec;
    famgraph::next_set get_next_v{ this->b, this->cur_v + 1, this->end };
    std::uint32_t edges = 0;
    auto &v = this->cur_v;

    for (; v < this->end && edges < this->capacity;) {
      auto const d = get_degree(v);
      if (d == 0) {
        v = get_next_v();
        this->cur_e = 0;
        continue;
      };
      auto const n_left = d - this->cur_e;
      auto const space_left = this->capacity - edges;
      auto const take = std::min(n_left, space_left);
      vec.emplace_back(v, d, this->cur_e, this->cur_e + take);
      edges += take;
      if (take == n_left) {
        v = get_next_v();
        this->cur_e = 0;
      } else {
        this->cur_e += take;
      }
    }

    return vec;
  }
};


struct WR
{
  ibv_send_wr wr;
  ibv_sge sge;
};

inline auto coalesce_intervals(std::vector<v_interval> const &vec) noexcept
{
  std::vector<v_interval> combined;
  combined.push_back(vec[0]);
  for (size_t i = 1; i < vec.size(); ++i) {
    auto const &interval = vec[i];
    if (vec[i - 1].v + 1 == interval.v) {
      auto &last_interval = combined.back();
      last_interval.end += interval.end - interval.start;
    } else {
      combined.push_back(interval);
    }
  }

  return combined;
}


inline void sign_buffer(std::vector<v_interval> const &vec,
  uint32_t *const buffer) noexcept
{
  auto b = buffer;
  for (auto &interval : vec) {
    auto const edges = interval.end - interval.start;
    b[0] = famgraph::NULL_VERT;
    b[edges - 1] = famgraph::NULL_VERT;
    b += edges;
  }
}

inline auto make_WRs(std::vector<v_interval> const &vec,
  std::uint32_t const rkey,
  std::uint32_t const lkey,
  std::uintptr_t const peer_addr,
  famgraph::vertex *const vtable,
  uint32_t *const RDMA_area) noexcept
{
  std::vector<WR> WRs;
  auto buffer = RDMA_area;

  for (auto const &interval : vec) {
    auto const v = interval.v;
    auto const remote_offset =
      (vtable[v].edge_offset + interval.start) * sizeof(uint32_t);
    auto const edges = interval.end - interval.start;
    WRs.emplace_back();
    auto &wr = WRs.back().wr;
    auto &sge = WRs.back().sge;

    memset(&wr, 0, sizeof(wr));
    wr.opcode = IBV_WR_RDMA_READ;
    wr.send_flags = 0;// not signaled
    wr.wr.rdma.remote_addr = peer_addr + remote_offset;
    wr.wr.rdma.rkey = rkey;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    sge.addr = reinterpret_cast<uintptr_t>(buffer);
    sge.length = edges * static_cast<std::uint32_t>(sizeof(std::uint32_t));
    sge.lkey = lkey;

    buffer += edges;
  }

  return WRs;
}

inline void post_all(std::vector<WR> &WRs, ibv_qp *qp) noexcept
{
  ibv_send_wr *wr = &WRs[0].wr;
  for (size_t i = 0; i < WRs.size(); ++i) {
    auto &my_wr = WRs[i].wr;
    if ((i % famgraph::WR_WINDOW_SIZE == famgraph::WR_WINDOW_SIZE - 1)
        || (i == WRs.size() - 1)) {
      my_wr.send_flags = IBV_SEND_SIGNALED;
      my_wr.next = nullptr;

      ibv_send_wr *bad_wr = nullptr;
      TEST_NZ(ibv_post_send(qp, wr, &bad_wr));
      wr = i == WRs.size() - 1 ? nullptr : &WRs[i + 1].wr;
    } else {
      my_wr.next = &WRs[i + 1].wr;
    }
  }
}

template<typename F>
void edgemap(famgraph::Bitmap const &frontier,
  tbb::blocked_range<uint32_t> const &my_range,
  uint32_t *const RDMA_area,
  uint32_t const edge_buf_size,
  famgraph::vertex *const vtable,
  struct client_context *const ctx,
  F const &function) noexcept
{
  auto get_degree = [=](std::uint32_t v) noexcept
  {
    return famgraph::get_num_edges(
      v, vtable, ctx->app->num_vertices, ctx->app->num_edges);
  };

  std::cerr << "Edgemap()" << std::endl;

  tbb::parallel_for(my_range, [&](auto const &range) noexcept {
    auto const worker_id =
      static_cast<size_t>(tbb::this_task_arena::current_thread_index());
    auto qp = (ctx->cm_ids)[worker_id]->qp;
    auto const rkey = ctx->peer_rkey;
    auto const lkey = ctx->heap_mr->lkey;
    auto const edge_buf = RDMA_area + (worker_id * edge_buf_size);
    auto start = range.begin();
    auto const end = range.end();
    // std::cerr << "A" << std::endl;
    next_batch get_next_batch{ frontier, start, end, edge_buf_size };
    while (!get_next_batch.is_done()) {
      // std::cerr << "B" << std::endl;
      auto const intervals = get_next_batch(get_degree);
      // std::cerr << "C" << std::endl;
      if (intervals.empty()) continue;

      // std::cerr << "signing" << std::endl;
      sign_buffer(intervals, edge_buf);
      auto const combined = coalesce_intervals(intervals);
      auto WRs = make_WRs(combined, rkey, lkey, ctx->peer_addr, vtable, edge_buf);
      post_all(WRs, qp);

      // std::cerr << "done posting" << std::endl;
      uint32_t volatile *e_buf = edge_buf;
      for (auto &interval : intervals) {
        auto const edges = interval.end - interval.start;
        while (e_buf[0] == famgraph::NULL_VERT) {}
        while (e_buf[edges - 1] == famgraph::NULL_VERT) {}
        // std::cerr << "done spinning" << std::endl;
        std::atomic_thread_fence(std::memory_order_seq_cst);
        function(interval.v, const_cast<uint32_t *const>(e_buf), edges, interval.d);
        e_buf += edges;
      }
    }
  });
}
}// namespace famgraph

#endif//_EDGEMAP_H_
