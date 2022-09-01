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

template<typename T> auto read_one(std::ifstream &s)
{
  T x;
  s.read(reinterpret_cast<char *>(&x), sizeof(T));
  if (static_cast<unsigned long>(s.gcount()) != sizeof(T)) {
    throw std::runtime_error("can't read index data");
  }
  return x;
}

template<typename T>
void compare_for(std::ifstream &streamA, std::ifstream &streamB, uint64_t const n)
{
  auto const onePCT = std::max(n / 100, static_cast<uint64_t>(1));
  uint64_t read = 0;
  int percent_done = 0;

  for (uint64_t i = 0; i < n; ++i) {
    auto a = read_one<T>(streamA);
    auto b = read_one<T>(streamB);

    if (a != b){     
      std::cout << "\nindex: " << i << " A: " << a << " B: " << b << std::endl;
    }
    
    read++;
    if (read % (2*onePCT) == 0) {
      percent_done+=2;
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

auto compare_headers(std::ifstream &streamA, std::ifstream &streamB)
{
  using header_word = uint64_t;
  auto vA = read_one<header_word>(streamA);
  auto weightA = read_one<header_word>(streamA);
  auto vertsA = read_one<header_word>(streamA);
  auto edgesA = read_one<header_word>(streamA);

  auto vB = read_one<header_word>(streamB);
  auto weightB = read_one<header_word>(streamB);
  auto vertsB = read_one<header_word>(streamB);
  auto edgesB = read_one<header_word>(streamB);

  auto matched = (vA == vB || weightA == weightB || vertsA == vertsB || edgesA == edgesB);

  return std::make_tuple(matched, vertsA, edgesA);
}
}// namespace

int main(int argc, char *argv[])
{
  try {
    po::options_description desc{ "Options" };
    desc.add_options()("help,h", "Help screen")("A,a",
      po::value<std::string>(),
      ".gr file1")("B,b", po::value<std::string>(), ".gr file2");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    if (vm.count("help")) {
      std::cout << desc << std::endl;
      return 0;
    };

    fs::path f1{ vm["A"].as<std::string>() };
    fs::path f2{ vm["B"].as<std::string>() };

    BOOST_LOG_TRIVIAL(info) << f1;
    BOOST_LOG_TRIVIAL(info) << f2;

    validate_file(f1);
    validate_file(f2);

    std::ifstream inputA(f1.c_str(), std::ios::binary);
    if (!inputA) throw std::runtime_error("Couldn't open file!");
    std::ifstream inputB(f2.c_str(), std::ios::binary);
    if (!inputB) throw std::runtime_error("Couldn't open file!");

    auto [m, V, E] = compare_headers(inputA, inputB);
        
    if (!m){
      BOOST_LOG_TRIVIAL(fatal) << "headers do not match";
      return 0;
    }

    BOOST_LOG_TRIVIAL(fatal) << "|V|: " << V;
    BOOST_LOG_TRIVIAL(fatal) << "|E|: " << E;

    BOOST_LOG_TRIVIAL(fatal) << "comparing index";
    compare_for<uint64_t>(inputA, inputB, V);
    BOOST_LOG_TRIVIAL(fatal) << "comparing adj";
    compare_for<uint32_t>(inputA, inputB, E);
    
    return 0;
  } catch (const std::exception &ex) {
    std::cout << "Caught Runtime Exception: " << ex.what() << std::endl;
    return 1;
  }
}
