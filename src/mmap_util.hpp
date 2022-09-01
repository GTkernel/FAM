#ifndef __FG_MMAP_UTIL_H__
#define __FG_MMAP_UTIL_H__

#include <infiniband/verbs.h>
#include <sys/mman.h>
#include <boost/log/trivial.hpp>
#include <memory>
#include <stdexcept>
#include <boost/align/align_up.hpp>

namespace famgraph {
constexpr auto PROT_RW = PROT_READ | PROT_WRITE;
constexpr auto MAP_ALLOC = MAP_PRIVATE | MAP_ANONYMOUS;
constexpr auto IB_FLAGS = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ;

struct RDMA_mmap_deleter
{
  std::size_t const m_size;
  struct ibv_mr *const mr;

  RDMA_mmap_deleter(std::size_t size, struct ibv_mr *t_mr) : m_size{ size }, mr{ t_mr } {}

  void operator()(void *ptr) const
  {
    if (ibv_dereg_mr(mr)) { BOOST_LOG_TRIVIAL(fatal) << "error unmapping RDMA buffer"; }
    munmap(ptr, m_size);
  }
};

template<typename T> auto RDMA_mmap_unique(uint64_t array_size, ibv_pd *pd, bool use_HP)
{
  auto constexpr HP_align = 1 << 30;// 1 GB huge pages
  auto const HP_FLAGS = use_HP ? MAP_HUGETLB : 0;
  auto const req_size = sizeof(T) * array_size;
  auto const aligned_size =
    use_HP ? boost::alignment::align_up(req_size, HP_align) : req_size;

  BOOST_LOG_TRIVIAL(debug) << "aligned size: " << aligned_size << " use_HP: " << use_HP;
  if (auto ptr = mmap(0, aligned_size, PROT_RW, MAP_ALLOC | HP_FLAGS, -1, 0)) {
    struct ibv_mr *mr = ibv_reg_mr(pd, ptr, aligned_size, IB_FLAGS);
    if (!mr) {
      BOOST_LOG_TRIVIAL(fatal) << "ibv_reg_mr failed";
      throw std::runtime_error("ibv_reg_mr failed");
    }

    auto del = RDMA_mmap_deleter(aligned_size, mr);
    return std::unique_ptr<T, RDMA_mmap_deleter>(static_cast<T *>(ptr), del);
  }

  throw std::bad_alloc();
}

class mmap_deleter
{
  std::size_t m_size;

public:
  mmap_deleter(std::size_t size) : m_size{ size } {}

  void operator()(void *ptr) const { munmap(ptr, m_size); }
};

template<typename T> auto mmap_unique(uint64_t const array_size, bool const use_HP)
{
  auto constexpr HP_align = 1 << 30;// 1 GB huge pages
  auto const HP_FLAGS = use_HP ? MAP_HUGETLB : 0;
  auto const req_size = sizeof(T) * array_size;
  auto const aligned_size =
    use_HP ? boost::alignment::align_up(req_size, HP_align) : req_size;

  BOOST_LOG_TRIVIAL(debug) << "nonRDMA aligned size: " << aligned_size
                           << " use_HP: " << use_HP;
  if (auto ptr = mmap(0, aligned_size, PROT_RW, MAP_ALLOC | HP_FLAGS, -1, 0)) {
    for (uint64_t i = 0; i < array_size; ++i) {
      (void)new (static_cast<T *>(ptr) + i) T{};// T must be default constructable
    }
    auto del = mmap_deleter(aligned_size);
    return std::unique_ptr<T, mmap_deleter>(static_cast<T *>(ptr), del);
  }

  throw std::bad_alloc();
}
}// namespace famgraph

#endif//__FG_MMAP_UTIL_H__
