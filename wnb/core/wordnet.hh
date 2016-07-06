#ifndef _WORDNET_HH
# define _WORDNET_HH

# include <iostream>
# include <string>
# include <cassert>
# include <vector>
#include <unordered_set>
# include <map>
//#include <unordered_map>
//# include <boost/filesystem.hpp>

//Possible https://bugs.launchpad.net/ubuntu/+source/boost/+bug/270873
# include <boost/graph/graph_traits.hpp>
# include <boost/graph/adjacency_list.hpp>

# include "wnb/core/load_wordnet.hh"
# include "wnb/core/pos_t.hh"

namespace wnb
{

  /// More info here: http://wordnet.princeton.edu/wordnet/man/wndb.5WN.html

  struct info_helper;

  /// Synset
  struct synset
  {
    int  synset_offset_;  ///< ***Deprecated*** @deprecated in-file wordnet
    int  lex_filenum;
    char ss_type_;       ///< ***Deprecated*** @deprecated unsafe pos
    int  w_cnt;
    std::vector<std::string>* words;
    std::vector<int>* lex_ids;
    std::vector<std::string>* _sensekeys;
    std::unordered_set<std::string> skeyset;
    int p_cnt;
    std::string gloss;

    // extra
    pos_t pos;        ///< pos
    int id;           ///< unique identifier
    int sense_number; ///< http://wordnet.princeton.edu/man/senseidx.5WN.html
    int tag_cnt;      ///< http://wordnet.princeton.edu/man/senseidx.5WN.html

    bool operator==(const synset& s) const { return (id == s.id);  }
    bool operator<(const synset& s) const { return (id < s.id);   }
    synset()
    {
        words = new std::vector<std::string>();
        lex_ids = new std::vector<int>();
        _sensekeys = new std::vector<std::string>();
    };
    ~synset(){ delete words; delete lex_ids; delete _sensekeys;}
  };


  /// Rel between synsets properties
  struct ptr
  {
    //std::string pointer_symbol; ///< symbol of the relation
    int pointer_symbol;
    int source; ///< source word inside synset
    int target; ///< target word inside synset
    ptr() {}
  };


  /// Index
  struct index
  {
    std::string lemma;
    char  pos_;        ///< ***Deprecated*** @deprecated unsafe pos
    int   synset_cnt;
    int   p_cnt;
    int   sense_cnt;
    float tagsense_cnt;
    std::vector<std::string> ptr_symbols;
    std::vector<int>         synset_offsets;

    // extra
    std::vector<int> ids;
    pos_t pos;

    bool operator<(const index& b) const
    {
      return (lemma.compare(b.lemma) < 0);
    }
    index() {};
  };

  template <class edge_descriptor>
  struct all_edge_true {
    bool operator()(const edge_descriptor& e) const {
      return true;
    }
  };

  /// Wordnet interface class
  struct wordnet
  {
    typedef boost::adjacency_list<boost::listS, boost::vecS,
                                  boost::bidirectionalS,
                                  synset*, ptr> graph; ///< boost graph type

    typedef all_edge_true<boost::graph_traits<graph>::edge_descriptor> all_E_true;
    /// Constructor
    wordnet(const std::string& wordnet_dir, bool verbose=false);

    void _merge(const std::vector<int>& indices, std::unordered_set<int>& removed_v);

    void merge_synsets(const std::string& clusterfile);
    /// Return synsets matching word
    std::vector<synset*> get_synsets(const std::string& word);

    std::vector<synset*> get_synsets(const std::string& word, const std::string& pos);

    std::vector<synset*> get_synsets(const std::string& word, pos_t pos);

    //FIXME: todo
    std::vector<synset*> get_synset(const std::string& word, char pos, int i);

    std::string wordbase(const std::string& word, int ender);

    std::string morphword(const std::string& word, pos_t pos);

    std::vector<index*> index_list;    ///< index list // FIXME: use a map
    // std::unordered_map<std::pair<std::string, pos_t>, index*> index_map;    ///< index list // FIXME: use a map
    std::map<std::pair<std::string, pos_t>, index*> index_map;    ///< index list // FIXME: use a map
    // std::unordered_map<std::string, int> sensekey_indice;    ///< index list // FIXME: use a map
    std::map<std::string, int> sensekey_indice;    ///< index list // FIXME: use a map
    std::map<int, std::vector<std::string>> indice_sensekeys;
    // std::map<int, int> indice_offset;
    graph              wordnet_graph; ///< synsets graph
    info_helper        info;          ///< helper object
    all_E_true         all_true;
    bool               _verbose;

    typedef std::map<std::string,std::string> exc_t;
    std::map<pos_t, exc_t> exc;
    ~wordnet()
    {
        index_map.clear();
        for(auto& ip: index_list) delete ip;
        index_list.clear();
        boost::graph_traits<graph>::vertex_iterator vi, vi_end, next;
        std::tie(vi, vi_end) = boost::vertices(wordnet_graph);
        for(next = vi; vi != vi_end; vi = next)
        {
            ++next;
            delete wordnet_graph[*vi];
        }
    };
  };

} // end of namespace wnb

#endif /* _WORDNET_HH */
