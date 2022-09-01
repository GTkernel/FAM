#include <iostream>
#include <stdexcept>
#include <vector>
#include <utility> // for pair<>
#include <algorithm>

#include <boost/program_options.hpp>
#include <boost/log/trivial.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <oneapi/dpl/execution>
#include <oneapi/dpl/algorithm>
#pragma GCC diagnostic pop

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using std::vector;

namespace {
    struct w_csr_graph {
        vector<uint64_t> index; //index into edgelist
        vector<uint32_t> dest; //dest verts
        vector<float> val; //edge weight
        
        w_csr_graph(vector<uint64_t> i, vector<uint32_t> d, vector<float> w) :
            index(std::move(i)), dest(std::move(d)), val(std::move(w)) {}

        // void print_as_edgelist(){
        //     for (int row = 0; row < index.size(); ++row){
        //         uint64_t start = index[row];
        //         uint64_t end = row == index.size() - 1 ? dest.size() : index[row+1];
        //         for (int i = start; i < end; ++i){
        //             std::cout << row << " " << dest[i] << std::endl;
        //         }
        //     }
        // }
    };

    struct csr_graph {
        vector<uint64_t> index; //index into edgelist
        vector<uint32_t> dest; //dest verts
        
        csr_graph(vector<uint64_t> i, vector<uint32_t> d) :
            index(std::move(i)), dest(std::move(d)) {}

        // void print_as_edgelist(){
        //     for (int row = 0; row < index.size(); ++row){
        //         uint64_t start = index[row];
        //         uint64_t end = row == index.size() - 1 ? dest.size() : index[row+1];
        //         for (int i = start; i < end; ++i){
        //             std::cout << row << " " << dest[i] << std::endl;
        //         }
        //     }
        // }
    };

}

#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Wold-style-cast"

struct w_csr_graph edge_list_to_w_csr(vector<std::tuple<uint32_t,uint32_t,float>> const & v, uint32_t const max_vert){
    vector<uint64_t> idx (max_vert+1, v.size());
    vector<uint32_t> dest(v.size(), 0);
    vector<float> w(v.size(), 0);

    uint32_t prev_vertex_from = 0;
    uint64_t count = 0;
    idx[0] = 0;
    uint32_t vertex_from = 0;
    for (auto const& p : v){
        vertex_from        = std::get<0>(p);
        uint32_t vertex_to = std::get<1>(p);
        if (vertex_from != prev_vertex_from) {
            //Fill in all rows that have no out edges
            for (uint32_t row = prev_vertex_from + 1;
                row <= vertex_from; ++row){
                idx[row] = count;
            }
            prev_vertex_from = vertex_from;
        }
        dest[count] = vertex_to;
        w[count] = std::get<2>(p);
        count++;
    }
    for (uint32_t s = vertex_from+1; s <= max_vert; ++s){
        idx[s] = count;
    }
    return w_csr_graph(std::move(idx), std::move(dest), std::move(w));
}

void encode_weighted(fs::path const &p, fs::path const &index, fs::path const &adj, po::variables_map const& vm){
    bool make_undirected = vm.count("make-undirected") ? true : false;
    fs::ifstream ifs(p);
    uint32_t a, b;
    float c;
    std::vector<std::tuple<uint32_t, uint32_t, float>> v;
    uint32_t max_vert = 0;
    while (ifs >> a >> b >> c){
        v.push_back(std::make_tuple(a, b, c));
        if (make_undirected){
            v.push_back(std::make_tuple(b, a, c));
        }
        max_vert = std::max(max_vert, std::max(a, b));
    }
    //sort the vector by starting vertex
    if (!vm.count("sorted")){
        oneapi::dpl::sort(oneapi::dpl::execution::par_unseq,v.begin(),v.end());
        // std::sort(v.begin(), v.end());
    }
            
    w_csr_graph g = edge_list_to_w_csr(v, max_vert);
    // g.print_as_edgelist();
    fs::ofstream index_ostream(index);
    fs::ofstream adj_ostream(adj);
    for (auto i : g.index){
        index_ostream.write((char*)&i, sizeof(i));
    }

    for (size_t idx = 0; idx < g.dest.size(); ++idx){
        auto& d = g.dest[idx];
        auto& w = g.val[idx];
        adj_ostream.write((char*)&d, sizeof(d)); //write dest
        adj_ostream.write((char*)&w, sizeof(w)); //write weight

    }
    index_ostream.close();
    adj_ostream.close();
}

struct csr_graph edge_list_to_csr(vector<std::pair<uint32_t,uint32_t>> const & v, uint32_t const max_vert){
    vector<uint64_t> idx (max_vert+1, v.size()-1);
    vector<uint32_t> dest(v.size(), 0);

    BOOST_LOG_TRIVIAL(info) << "dest size: " << dest.size();
    
    uint32_t prev_vertex_from = 0;
    uint64_t count = 0;
    idx[0] = 0;
    uint32_t vertex_from = 0;
    for (auto const& p : v){
        vertex_from        = p.first;
        uint32_t vertex_to = p.second;
        if (vertex_from != prev_vertex_from) {
            //Fill in all rows that have no out edges
            for (uint32_t row = prev_vertex_from + 1;
                row <= vertex_from; ++row){
                idx[row] = count;
            }
            prev_vertex_from = vertex_from;
        }
        dest[count] = vertex_to;
        count++;
    }
    for (uint32_t s = vertex_from+1; s <= max_vert; ++s){
        idx[s] = v.size();
    }
    return csr_graph(std::move(idx), std::move(dest));
}

void encode_unweighted(fs::path const &p, fs::path const &index, fs::path const &adj, po::variables_map const& vm){
    bool make_undirected = vm.count("make-undirected") ? true : false;
    fs::ifstream ifs(p);
    uint32_t a, b;
    std::vector<std::pair<uint32_t, uint32_t>> v;
    uint32_t max_vert = 0;
    while (ifs >> a >> b){
        v.push_back(std::make_pair(a, b));
        if (make_undirected){
            v.push_back(std::make_pair(b,a));
        }
        max_vert = std::max(max_vert, std::max(a, b));
    }

    BOOST_LOG_TRIVIAL(info) << "|V| " << max_vert;
    BOOST_LOG_TRIVIAL(info) << "|E| " << v.size();
    
    //sort the vector by starting vertex
    if (!vm.count("sorted")){
        oneapi::dpl::sort(oneapi::dpl::execution::par_unseq,v.begin(),v.end());
        // std::sort(v.begin(), v.end());
    }
            
    csr_graph g = edge_list_to_csr(v, max_vert);

    BOOST_LOG_TRIVIAL(info) << "Writing " << g.index.size() << " vertices";
    BOOST_LOG_TRIVIAL(info) << "Writing " << g.dest.size() << " edges";
    uint32_t v_written = 0;
    uint64_t e_written = 0;
    
    // g.print_as_edgelist();
    fs::ofstream index_ostream(index);
    fs::ofstream adj_ostream(adj);
    for (auto i : g.index){
        index_ostream.write((char*)&i, sizeof(i));
        v_written++;
    }
    for (auto i : g.dest){
        adj_ostream.write((char*)&i, sizeof(i));
        e_written++;
    }

    BOOST_LOG_TRIVIAL(info) << "Wrote " << v_written << " vertices";
    BOOST_LOG_TRIVIAL(info) << "Wrote " << e_written << " edges";
    
    index_ostream.close();
    adj_ostream.close();
}

int main(int argc, char * argv[])
{
    try {
        po::options_description desc{"Options"};
        desc.add_options()
            ("help,h", "Help screen")
            ("infile,i", po::value<std::string>(), "input filepath")
            ("outdir,o", po::value<std::string>(), "ouput dir")
            ("sorted,s", "set if input edgelist is sorted by origin vertex")
            ("weighted,w", "graph has floating point edge weight")
            ("make-undirected", "add a reverse edge for each edge in the list")
            ;
        po::variables_map vm;
        po::store(po::parse_command_line(argc,argv,desc), vm);
        po::notify(vm);
        if (vm.count("help"))
            std::cout << desc << std::endl;

        std::string file   = vm["infile"].as<std::string>();
        std::string outdir = vm["outdir"].as<std::string>();
        std::string outdir2 = vm["outdir"].as<std::string>();
        fs::path p(file);
        fs::path p2(outdir.append(p.stem().c_str()));
        fs::path p3(outdir2.append(p.stem().c_str()));
        fs::path index( p2.replace_extension(".idx"));
        fs::path adj(p3.replace_extension(".adj"));

        BOOST_LOG_TRIVIAL(info) << index;
        BOOST_LOG_TRIVIAL(info) << adj;
        
        if (fs::exists(p) && fs::is_regular_file(p)){
            if (vm.count("weighted")){
                encode_weighted(p, index, adj, vm);
            } else{
                encode_unweighted(p, index, adj, vm);
            }
        } else {
            throw "Input file not found";
        }        
        return 0;
    } catch (const std::exception& ex) {
        std::cout << "Caught Runtime Exception: " << ex.what() << std::endl;
        return 1;
    }    
}

#pragma GCC diagnostic pop
