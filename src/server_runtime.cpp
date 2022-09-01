#include <iostream>
#include <stdexcept>

#include <server_runtime.hpp>

#include <boost/program_options.hpp>
#include <boost/log/trivial.hpp>
#include <boost/filesystem.hpp>//Remove when refactor edgelist init
#include <boost/filesystem/fstream.hpp>//Remove when refactor edgelist init

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>//for mmap, munmap
#include <sys/types.h>//for open
#include <sys/stat.h>//for open
#include <fcntl.h>//for open

#include "mmap_util.hpp"
#include "connection_utils.hpp"
#include "messages.hpp"

namespace {
struct conn_context *g_ctx = 0;

void validate_params(boost::program_options::variables_map const &vm)
{
  if (!vm.count("server-addr"))
    throw boost::program_options::validation_error(
      boost::program_options::validation_error::invalid_option_value, "server-addr");
  if (!vm.count("port"))
    throw boost::program_options::validation_error(
      boost::program_options::validation_error::invalid_option_value, "port");
  if (!vm.count("edgefile"))
    throw boost::program_options::validation_error(
      boost::program_options::validation_error::invalid_option_value, "edgefile");
}

struct conn_context// consider renaming server context
{
  std::string adj_filename;
  std::vector<std::unique_ptr<uint32_t, famgraph::RDMA_mmap_deleter>> v;

  struct message *tx_msg;
  struct ibv_mr *tx_msg_mr;

  struct message *rx_msg;
  struct ibv_mr *rx_msg_mr;

  bool use_hp{ false };

  conn_context(std::string const &file) : adj_filename{ file } {}

  conn_context &operator=(const conn_context &) = delete;
  conn_context(const conn_context &) = delete;
};

template<typename T> auto num_elements(boost::filesystem::path const &p)
{
  const auto file_size = boost::filesystem::file_size(p);
  const auto n = file_size / sizeof(T);
  return n;
}

class file_mapper
{
  constexpr static uint64_t default_chunk = 30UL * (1 << 30);// 30 GB
  uint64_t const chunk_size{ default_chunk };
  uint64_t offset{ 0 };
  uint64_t filesize;
  int fd;

public:
  file_mapper(std::string const &file, uint64_t t_fsize)
    : filesize{ t_fsize }, fd{ open(file.c_str(), O_RDONLY) }
  {
    if (this->fd == -1) { throw std::runtime_error("open() failed on .adj file"); }
  }
  
  ~file_mapper() { close(this->fd);}

  file_mapper(const file_mapper &) = delete;
  file_mapper &operator=(const file_mapper &) = delete;

  bool has_next() noexcept { return this->offset < this->filesize; }

  auto operator()()
  {
    auto length = std::min(this->chunk_size, this->filesize - this->offset);
    auto del = [length](void *p) {
      auto r = munmap(p, length);
      if (r) BOOST_LOG_TRIVIAL(fatal) << "munmap chunk failed";
    };

    auto constexpr flags = MAP_PRIVATE | MAP_POPULATE;
    auto ptr = mmap(0, length, PROT_READ, flags, this->fd, static_cast<long>(this->offset));
    this->offset += length;

    return make_pair(std::unique_ptr<void, decltype(del)>(ptr, del), length);
  }
};

auto get_edge_list(std::string file, ibv_pd *pd, bool use_HP)
{
  namespace fs = boost::filesystem;

  fs::path p(file);
  if (!(fs::exists(p) && fs::is_regular_file(p)))
    throw std::runtime_error(".adj file not found");

  auto const edges = num_elements<uint32_t>(p);
  auto ptr = famgraph::RDMA_mmap_unique<uint32_t>(edges, pd, use_HP);
  auto array = reinterpret_cast<char*>(ptr.get());
  auto const filesize = edges * sizeof(uint32_t);
  file_mapper get_mapped_chunk{file, filesize};

  while (get_mapped_chunk.has_next()){
    auto const [fptr, len] = get_mapped_chunk();
    std::memcpy(array, fptr.get(), len);
    array += len;
    std::cout << "#" << std::flush;
  }
  
  auto mr = ptr.get_deleter().mr;
  return std::make_tuple(std::move(ptr), mr, edges);
}

void send_message(struct rdma_cm_id *id)
{
  struct conn_context *ctx = static_cast<struct conn_context *>(id->context);

  struct ibv_send_wr wr, *bad_wr = NULL;
  struct ibv_sge sge;

  memset(&wr, 0, sizeof(wr));

  wr.wr_id = reinterpret_cast<uintptr_t>(id);
  wr.opcode = IBV_WR_SEND;
  wr.sg_list = &sge;
  wr.num_sge = 1;
  wr.send_flags = IBV_SEND_SIGNALED;

  sge.addr = reinterpret_cast<uintptr_t>(ctx->tx_msg);
  sge.length = sizeof(*ctx->tx_msg);
  sge.lkey = ctx->tx_msg_mr->lkey;

  TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));
}

void post_receive(struct rdma_cm_id *id)
{
  struct conn_context *ctx = static_cast<struct conn_context *>(id->context);
  struct ibv_recv_wr wr, *bad_wr = NULL;
  struct ibv_sge sge;

  memset(&wr, 0, sizeof(wr));

  wr.wr_id = reinterpret_cast<uintptr_t>(id);
  wr.sg_list = &sge;
  wr.num_sge = 1;

  sge.addr = reinterpret_cast<uintptr_t>(ctx->rx_msg);
  sge.length = sizeof(*ctx->rx_msg);
  sge.lkey = ctx->rx_msg_mr->lkey;

  TEST_NZ(ibv_post_recv(id->qp, &wr, &bad_wr));
}

void on_pre_conn(struct rdma_cm_id *id)
{
  BOOST_LOG_TRIVIAL(debug) << "precon";
  struct conn_context *ctx = g_ctx;// find a better way later

  id->context = ctx;

  if (posix_memalign(reinterpret_cast<void **>(&ctx->tx_msg),
        static_cast<size_t>(sysconf(_SC_PAGESIZE)),
        sizeof(*ctx->tx_msg))) {
    throw std::runtime_error("posix memalign failed");
  }

  TEST_Z(ctx->tx_msg_mr = ibv_reg_mr(rc_get_pd(), ctx->tx_msg, sizeof(*ctx->tx_msg), 0));

  if (posix_memalign(reinterpret_cast<void **>(&ctx->rx_msg),
        static_cast<size_t>(sysconf(_SC_PAGESIZE)),
        sizeof(*ctx->rx_msg))) {
    throw std::runtime_error("posix memalign failed");
  }
  TEST_Z(ctx->rx_msg_mr = ibv_reg_mr(
           rc_get_pd(), ctx->rx_msg, sizeof(*ctx->rx_msg), IBV_ACCESS_LOCAL_WRITE));

  post_receive(id);
}

void on_connection(struct rdma_cm_id *id)
{
  BOOST_LOG_TRIVIAL(debug) << "on connection";
  struct conn_context *ctx = static_cast<struct conn_context *>(id->context);

  auto [ptr, mr, edges] = get_edge_list(ctx->adj_filename, rc_get_pd(), ctx->use_hp);
  ctx->v.emplace_back(std::move(ptr));
  
  ctx->tx_msg->id = MSG_MR;
  ctx->tx_msg->data.mr.addr = reinterpret_cast<uintptr_t>(mr->addr);
  ctx->tx_msg->data.mr.rkey = mr->rkey;
  ctx->tx_msg->data.mr.total_edges = edges;

  send_message(id);
}

void on_completion(struct ibv_wc *wc)
{
  BOOST_LOG_TRIVIAL(debug) << "completion";
  struct rdma_cm_id *id = reinterpret_cast<struct rdma_cm_id *>(wc->wr_id);
  struct conn_context *ctx = static_cast<struct conn_context *>(id->context);

  if (wc->opcode & IBV_WC_RECV) {
    if (ctx->rx_msg->id == MSG_READY) {
      post_receive(id);
      BOOST_LOG_TRIVIAL(debug) << "received READY";
      ctx->tx_msg->id = MSG_DONE;
      send_message(id);
    } else if (ctx->rx_msg->id == MSG_DONE) {// server never receives this
      printf("received DONE\n");
      post_receive(id);
      ctx->tx_msg->id = MSG_DONE;
      send_message(id);
      // rc_disconnect(id);//should never recv this...
      return;
    }
  }
}

void on_disconnect(struct rdma_cm_id *id)
{
  struct conn_context *ctx = static_cast<struct conn_context *>(id->context);

  ibv_dereg_mr(ctx->rx_msg_mr);
  ibv_dereg_mr(ctx->tx_msg_mr);
  free(ctx->rx_msg);
  free(ctx->tx_msg);
}
}// namespace


void run_server(boost::program_options::variables_map const &vm)
{
  validate_params(vm);
  std::string server_ip = vm["server-addr"].as<std::string>();
  std::string server_port = vm["port"].as<std::string>();
  std::string file = vm["edgefile"].as<std::string>();

  BOOST_LOG_TRIVIAL(info) << "Starting server";
  BOOST_LOG_TRIVIAL(info) << "Server IPoIB address: " << server_ip
                          << " port: " << server_port;

  BOOST_LOG_TRIVIAL(info) << "Reading in edgelist";

  struct conn_context ctx{file};
  ctx.use_hp = vm.count("hp") ? true : false;
  BOOST_LOG_TRIVIAL(info) << "hugepages? " << ctx.use_hp;
  g_ctx = &ctx;

  rc_init(on_pre_conn, on_connection, on_completion, on_disconnect);

  BOOST_LOG_TRIVIAL(info) << "waiting for connections. interrupt (^C) to exit.";

  rc_server_loop(server_port.c_str());
}
