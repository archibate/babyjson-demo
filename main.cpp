#include <variant>
#include <vector>
#include <unordered_map>
#include <string>
#include <string_view>
#include <optional>
#include <regex>
#include <charconv>
#include "print.h"
#include <fstream>

struct JSONObject;

using JSONDict = std::unordered_map<std::string, JSONObject>;
using JSONList = std::vector<JSONObject>;

struct JSONObject {
    std::variant
    < std::monostate  // error type
    , std::nullptr_t  // null
    , bool            // true
    , int             // 42
    , double          // 3.14
    , std::string     // "hello"
    , JSONList        // [42, "hello"]
    , JSONDict        // {"hello": 985, "world": 211}
    > inner;

    void do_print() const {
        printnl(inner);
    }

    template <class T>
    bool is() const {
        return std::holds_alternative<T>(inner);
    }

    template <class T>
    T const &get() const {
        return std::get<T>(inner);
    }

    template <class T>
    T &get() {
        return std::get<T>(inner);
    }
};

std::string read_file(std::string const& file_path){
    std::ifstream input_file(file_path);
    return std::move(std::string(std::istreambuf_iterator<char>(input_file),std::istreambuf_iterator<char>()));
}

template <class T>
std::optional<T> try_parse_num(std::string_view str) {
    T value;
    auto res = std::from_chars(str.data(), str.data() + str.size(), value);
    if (res.ec == std::errc() && res.ptr == str.data() + str.size()) {
        return value;
    }
    return std::nullopt;
}

char unescaped_char(char c) {
    switch (c) {
    case 'n': return '\n';
    case 'r': return '\r';
    case '0': return '\0';
    case 't': return '\t';
    case 'v': return '\v';
    case 'f': return '\f';
    case 'b': return '\b';
    case 'a': return '\a';
    default: return c;
    }
}

std::pair<std::string,size_t> try_parse_string(std::string_view json, char queto){
    enum {
        Raw,
        Escaped,
        Hex1,
        Hex2
    } phase = Raw;
    size_t i;
    std::string str;
    char hex_ch{};
    for (i = 1; i < json.size(); i++) {
        char ch = json[i];
        if (phase == Raw) {
            if (ch == '\\') {
                phase = Escaped;
            } else if (ch == queto) {
                i += 1;
                break;
            } else {
                str += ch;
            }
        } else if (phase == Escaped) {
            if(ch=='x'){
                phase = Hex1;
            }
            else{
                str += unescaped_char(ch);
                phase = Raw;
            }
        } else if (phase == Hex1){
            if((ch>='0'&&ch<='9') || (ch>='a'&&ch<='f') || (ch>='A'&&ch<='F')){
                if(ch>='0'&&ch<='9'){
                    hex_ch += ch - '0';
                }
                else if(ch>='a'&&ch<='f'){
                    hex_ch += ch - 'a' + 10;
                }
                else if(ch>='A'&&ch<='F'){
                    hex_ch += ch - 'A' + 10;
                }
                phase = Hex2;
            }
            else{
                i--;
                hex_ch = '\0';
                phase = Raw;
            }
        } else if (phase == Hex2){
            if((ch>='0'&&ch<='9') || (ch>='a'&&ch<='f') || (ch>='A'&&ch<='F')){
                hex_ch *= 16;
                if(ch>='0'&&ch<='9'){
                    hex_ch += ch - '0';
                }
                else if(ch>='a'&&ch<='f'){
                    hex_ch += ch - 'a' + 10;
                }
                else if(ch>='A'&&ch<='F'){
                    hex_ch += ch - 'A' + 10;
                }
                str += hex_ch;
                hex_ch = '\0';
                phase = Raw;
            }
            else{
                i--;
                hex_ch = '\0';
                phase = Raw;
            }
        }
    }
    if(queto != ' '){
        str += queto;
        str = queto + str;
    }
    return {std::move(str),i};
}

std::pair<JSONObject, size_t> parse(std::string_view json) {
    if (json.empty()) {
        return {JSONObject{std::nullptr_t{}}, 0};
    } else if (size_t off = json.find_first_not_of(" \n\r\t\v\f\0"); off != 0 && off != json.npos) {
        //should be ignored
        auto [obj, eaten] = parse(json.substr(off));
        return {std::move(obj), eaten + off};
    } else if ('0' <= json[0] && json[0] <= '9' || json[0] == '+' || json[0] == '-') {
        //for numbers
        std::regex num_re{"[+-]?[0-9]+(\\.[0-9]*)?([eE][+-]?[0-9]+)?"};
        std::cmatch match;
        if (std::regex_search(json.data(), json.data() + json.size(), match, num_re)) {
            std::string str = match.str();
            if (auto num = try_parse_num<int>(str)) {
                return {JSONObject{*num}, str.size()};
            }
            if (auto num = try_parse_num<double>(str)) {
                return {JSONObject{*num}, str.size()};
            }
        }
    } else if (json[0] == '"') {
        //for double-quoted string 
        auto [str,i] = try_parse_string(json,'"');
        return {JSONObject{std::move(str)}, i};
    } else if (json[0] == '\''){
        //for single-quoted string 
        auto [str,i] = try_parse_string(json, '\'');
        return {JSONObject{std::move(str)}, i};
    } else if (json[0] == '[') {
        //for list
        std::vector<JSONObject> res;
        size_t i;
        for (i = 1; i < json.size();) {
            if (json[i] == ']') {
                i += 1;
                break;
            }
            auto [obj, eaten] = parse(json.substr(i));
            if (eaten == 0) {
                i = 0;
                break;
            }
            res.push_back(std::move(obj));
            i += eaten;
            if (json[i] == ',') {
                i += 1;
            }
        }
        return {JSONObject{std::move(res)}, i};
    } else if (json[0] == '{') {
        //for dict
        std::unordered_map<std::string, JSONObject> res;
        size_t i;
        for (i = 1; i < json.size();) {
            if (json[i] == '}') {
                i += 1;
                break;
            }
            std::string key;
            {//for key
                std::regex elem_re(R"([ \n\r\t\v\f\0]*?[\'\"]{0,1}[a-zA-Z]+[\'\"]{0,1}[ \n\r\t\v\f\0]*?:)");
                std::cmatch match;
                std::regex_search(json.data() + i, json.data() + json.size(), match, elem_re);
                key = match.str();
                i += key.size();
                key = key.substr(0, key.size() - 1);
                int beg = 0;
                while(std::string(" \n\r\t\v\f\0").find(key.at(beg)) != std::string::npos){
                    beg++;
                }
                int end = key.size() - 1;
                while(std::string(" \n\r\t\v\f\0").find(key.at(end)) != std::string::npos){
                    end--;
                }

                key = key.substr(beg, end - beg + 1);
            }
            auto [valobj, valeaten] = parse(json.substr(i));
            if (valeaten == 0) {
                i = 0;
                break;
            }
            i += valeaten;
            res.try_emplace(std::move(key), std::move(valobj));
            //solve issue : can't allow any space behind value
            while(std::string(" \n\r\t\v\f\0").find(json[i])!=std::string::npos){
                i++;
            }
            if (json[i] == ',') {
                i += 1;
            }
        }
        return {JSONObject{std::move(res)}, i};
    }
    else if(json[0]=='t'){
        //for bool
        if(auto index = json.find("true"); index==0){
            return {JSONObject{true},4};
        }
    }
    else if(json[0]=='f'){
        //for bool
        if(auto index = json.find("false"); index==0){
            return {JSONObject{false},5};
        }
    }
    else if(json[0]=='n'){
        //for null
        if(auto index = json.find("null"); index==0){
            return {JSONObject{nullptr},4};
        }
    }
    return {JSONObject{std::monostate{}}, 0};
}

template <bool is_pretty>
std::string dumper(JSONObject const& obj){
    std::string res;
    if constexpr(!is_pretty){
        // not pretty version
        auto dovisit =[&](auto &&func,JSONObject const& obj) -> void{
        std::visit(
            overloaded{
                [&] (int val) {
                    res += std::to_string(val);
                },
                [&] (double val) {
                    res += std::to_string(val);
                },
                [&] (std::string val) {
                    res += val;
                },
                [&] (JSONDict val){
                    res += "{";
                    for(auto [key,value]:val){
                        res += key;
                        res += ":";
                        func(func,value);
                        res += ",";
                    }
                    res = res.substr(0,res.size() - 1);
                    res += "}";
                },
                [&] (JSONList val){
                    res += "[";
                    for(auto v : val){
                        func(func,v);
                        res += ",";
                    }
                    res += "]";
                },
                [&] (bool val){
                    if(val){
                        res += "true";
                    }
                    else{
                        res += "false";
                    }
                },
                [&] (std::nullptr_t ){
                    res += "null";
                },
                [&] (auto val) {
                    
                },
            },
            obj.inner);
        };
        dovisit(dovisit,obj);
    } else{
        //pretty version
        int depth = 0;
        auto dovisit =[&](auto &&func,JSONObject const& obj) -> void{
        std::visit(
            overloaded{
                [&] (int val) {
                    res += std::to_string(val);
                },
                [&] (double val) {
                    res += std::to_string(val);
                },
                [&] (std::string val) {
                    res += val;
                },
                [&] (JSONDict val){
                    res += "{\n";
                    for(auto [key,value]:val){
                        res += "\t";
                        for(int i = 0; i < depth; i++)
                            res += "\t";
                        res += key;
                        res += " : ";
                        depth++;
                        func(func,value);
                        depth--;
                        res += ",";
                        res += "\n";
                    }
                    res = res.substr(0,res.size() - 2);
                    res += "\n";
                    for(int i = 0; i < depth; i++)
                        res += "\t";
                    res += "}";
                },
                [&] (JSONList val){
                    res += "[";
                    for(auto v : val){
                        func(func,v);
                        res += ", ";
                    }
                    res = res.substr(0,res.size() - 2);
                    res += "]";
                },
                [&] (bool val){
                    if(val){
                        res += "true";
                    }
                    else{
                        res += "false";
                    }
                },
                [&] (std::nullptr_t ){
                    res += "null";
                },
                [&] (auto val) {
                    
                },
            },
            obj.inner);
        };
        dovisit(dovisit,obj);
    }

    return std::move(res);
}

template <class ...Fs>
struct overloaded : Fs... {
    using Fs::operator()...;
};

template <class ...Fs>
overloaded(Fs...) -> overloaded<Fs...>;

int main() {
    std::string file_path = "./test.json";
    std::string str = read_file(file_path);
    auto [obj, eaten] = parse(str);
    // print(obj);
    std::cout << dumper<true>(obj) << std::endl;

    return 0;
}
