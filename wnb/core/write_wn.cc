#include "wnb/core/write_wn.hh"


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

namespace wnb{
  namespace
  {
    std::string get_hex(int i){
      std::stringstream ret("");
      if(i < 16)
        ret << 0 << std::hex << i;
      else
        ret << std::hex << i;
      return ret.str();
    }
    // write synset's words
    void write_data_row_words(std::stringstream& ret, const synset*& syn)
    {
      // w_cnt  word  lex_id  [word  lex_id...]
      // w_cnt  [word  lex_id]*N
      assert((int)syn->words->size() == syn->w_cnt);
      ret << get_hex(syn->w_cnt) << " ";
      if(syn->words->size() > syn->lex_ids->size())
         for(int i = syn->words->size() > syn->lex_ids->size(); i > 0; --i)
            syn->lex_ids->push_back(-1);
      for (int i = 0; i < syn->w_cnt; i++)
      {
        //word lex_id
        ret << syn->words->at(i) << " ";
        ret << get_hex(syn->lex_ids->at(i)) << " ";
      }
    }

    // write ptrs
    void write_data_row_ptrs(std::stringstream& ret, const synset*& syn,
                            const wordnet& wn, info_helper& info)
    {
    //   ret << syn.p_cnt << " ";
      std::stringstream temp("");
      int pcount = 0;
      auto oe = boost::out_edges(syn->id, wn.wordnet_graph);
      for(auto oit = oe.first; oit != oe.second; ++oit)
      {
        //http://wordnet.princeton.edu/wordnet/man/wndb.5WN.html#sect3
        //pointer_symbol  synset_offset  pos  source/target
        const synset *t = wn.wordnet_graph[boost::target(*oit, wn.wordnet_graph)];
        ptr edge_prop = wn.wordnet_graph[*oit];
        std::string pointer_symbol_;

        temp << info.symbols[edge_prop.pointer_symbol] << " ";
        temp << t->synset_offset_ << " ";
        temp << t->ss_type_ << " ";
        temp << get_hex(edge_prop.source) << get_hex(edge_prop.target) << " ";
        ++pcount;
      }
      ret << pcount << " ";
      ret << temp.str();
    }


    // write a synset and add it to the wordnet class.
    std::string write_data_row(const wordnet& wn, info_helper& info, const synset* syn)
    {
      //http://wordnet.princeton.edu/wordnet/man/wndb.5WN.html#sect3
      // synset_offset  lex_filenum  ss_type  w_cnt  word  lex_id  [word  lex_id...]  p_cnt  [ptr_symbol synset_ofs pos target]                         [frames...] |   gloss
      // 00001740       03           n        01     entity 0                         003     ~          00001930   n   0000    ~ 00002137 n 0000 ~ 04424418 n 0000 | that which is perceived or known or inferred to have its own distinct existence (living or nonliving)
      // synset synset = wn.wordnet_graph[indice];

    //   printf("synset: %d\n", syn->synset_offset_);
      std::stringstream ret("");
      ret << syn->synset_offset_ << " ";
      ret << syn->lex_filenum << " ";
      ret << syn->ss_type_ << " ";

      // words
      write_data_row_words(ret, syn);

      // ptrs (relations between synsets)
      write_data_row_ptrs(ret, syn, wn, info);

    //   if(syn->synset_offset_ == 74790)
        // std::cout << ret.str() << std::endl;
      //frames (skipped)
      ret << "|";
      // gloss
      ret << syn->gloss;

      return ret.str();
    }


    // Parse data.noun files
    void write_wordnet_data(const std::string& fn, const wordnet& wn, info_helper& info, char pos)
    {
      //   printf("--------------data---------------------\n");
      std::ofstream fout(fn.c_str());
      if (!fout.is_open())
        throw std::runtime_error("File missing: " + fn);

      static const int MAX_LENGTH = 20480;
      char row[MAX_LENGTH];

      //skip header
      for(unsigned i = 0; i < 29; i++)
        fout << "##########" << std::endl;

      //parse data line
      // while (fout.getline(row, MAX_LENGTH))
    //   for(const std::pair<int, int>& p: wn.indice_offset){
      boost::graph_traits<wordnet::graph>::vertex_iterator vi, vi_end;
      std::tie(vi, vi_end) = boost::vertices(wn.wordnet_graph);
      for(; vi != vi_end; ++vi)
      {
        if(wn.wordnet_graph[*vi]->ss_type_ != pos)
          if(!(wn.wordnet_graph[*vi]->ss_type_ == 's' && pos == 'a'))
            continue;
        fout << write_data_row(wn, info, wn.wordnet_graph[*vi]) << std::endl;
      }

      fout.close();
    }


    //FIXME: It seems possible to replace synset_offsets with indice here.
    std::string write_index_row(const index*& idx, const wordnet& wn, info_helper& info)
    {
      // lemma  pos  synset_cnt  p_cnt  [ptr_symbol...]  sense_cnt  tagsense_cnt   synset_offset  [synset_offset...]
      // bank   n    10          5      @ ~ #m %p +      10         4              09213565       08420278 09213434 08462066 13368318 13356402 09213828 04139859 02787772 00169305
      std::stringstream ret("");
      ret << idx->lemma << " ";
      ret << idx->pos_ << " ";
      ret << idx->synset_cnt << " ";
      ret << idx->p_cnt << " ";

      std::string tmp_p;
      for (int i = 0; i < idx->p_cnt; i++)
        ret << idx->ptr_symbols[i] << " ";

      ret << idx->sense_cnt << " ";
      ret << idx->tagsense_cnt;

      int tmp_o;
      // while (srow >> tmp_o)
      for(const int& i: idx->synset_offsets)
        ret << " " << i;

      return ret.str();
    }


    void write_wordnet_index(const std::string& fn, const wordnet& wn, info_helper& info, char pos)
    {
      std::ofstream fout(fn.c_str());
      if (!fout.is_open())
        throw std::runtime_error("File Not Found: " + fn);

      static const int MAX_LENGTH = 20480;
      char row[MAX_LENGTH];

      //skip header
      for(unsigned i = 0; i < 29; i++)
        fout << "##########" << std::endl;

      //parse data line
      // while (fout.getline(row, MAX_LENGTH))
      for(const index* idx: wn.index_list){
        if(idx->pos_ != pos)
          if(!(idx->pos_ == 's' && pos == 'a'))
            continue;
        fout << write_index_row(idx, wn, info) << std::endl;
      }

      fout.close();
    }


    // we should only copying original exc file
    // void write_wordnet_exc(const std::string& dn, std::string cat,
    //                       wordnet& wn, info_helper&)
    // {
    //   std::string fn = dn + cat + ".exc";
    //   std::ofstream fout(fn.c_str());
    //   if (!fout.is_open())
    //     throw std::runtime_error("File Not Found: " + fn);

    //   std::map<std::string,std::string>& exc = wn.exc[get_pos_from_name(cat)];

    //   std::string row;

    //   std::string key, value;
    //   while (std::getline(fout, row))
    //   {
    //     std::stringstream srow(row);
    //     srow >> key;
    //     srow >> value;

    //     exc[key] = value;
    //   }
    // }

    void write_wordnet_cat(const std::string dn, std::string cat,
                          const wordnet& wn, info_helper& info, char pos)
    {
      write_wordnet_data((dn + "data." + cat), wn, info, pos);
      write_wordnet_index((dn + "index." + cat), wn, info, pos);
      // write_wordnet_exc(dn, cat, wn, info);
    }

    void write_wordnet_index_sense(const std::string& dn, const wordnet& wn, info_helper& info)
    {
      // sense-key          synset-offset sense-number tag_cnt
      // backhand%1:04:00:: 00566690      1            0
      std::string fn = dn + "index.sense";
      std::ofstream fout(fn.c_str());
      if (!fout.is_open())
        throw std::runtime_error("File Not Found: " + fn);

      // while (std::getline(fout, row))
      for(auto it = wn.sensekey_indice.begin(); it != wn.sensekey_indice.end(); ++it)
      {
        const synset* syn = wn.wordnet_graph[it->second];
        assert(syn->id == it->second);
        fout << it->first << " " << syn->synset_offset_ << " ";
        fout << syn->sense_number << " " << syn->tag_cnt << std::endl;
      }
      fout.close();
    }

  } // end of anonymous namespace

  void write_wordnet(const wordnet& wn, const std::string& dn, info_helper& info)
  {
    // vertex added in this order a n r v

    std::string fn = dn;

    if (wn._verbose)
    {
      std::cout << std::endl;
      std::cout << "### writeing Wordnet 3.0";
      boost::progress_display show_progress(5);
      boost::progress_timer t;

      write_wordnet_cat(dn, "adj", wn, info, 'a');
      ++show_progress;
      write_wordnet_cat(dn, "noun", wn, info, 'n');
      ++show_progress;
      write_wordnet_cat(dn, "adv", wn, info, 'r');
      ++show_progress;
      write_wordnet_cat(dn, "verb", wn, info, 'v');
      ++show_progress;
      write_wordnet_index_sense(dn, wn, info);
      ++show_progress;
      std::cout << std::endl;
    }
    else
    {
      write_wordnet_cat(dn, "adj", wn, info, 'a');
      write_wordnet_cat(dn, "noun", wn, info, 'n');
      write_wordnet_cat(dn, "adv", wn, info, 'r');
      write_wordnet_cat(dn, "verb", wn, info, 'v');
      write_wordnet_index_sense(dn, wn, info);
    }

    // std::stable_sort(wn.index_list.begin(), wn.index_list.end());
  }

}
