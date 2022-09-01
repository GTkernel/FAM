#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>

#include <boost/program_options.hpp>
#include <boost/log/trivial.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using std::vector;

namespace {

template<typename T> auto num_elements(fs::path const &p)
{
  const auto file_size = fs::file_size(p);
  const auto n = file_size / sizeof(T);
  return n;
}

template<typename T> auto read_one(std::ifstream &s)
{
  T x;
  s.read(reinterpret_cast<char *>(&x), sizeof(T));
  if (static_cast<unsigned long>(s.gcount()) != sizeof(T)) {
    throw std::runtime_error("can't read index data");
  }
  return x;
}

template<typename T> void write_one(fs::ofstream &o, T const x)
{
  o.write(reinterpret_cast<char const *>(&x), sizeof(x));
}


template<typename T>
void write_as(std::ifstream &input, fs::ofstream &o, uint64_t const n)
{
  auto const onePCT = std::max(n / 100, static_cast<uint64_t>(1));
  uint64_t written = 0;
  int percent_done = 0;

  for (uint64_t i = 0; i < n; ++i) {
    T x = read_one<T>(input);
    write_one(o, x);

    written++;
    if (written % (2 * onePCT) == 0) {
      percent_done += 2;
      if (percent_done % 10 == 0)
        std::cout << percent_done << "%" << std::flush;
      else
        std::cout << "#" << std::flush;
    }
  }
  std::cout << std::endl;
}

void validate_file(fs::path const &p)
{
  if (!(fs::exists(p) && fs::is_regular_file(p))) {
    throw std::runtime_error("file not found");
  }
}

void write_header(fs::ofstream &o, uint64_t const n_v, uint64_t const n_e)
{
  uint64_t const version = 1;
  uint64_t const edgedata_size = 0;

  write_one(o, version);
  write_one(o, edgedata_size);
  write_one(o, n_v);
  write_one(o, n_e);
}
}// namespace

int main(int argc, char *argv[])
{
  try {
    po::options_description desc{ "Options" };
    desc.add_options()("help,h", "Help screen")("idx,i",
      po::value<std::string>(),
      ".idx filepath")("adj,a", po::value<std::string>(), ".adj filepath")(
      "symmetric,s", "use .sgr extension for outfile");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    if (vm.count("help")) {
      std::cout << desc << std::endl;
      return 0;
    };

    std::string file = vm["idx"].as<std::string>();
    fs::path index(file);
    std::string file2 = vm["adj"].as<std::string>();
    fs::path adj(file2);

    auto out = [&](fs::path f) {
      if (vm.count("symmetric"))
        return f.replace_extension(".sgr");
      else
        return f.replace_extension(".gr");
    }(index);

    BOOST_LOG_TRIVIAL(info) << ".idx file " << index;
    validate_file(index);
    BOOST_LOG_TRIVIAL(info) << ".adj file " << adj;
    validate_file(adj);
    BOOST_LOG_TRIVIAL(info) << "outfile " << out;

    fs::ofstream os(out);
    auto const verts = num_elements<uint64_t>(index);
    auto const edges = num_elements<uint32_t>(adj);

    std::ifstream idxstream(index.c_str(), std::ios::binary);
    if (!idxstream) throw std::runtime_error("Couldn't open file!");

    std::ifstream adjstream(adj.c_str(), std::ios::binary);
    if (!adjstream) throw std::runtime_error("Couldn't open file!");

    write_header(os, verts, edges);

    BOOST_LOG_TRIVIAL(info) << "Writing idx";
    read_one<uint64_t>(idxstream);// burn card
    write_as<uint64_t>(idxstream, os, verts - 1);
    write_one(os, edges);

    BOOST_LOG_TRIVIAL(info) << "Writing adj";
    write_as<uint32_t>(adjstream, os, edges);
    if (edges & 1) write_one(os, static_cast<uint32_t>(0));
    os.close();

    return 0;
  } catch (const std::exception &ex) {
    std::cout << "Caught Runtime Exception: " << ex.what() << std::endl;
    return 1;
  }
}
