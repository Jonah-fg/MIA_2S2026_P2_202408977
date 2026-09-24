#ifndef JSON_H
#define JSON_H
#include <string>
#include <sstream>
#include <vector>

namespace Json{

    inline std::string escape(const std::string& s) {
        std::ostringstream o;
        for (char c : s){
            switch (c) {
                case '"': 
                     o << "\\\""; 
                     break;

                case '\\':
                    o << "\\\\"; 
                    break;

                case '\n': 
                    o << "\\n"; 
                    break;

                case '\r': 
                    o << "\\r";  
                    break;

                case '\t':
                    o << "\\t"; 
                    break;

                default:
                    if(static_cast<unsigned char>(c)< 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", c);
                        o <<buf;
                    } 
                    else {
                        o<< c;
                    }
            }
        }
        return o.str();
    }

    //construccion objeto JSON simple a partir de pares clave-valor
    inline std::string object(const std::vector<std::pair<std::string,std::string>>& kv, bool rawValues = false) {
        std::ostringstream o;
        o <<"{";
        for (size_t i =0; i<kv.size(); ++i) {
            if (i > 0){
                o << ",";
            }
            o<< "\"" << escape(kv[i].first)<< "\":";
            if (rawValues){
                o << kv[i].second;
            }

            else {
                o<<"\"" << escape(kv[i].second) <<"\"";
            }        
        }
        o << "}";
        return o.str();
    }


    inline std::string array(const std::vector<std::string>& items, bool raw = false) {
        std::ostringstream o;
        o <<"[";
        for(size_t i=0; i <items.size(); ++i) {
            if (i > 0)
                o << ",";
            if (raw){
                o << items[i];
            }
            else  {
                o << "\"" << escape(items[i]) << "\"";
            }  
        }
        o<<"]";
        return o.str();
    }
    inline std::string boolean(bool b)
         {return b ? "true":"false"; }
}
#endif