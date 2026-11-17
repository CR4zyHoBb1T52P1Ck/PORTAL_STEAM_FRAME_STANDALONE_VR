#pragma once
#include <string>
#include <string_view>
#include <cmath>
#include <limits>
#include <type_traits>
#include <cctype>
#include <exception>

namespace VrSettings {
// One non-throwing scalar reader for startup and the settings menu. Older
// menu builds wrote true/false to the integer RenderWindow field.
template<class T> inline T ConfigValue(std::string text,T fallback,bool* valid=nullptr) {
    static_assert(std::is_arithmetic_v<T>);
    if(valid)*valid=false;
    text=text.substr(0,text.find('#'));
    const auto first=text.find_first_not_of(" \t\r\n");
    if(first==std::string::npos)return fallback;
    text=text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);
    auto token=text;for(auto& ch:token)ch=char(std::tolower(static_cast<unsigned char>(ch)));
    try {
        size_t used=0;long double value;
        if(token=="true")value=1;
        else if(token=="false")value=0;
        else {value=std::stold(text,&used);if(used!=text.size())return fallback;}
        if(!std::isfinite(value))return fallback;
        if constexpr(std::is_integral_v<T>)if(value!=std::trunc(value))return fallback;
        if(value<std::numeric_limits<T>::lowest()||value>std::numeric_limits<T>::max())return fallback;
        if(valid)*valid=true;
        return static_cast<T>(value);
    } catch(const std::exception&) {return fallback;}
}
enum class Command { None, Left, Right, Recenter, Graphics };
inline Command Parse(std::string_view text) {
    const auto first=text.find_first_not_of(" \t\r\n");
    if(first==std::string_view::npos) return Command::None;
    text=text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);
    if(text=="portal1vr_hand_left") return Command::Left;
    if(text=="portal1vr_hand_right") return Command::Right;
    if(text=="portal1vr_recenter") return Command::Recenter;
    if(text=="portal1vr_graphics") return Command::Graphics;
    return Command::None;
}
// Preserve unrelated settings, comments and existing line endings, including
// duplicate keys (the runtime parser otherwise lets the last duplicate win).
inline std::string SetValue(const std::string& input,const std::string& key,const std::string& value) {
    std::string output;bool found=false;
    for(size_t pos=0;pos<input.size();) {
        const auto end=input.find('\n',pos);
        std::string line=input.substr(pos,end==std::string::npos ? input.size()-pos : end-pos+1);
        if(line.compare(0,key.size()+1,key+"=")==0) {
            auto suffix=line.find_first_of("#\r\n",key.size()+1);
            line=key+"="+value+(suffix==std::string::npos ? "" : line.substr(suffix));
            found=true;
        }
        output+=line;
        if(end==std::string::npos) break;
        pos=end+1;
    }
    if(!found) {
        const char* newline=input.find("\r\n")!=std::string::npos ? "\r\n" : "\n";
        if(!output.empty() && output.back()!='\n') output+=newline;
        output+=key+"="+value+newline;
    }
    return output;
}
inline std::string SetBool(const std::string& input,const std::string& key,bool value) {
    return SetValue(input,key,value ? "true" : "false");
}
}
