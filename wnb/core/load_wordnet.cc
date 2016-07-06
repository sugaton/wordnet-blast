#include "load_wordnet.hh"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

#include <boost/graph/adjacency_list.hpp>
#include <boost/progress.hpp>
#include <boost/algorithm/string.hpp>

#include <wnb/std_ext.hh>

#include "wordnet.hh"
#include "info_helper.hh"
#include "pos_t.hh"

namespace bg = boost::graph;

namespace wnb
{

  namespace
  {

    // Load synset's words
    void load_data_row_words(std::stringstream& srow, synset*& syn)
    {
      // w_cnt  word  lex_id  [word  lex_id...]
      // w_cnt  [word  lex_id]*N
      srow >> std::hex >> syn->w_cnt >> std::dec;
      for (int i = 0; i < syn->w_cnt; i++)
      {
        //word lex_id

        std::string word;
        srow >> word;
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        syn->words->push_back(ext::split(word,'(').at(0));

        int lex_id = 0;
        srow >> std::hex >> lex_id >> std::dec;
        syn->lex_ids->push_back(lex_id);
      }
      assert(syn->words->size() == syn->lex_ids->size());
      assert(syn->words->size() == syn->w_cnt);
    }

    // Add rel to graph
    void add_wordnet_rel(std::string& pointer_symbol_,// type of relation
                         int synset_offset,           // dest offset
                         pos_t pos,                   // p.o.s. of dest
                         int src,                     // word src
                         int trgt,                    // word target
                         synset*& syn,              // source synset
                         wordnet& wn,                 // our wordnet
                         info_helper& info)           // helper
    {
      //if (pos == S || synset.pos == S)
      //  return; //FIXME: check where are s synsets.

      int u = syn->id;
      int v = info.compute_indice(synset_offset, pos);

      ptr p;
      p.pointer_symbol = info.get_symbol(pointer_symbol_);
      p.source = src;
      p.target = trgt;

      boost::add_edge(u,v, p, wn.wordnet_graph);
    }


    // load ptrs
    void load_data_row_ptrs(std::stringstream& srow, synset*& syn,
                            wordnet& wn, info_helper& info)
    {
      srow >> syn->p_cnt;
      for (int i = 0; i < syn->p_cnt; i++)
      {
        //http://wordnet.princeton.edu/wordnet/man/wndb.5WN.html#sect3
        //pointer_symbol  synset_offset  pos  source/target
        std::string pointer_symbol_;
        int  synset_offset;
        pos_t pos;
        int   src;
        int   trgt;

        srow >> pointer_symbol_;
        srow >> synset_offset;

        char c;
        srow >> c;
        pos = info.get_pos(c);

        //print extracted edges
        //std::cout << "(" << pointer_symbol << ", " << synset_offset;
        //std::cout << ", " << pos << ")" << std::end;

        // Extract source/target words info
        std::string src_trgt;
        srow >> src_trgt;
        std::stringstream ssrc(std::string(src_trgt,0,2));
        std::stringstream strgt(std::string(src_trgt,2,2));
        ssrc >> std::hex >> src >> std::dec;
        strgt >> std::hex >> trgt >> std::dec;

        add_wordnet_rel(pointer_symbol_, synset_offset, pos, src, trgt, syn, wn, info);
      }
    }


    // Load a synset and add it to the wordnet class.
    void load_data_row(const std::string& row, wordnet& wn, info_helper& info)
    {
      //http://wordnet.princeton.edu/wordnet/man/wndb.5WN.html#sect3
      // synset_offset  lex_filenum  ss_type  w_cnt [word  lex_id] [word  lex_id...]  p_cnt  [ptr_symbol synset_ofs pos target]                         [frames...] |   gloss
      // 00001740       03           n        01     entity  0                         003     ~          00001930   n   0000    ~ 00002137 n 0000 ~ 04424418 n 0000 | that which is perceived or known or inferred to have its own distinct existence (living or nonliving)
      synset* syn = new synset();

      std::stringstream srow(row);
      srow >> syn->synset_offset_;
      srow >> syn->lex_filenum;
      srow >> syn->ss_type_;

    //   std::cout << syn->synset_offset_ << ":" << syn->lex_filenum << ":" << syn->ss_type_ << std::endl;
      // extra information
      syn->pos = info.get_pos(syn->ss_type_);

      syn->id  = info.compute_indice(syn->synset_offset_, syn->pos);

      // words
      load_data_row_words(srow, syn);

      // ptrs (relations between synsets)
      load_data_row_ptrs(srow, syn, wn, info);

      //frames (skipped)
      std::string tmp;
      while (srow >> tmp)
        if (tmp == "|")
          break;

      // gloss
      std::getline(srow, syn->gloss);

      // Add synset to graph
      wn.wordnet_graph[syn->id] = syn;
    }


    // Parse data.noun files
    void load_wordnet_data(const std::string& fn, wordnet& wn, info_helper& info)
    {
      std::ifstream fin(fn.c_str());
      if (!fin.is_open())
        throw std::runtime_error("File missing: " + fn);

      static const int MAX_LENGTH = 20480;
      char row[MAX_LENGTH];

      //skip header
      for(unsigned i = 0; i < 29; i++)
        fin.getline(row, MAX_LENGTH);

      //parse data line
      while (fin.getline(row, MAX_LENGTH))
        load_data_row(row, wn, info);

      fin.close();
    }


    //FIXME: It seems possible to replace synset_offsets with indice here.
    void load_index_row(const std::string& row, wordnet& wn, info_helper& info)
    {
      // lemma  pos  synset_cnt  p_cnt  [ptr_symbol...]  sense_cnt  tagsense_cnt   synset_offset  [synset_offset...]
      // bank   n    10          5      @ ~ #m %p +      10         4              09213565       08420278 09213434 08462066 13368318 13356402 09213828 04139859 02787772 00169305
      index* idx = new index();
      std::stringstream srow(row);

      srow >> idx->lemma;
      srow >> idx->pos_;
      srow >> idx->synset_cnt;
      srow >> idx->p_cnt;

      std::string tmp_p;
      for (int i = 0; i < idx->p_cnt; i++)
      {
        srow >> tmp_p;
        idx->ptr_symbols.push_back(tmp_p);
      }
      srow >> idx->sense_cnt;
      srow >> idx->tagsense_cnt;

      int tmp_o;
      while (srow >> tmp_o)
        idx->synset_offsets.push_back(tmp_o);

      //extra data
      idx->pos = info.get_pos(idx->pos_);

      //add synset to index list
      wn.index_list.push_back(idx);
      wn.index_map[std::pair<std::string, pos_t>(idx->lemma, idx->pos)] = idx;
    }


    void load_wordnet_index(const std::string& fn, wordnet& wn, info_helper& info)
    {
      std::ifstream fin(fn.c_str());
      if (!fin.is_open())
        throw std::runtime_error("File Not Found: " + fn);

      static const int MAX_LENGTH = 20480;
      char row[MAX_LENGTH];

      //skip header
      for(unsigned i = 0; i < 29; i++)
        fin.getline(row, MAX_LENGTH);

      //parse data line
      while (fin.getline(row, MAX_LENGTH))
        load_index_row(row, wn, info);

      fin.close();
    }


    void load_wordnet_exc(const std::string& dn, std::string cat,
                          wordnet& wn, info_helper&)
    {
      std::string fn = dn + cat + ".exc";
      std::ifstream fin(fn.c_str());
      if (!fin.is_open())
        throw std::runtime_error("File Not Found: " + fn);

      std::map<std::string,std::string>& exc = wn.exc[get_pos_from_name(cat)];

      std::string row;

      std::string key, value;
      while (std::getline(fin, row))
      {
        std::stringstream srow(row);
        srow >> key;
        srow >> value;

        exc[key] = value;
      }
      fin.close();
    }

    void load_wordnet_cat(const std::string dn, std::string cat,
                          wordnet& wn, info_helper& info)
    {
      load_wordnet_data((dn + "data." + cat), wn, info);
      load_wordnet_index((dn + "index." + cat), wn, info);
      load_wordnet_exc(dn, cat, wn, info);
    }

    // FIXME: this file is not found in any packaged version of wordnet
    void load_wordnet_index_sense(const std::string& dn, wordnet& wn, info_helper& info)
    {
      std::string fn = dn + "index.sense";
      std::ifstream fin(fn.c_str());
      if (!fin.is_open())
        throw std::runtime_error("File Not Found: " + fn);

      std::string row;
      std::string sense_key;
      int synset_offset;
      while (std::getline(fin, row))
      {
        std::stringstream srow(row);
        srow >> sense_key;

        // Get the pos of the lemma
        std::stringstream tmp(ext::split(ext::split(sense_key,'%').at(1), ':').at(0));
        std::string lemma(ext::split(sense_key,'%').at(0));
        int ss_type;
        tmp >> ss_type;
        pos_t pos = (pos_t) ss_type;


        srow >> synset_offset;

        // Update synset info
        int u = info.compute_indice(synset_offset, pos);
        wn.sensekey_indice[sense_key] = u;
        // wn.indice_offset[u] = synset_offset;
        // std::vector<std::string>& words = wn.wordnet_graph[u].words;
        // assert(std::find(words.begin(), words.end(), lemma) != words.end());
        if(wn.wordnet_graph[u]->synset_offset_ != synset_offset)
        {
            if(std::find(wn.wordnet_graph[u]->words->begin(), wn.wordnet_graph[u]->words->end(), lemma) == wn.wordnet_graph[u]->words->end())
            {
                printf("error: not matched the word");
                std::cout << lemma << std::endl;
            }
          printf("\n sense_key : %s \n \tconfilict %d : %d\n", sense_key.c_str(), wn.wordnet_graph[u]->synset_offset_ , synset_offset);
        }
        //assert(wn.wordnet_graph[u]->pos == pos);
        if(wn.wordnet_graph[u]->id == 0){
          assert(wn.wordnet_graph[u]->id == u);
          assert(wn.wordnet_graph[u]->synset_offset_ == 1740);
        }
        wn.wordnet_graph[u]->_sensekeys->push_back(sense_key);
        if(wn.indice_sensekeys.find(u) == wn.indice_sensekeys.end())
        {
            wn.indice_sensekeys[u] = std::vector<std::string>();
        }
        wn.indice_sensekeys[u].push_back(sense_key);

        srow >> wn.wordnet_graph[u]->sense_number;
        srow >> wn.wordnet_graph[u]->tag_cnt;
      }
      fin.close();
    }

  } // end of anonymous namespace

  void load_wordnet(const std::string& dn, wordnet& wn, info_helper& info)
  {
    // vertex added in this order a n r v

    std::string fn = dn;

    if (wn._verbose)
    {
      std::cout << std::endl;
      std::cout << "### Loading Wordnet 3.0";
      boost::progress_display show_progress(5);
      boost::progress_timer t;

      load_wordnet_cat(dn, "adj", wn, info);
      ++show_progress;
      load_wordnet_cat(dn, "noun", wn, info);
      ++show_progress;
      load_wordnet_cat(dn, "adv", wn, info);
      ++show_progress;
      load_wordnet_cat(dn, "verb", wn, info);
      ++show_progress;
      load_wordnet_index_sense(dn, wn, info);
      ++show_progress;
      std::cout << std::endl;
    }
    else
    {
      load_wordnet_cat(dn, "adj", wn, info);
      load_wordnet_cat(dn, "noun", wn, info);
      load_wordnet_cat(dn, "adv", wn, info);
      load_wordnet_cat(dn, "verb", wn, info);
      load_wordnet_index_sense(dn, wn, info);
    }

    std::stable_sort(wn.index_list.begin(), wn.index_list.end());
  }

} // end of namespace wnb
