#include "wordnet.hh"
#include <fstream>
#include <boost/algorithm/string.hpp>

std::string wordnetdir = "/home/lr/suga/local/src/WordNet-3.0/dict/";

int main(int argc, char* argv[]){
    std::string fn(argv[1]);
    std::ifstream in(fn);
    std::string s;
    std::vector<std::string> v;
    std::vector<std::string> v2;
    wnb::wordnet WN{wordnetdir};
    while(std::getline(in, s)) {
        boost::split(v, s, boost::is_space());
        for(auto& w: v){
            boost::split(v2, w, boost::is_any_of("@"));
            std::string now;
            if(v2.size() < 2){
                std::cout << w << " ";
                continue;
            }
            else now = WN.morphword(v2.at(0), wnb::get_pos_from_name(v2.at(1)));
            if(now == "") now = v2.at(0);
            std::cout << now << "@" << v2.at(1) << " ";
        }
        std::cout << std::endl;
    }

}
