#include <wnb/core/wordnet.hh>
#include <wnb/std_ext.hh>

#include <string>
#include <fstream>
#include <set>
// #include <unordered_set>
#include <algorithm>
#include <stdexcept>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/filtered_graph.hpp>

namespace wnb
{
    auto split1 = [](const std::string& s, const char& delim){
        std::istringstream iss(s);
        std::string ret;
        std::getline(iss, ret, delim);
        return std::move(ret);
    };
  //FIXME: Make (smart) use of fs::path
  wordnet::wordnet(const std::string& wordnet_dir, bool verbose)
    : _verbose(verbose)
  {
    wordnet& wn = *this;

    if (_verbose)
    {
      std::cout << wordnet_dir << std::endl;
    }

    info = preprocess_wordnet(wordnet_dir);

    wordnet_graph = graph(info.nb_synsets());
    load_wordnet(wordnet_dir, wn, info);

    if (_verbose)
    {
      std::cout << "nb_synsets: " << info.nb_synsets() << std::endl;
    }
    //FIXME: this check is only valid for Wordnet 3.0
    //assert(info.nb_synsets() == 142335);//117659);
    //assert(info.nb_synsets() == 117659);
  }

  // bool
  // wordnet::__exist_node(const graph& G,
  //                       const int indice)
  //   {
  //   boost::graph_traits<graph>::vertex_iterator it;
  //   boost::graph_traits<graph>::vertex_iterator end;
  //   boost::tie(it, end) = boost::vertices(G);
  //   std::find_if(it, end, [&](const synset node){ return g[*it]->id == indice;});
  //   return (it != end);
  // }

  void
  wordnet::_merge(const std::vector<int>& indices, std::unordered_set<int>& removed_v)
  {
    typedef boost::graph_traits<wordnet::graph>::vertex_descriptor vertex;
    if(indices.size() < 2) return;
    synset* sa = wordnet_graph[indices[0]];
    vertex a = sa->id;
    boost::graph_traits<graph>::vertex_iterator _begin, _end;
    boost::graph_traits<graph>::vertex_iterator _match;
    int a_offset = sa->synset_offset_;
    for(size_t i = 1; i < indices.size(); i++){
      synset* sb = wordnet_graph[indices[i]];
      // TODO: Is there a need to add "sb"'s gloss to "sa" ?
      sa->gloss += sb->gloss;
      const std::vector<std::string>& words = *(sb->words);
      const pos_t& pos = sb->pos;
      const int offset = sb->synset_offset_;
      // remove synset from index
      for(const std::string& w: words){
        auto it = index_map.find(std::pair<std::string, pos_t>(w, pos));
        if(it == index_map.end()) continue;
        index *idx = it->second;
        // if sb->synset_offset_ is in index of "w", erase offset from index
        for(auto _it = idx->synset_offsets.begin(); _it != idx->synset_offsets.end(); _it++){
          if(*_it == offset){
            _it = idx->synset_offsets.erase(_it);
            idx->synset_cnt--;
            idx->sense_cnt--;
            break;
          }
        }
        // if a_offset is not in 'w'-index , push_back a_offset
        auto iter = find(idx->synset_offsets.begin(), idx->synset_offsets.end(), a_offset);
        if(iter == idx->synset_offsets.end()) idx->synset_offsets.push_back(a_offset);
        // merge words list
        if(find(sa->words->begin(), sa->words->end(), w) == sa->words->end()){
          sa->words->push_back(w);
          sa->lex_ids->push_back(idx->synset_offsets.size());
          sa->w_cnt++;
        }
        // merge sensekeys
        for(std::string& skey: *(sb->_sensekeys))
        {
            std::string lem_ =  split1(skey, '%');
            if(sa->skeyset.find(lem_) == sa->skeyset.end())
            {
                sa->skeyset.insert(lem_);
                sa->_sensekeys->push_back(skey);
            }
        }
      }
      // remove synset end.

      // copying b's edge to a
      vertex b = sb->id;
      ptr temptr;
      auto ie = boost::in_edges(b, wordnet_graph);
      for(auto eit = ie.first; eit != ie.second; ++eit){
        temptr = wordnet_graph[*eit];
        if(source(*eit, wordnet_graph) == a) continue;
        boost::add_edge(source(*eit, wordnet_graph), a, std::move(temptr), wordnet_graph);
      }
      auto oe = boost::out_edges(b, wordnet_graph);
      for(auto oit = oe.first; oit != oe.second; ++oit){
        temptr = wordnet_graph[*oit];
        if(target(*oit, wordnet_graph) == a) continue;
        boost::add_edge(a, target(*oit, wordnet_graph), std::move(temptr), wordnet_graph);
        sa->p_cnt++;
      }

      // printf("now deleting\n");
      if(indices[i] == 77668){
        oe = boost::out_edges(b, wordnet_graph);
        for(auto oit = oe.first; oit != oe.second; ++oit){
          temptr = wordnet_graph[*oit];
          // std::cout << "ptr:" << temptr.pointer_symbol << " " << temptr.source << " " << temptr.target << std::endl;
          // std::cout << "target: " << wordnet_graph[boost::target(*oit, wordnet_graph)].words[0] << std::endl;
        }
      }
      // boost::remove_out_edge_if(b, all_true, wordnet_graph);
      boost::clear_vertex(b, wordnet_graph);
      // printf("deleting done!\n");
      // boost::remove_in_edge_if(b, all_true, wordnet_graph);
      // ie = boost::in_edges(b, wordnet_graph);
      // for(auto eit = ie.first; eit != ie.second; ++eit)
      //   boost::remove_edge(*eit, wordnet_graph);

      // std::vector<boost::graph_traits<graph>::edge_descriptor> edv;
      // oe = boost::out_edges(b, wordnet_graph);
      // for(auto oit = oe.first; oit != oe.second; ++oit)
      //    edv.push_back(*oit);
      // for(auto& ed: edv)
      //    boost::remove_edge(ed, wordnet_graph);

      // boost::remove_vertex(b, wordnet_graph);
      removed_v.insert(indices[i]);
    }
    return;
  }

  void
  wordnet::merge_synsets(const std::string& clusterfile)
  {
    std::fstream fin2(clusterfile);
    if (!fin2.is_open())
      throw std::runtime_error("wordnet::merge_synsets : File Not Found: " + clusterfile);
    std::string s;
    int count = 0;
    while(fin2 >> s)
    {
       ++count;
    }
    std::string row;
    std::string sense_key;
    std::unordered_set<int> removed_v;
    removed_v.reserve(count);
    std::vector<int> cluster;
    cluster.reserve(10);
    std::fstream fin(clusterfile);
    while(std::getline(fin, row)) {
      std::stringstream srow(row);
      // cluster store the synset's indice value
      cluster.clear();
      while(srow >> sense_key){
        if(sensekey_indice.find(sense_key) == sensekey_indice.end()) continue;
        // cluster.push_back(sensekey_indice[sense_key]);
        cluster.push_back(sensekey_indice[sense_key]);
        // replace sensekey_indice
        if(cluster.size() > 1) sensekey_indice[sense_key] = cluster[0];
        // std::cout << sensekey_indice[sense_key] << std::endl;
      }
      // std::cout << std::endl;
      _merge(cluster, removed_v);
    }
    // erase removed synset from indice_offset
    /*
    for(auto it = indice_offset.begin(); it != indice_offset.end(); )
    {
      if(removed_v.find(it->second) != removed_v.end())
        it = indice_offset.erase(it);
      else
        ++it;
    }
    */
    return;
  }

  std::vector<synset*>
  wordnet::get_synsets(const std::string& word)
  {
    typedef std::vector<index> vi;
    std::vector<synset*> synsets;

    index light_index;
    light_index.lemma = word;

    static const unsigned nb_pos = POS_ARRAY_SIZE-1;
    for (unsigned p = 1; p <= nb_pos; p++)
    {
      std::vector<synset*> res = get_synsets(word, (pos_t)p);
      synsets.insert(synsets.end(), res.begin(), res.end());
    }

    return synsets;
  }

  std::vector<synset*>
  wordnet::get_synsets(const std::string& word, const std::string& pos){
    return get_synsets(word, wnb::get_pos_from_name(pos));
  }

  std::vector<synset*>
  wordnet::get_synsets(const std::string& word, pos_t pos)
  {
    std::vector<synset*> synsets;

    // morphing
    std::string mword = morphword(word, pos);
    if (mword == "")
      return synsets;

    // binary_search
    // index light_index;
    // light_index.lemma = mword; //(mword != "") ? mword : word;
    // typedef std::vector<index> vi;

    index* idx = index_map[std::pair<std::string, pos_t>(mword, pos)];
    // std::pair<vi::iterator,vi::iterator> bounds =
      // std::equal_range(index_list.begin(), index_list.end(), light_index);
    for (unsigned i = 0; i < idx->synset_offsets.size(); i++)
    {
      int offset = idx->synset_offsets[i];
      // if (idx->pos == pos) // FIXME: one index_list by pos ?
      {
        int u = info.compute_indice(offset, idx->pos);
        synsets.push_back(wordnet_graph[u]);
      }
    }
    // vi::iterator it;
    // for (it = bounds.first; it != bounds.second; it++)
    // {
    //   for (unsigned i = 0; i < it->synset_offsets.size(); i++)
    //   {
    //     int offset = it->synset_offsets[i];
    //     if (it->pos == pos) // FIXME: one index_list by pos ?
    //     {
    //       int u = info.compute_indice(offset, it->pos);
    //       synsets.push_back(wordnet_graph[u]);
    //     }
    //   }
    // }

    return synsets;
  }

  std::string
  wordnet::wordbase(const std::string& word, int ender)
  {
    if (ext::ends_with(word, info.sufx[ender]))
    {
      int sufxlen = info.sufx[ender].size();
      std::string strOut = word.substr(0, word.size() - sufxlen);
      if (!info.addr[ender].empty())
        strOut += info.addr[ender];
      return strOut;
    }
    return word;
  }

  bool is_defined(const std::string&, pos_t)
  {
    throw "Not Implemented";
  }

  // Try to find baseform (lemma) of individual word in POS
  std::string
  wordnet::morphword(const std::string& word, pos_t pos)
  {
    // first look for word on exception list
    exc_t::iterator it = exc[pos].find(word);
    if (it != exc[pos].end())
      return it->second; // found in exception list

    std::string tmpbuf;
    std::string end;
    int cnt = 0;

    if (pos == R)
      return ""; // Only use exception list for adverbs

    if (pos == N)
    {
      if (ext::ends_with(word, "ful"))
      {
        cnt = word.size() - 3;
        tmpbuf = word.substr(0, cnt);
        end = "ful";
      }
      else
      {
        // check for noun ending with 'ss' or short words
        if (ext::ends_with(word, "ss") || word.size() <= 2)
          return "";
      }
    }

    // If not in exception list, try applying rules from tables

    if (tmpbuf.size() == 0)
      tmpbuf = word;

    //offset = offsets[pos];
    //cnt = cnts[pos];

    int offset  = info.offsets[pos];
    int pos_cnt = info.cnts[pos];

    std::string morphed;
    for  (int i = 0; i < pos_cnt; i++)
    {
      morphed = wordbase(tmpbuf, (i + offset));
  	  if (morphed != tmpbuf) // && is_defined(morphed, pos))
        return morphed + end;
    }

    return morphed;
  }

} // end of namespace wnb
