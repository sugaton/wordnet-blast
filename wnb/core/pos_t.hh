#ifndef _POS_T_HH
# define _POS_T_HH

namespace wnb
{

  static const std::size_t POS_ARRAY_SIZE = 6;
  static const char POS_ARRAY[POS_ARRAY_SIZE] = {'u', 'n', 'v', 'a', 'r', 's'};

  enum pos_t
  	{
      UNKNOWN = 0,
      N       = 1,
      V       = 2,
      A       = 3,
      R       = 4,
      S       = 5,
  	};


  inline pos_t get_pos_from_name(const std::string& pos)
  {
    if (pos == "adj" || pos == "ADJ")
      return A;
    if (pos == "noun" || pos == "NOUN")
      return N;
    if (pos == "adv" || pos == "ADV")
      return R;
    if (pos == "verb" || pos == "VERB")
      return V;
    return UNKNOWN;
  }

  inline std::string get_name_from_pos(const pos_t& pos)
  {
    switch (pos)
    {
    case A: return "adj";
    case N: return "noun";
    case R: return "adv";
    case V: return "verb";
    default: return "UNKNOWN";
    }
  }

} // end of namespace wncpp


#endif /* _POS_T_HH */

