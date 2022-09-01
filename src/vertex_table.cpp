#include "vertex_table.hpp"

#include <boost/numeric/conversion/cast.hpp>

// this returns a uint32, so we need to do runtime check to assert < 4B verts
uint32_t famgraph::get_num_verts(std::string const &file)
{
  namespace fs = boost::filesystem;
  fs::path p(file);
  if (fs::exists(p) && fs::is_regular_file(p)) {
    const uint64_t file_size = fs::file_size(p);
    const uint64_t num_vertices = file_size / sizeof(uint64_t);
    return boost::numeric_cast<uint32_t>(num_vertices);
  }
  throw std::runtime_error("couldn't read num verts");
}

uint32_t famgraph::get_num_edges(uint32_t const v,
  famgraph::vertex const *const table,
  uint32_t const total_verts,
  uint64_t const total_edges) noexcept
{
  uint64_t const start_range = table[v].edge_offset;
  uint64_t const max_exclusive =
    v == (total_verts - 1) ? total_edges : table[v + 1].edge_offset;
  assert(start_range <= max_exclusive);
  uint32_t const num_edges = static_cast<uint32_t>(max_exclusive - start_range);
  return num_edges;
}

uint32_t famgraph::get_max_out_degree(famgraph::vertex *const vtable,
  uint32_t const n_vert,
  uint64_t const n_edges) noexcept
{
  uint32_t my_max = 0;
  for (uint32_t v = 0; v < n_vert; v++) {
    my_max = std::max(my_max, famgraph::get_num_edges(v, vtable, n_vert, n_edges));
  }
  return my_max;
}
