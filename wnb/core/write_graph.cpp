#include <fstream>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/filtered_graph.hpp>

#include <wnb/core/wordnet.hh>
#include <wnb/core/load_wordnet.hh>
#include <wnb/core/info_helper.hh>

using namespace wnb;

class label_writer {
public:
    label_writer() {}
    void operator()(std::ostream& out, const synset*& syn) const {
      out << "[label=\"" << syn->words->at(0) << "\"]";
    }
};
// filtering functor
template <typename PointerSymbolMap>
struct hyper_edge
{
  hyper_edge() { }

  hyper_edge(PointerSymbolMap pointer_symbol)
    : m_pointer_symbol(pointer_symbol) { }

  // filtering function
  template <typename Edge>
  bool operator()(const Edge& e) const
  {
    int p_s = get(m_pointer_symbol, e);
    return p_s == 1; // hypernyme (instance_hypernyme not used here)
  }

  PointerSymbolMap m_pointer_symbol;
};
typedef boost::property_map<wordnet::graph,
                            int ptr::*>::type PointerSymbolMap;
typedef boost::filtered_graph<wordnet::graph,
                              hyper_edge<PointerSymbolMap> > isaG;

int main(int argc, char const *argv[]) {
    std::string wndir = argv[1];
    std::string out_filename = argv[1];
    wordnet wn(wndir, true);
    hyper_edge<PointerSymbolMap> filter;
    isaG fg(wn.wordnet_graph, filter);
    std::ofstream file(out_filename);
    boost::write_graphviz(file, fg, label_writer());
    return 0;
}
