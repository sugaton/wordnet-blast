#ifndef _WRITE_WN_HH
# define _WRITE_WN_HH

# include "info_helper.hh"

namespace wnb{
   struct wordnet;
   void write_wordnet(const wordnet& wn, const std::string& dn, info_helper& info);
}
#endif