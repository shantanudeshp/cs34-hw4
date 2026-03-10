#include "StringUtils.h"
#include <cctype>
#include <algorithm>

namespace StringUtils{

std::string Slice(const std::string &str, ssize_t start, ssize_t end) noexcept{
    ssize_t len = str.length();

    if(start < 0){
        start = len + start;
    }

    if(end == 0){
        end = len;
    }
    else if(end < 0){
        end = len + end;
    }

    if(start < 0) start = 0;
    if(end > len) end = len;
    if(start >= end) return "";

    return str.substr(start, end - start);
}

std::string Capitalize(const std::string &str) noexcept{
    std::string result = str;
    if(!result.empty()){
        result[0] = toupper(result[0]);
        for(size_t i = 1; i < result.length(); i++){
            result[i] = tolower(result[i]);
        }
    }
    return result;
}

std::string Upper(const std::string &str) noexcept{
    std::string result = str;
    for(size_t i = 0; i < result.length(); i++){
        result[i] = toupper(result[i]);
    }
    return result;
}

std::string Lower(const std::string &str) noexcept{
    std::string result = str;
    for(size_t i = 0; i < result.length(); i++){
        result[i] = tolower(result[i]);
    }
    return result;
}

std::string LStrip(const std::string &str) noexcept{
    size_t start = 0;
    while(start < str.length() && isspace(str[start])){
        start++;
    }
    return str.substr(start);
}

std::string RStrip(const std::string &str) noexcept{
    if(str.empty()){
        return "";
    }
    size_t end = str.length() - 1;
    while(end > 0 && isspace(str[end])){
        end--;
    }
    if(isspace(str[end])){
        return "";
    }
    return str.substr(0, end + 1);
}

std::string Strip(const std::string &str) noexcept{
    return LStrip(RStrip(str));
}

std::string Center(const std::string &str, int width, char fill) noexcept{
    int len = str.length();
    if(len >= width){
        return str;
    }
    int total_padding = width - len;
    int left_pad = total_padding / 2;
    int right_pad = total_padding - left_pad;
    return std::string(left_pad, fill) + str + std::string(right_pad, fill);
}

std::string LJust(const std::string &str, int width, char fill) noexcept{
    int len = str.length();
    if(len >= width){
        return str;
    }
    return str + std::string(width - len, fill);
}

std::string RJust(const std::string &str, int width, char fill) noexcept{
    int len = str.length();
    if(len >= width){
        return str;
    }
    return std::string(width - len, fill) + str;
}

std::string Replace(const std::string &str, const std::string &old, const std::string &rep) noexcept{
    if(old.empty()){
        return str;
    }
    std::string result = str;
    size_t pos = 0;
    while((pos = result.find(old, pos)) != std::string::npos){
        result.replace(pos, old.length(), rep);
        pos += rep.length();
    }
    return result;
}

std::vector< std::string > Split(const std::string &str, const std::string &splt) noexcept{
    std::vector<std::string> result;

    if(splt.empty()){
        std::string word;
        for(char c : str){
            if(isspace(c)){
                if(!word.empty()){
                    result.push_back(word);
                    word.clear();
                }
            }
            else{
                word += c;
            }
        }
        if(!word.empty()){
            result.push_back(word);
        }
        return result;
    }

    size_t start = 0;
    size_t end = str.find(splt);

    while(end != std::string::npos){
        result.push_back(str.substr(start, end - start));
        start = end + splt.length();
        end = str.find(splt, start);
    }
    result.push_back(str.substr(start));

    return result;
}

std::string Join(const std::string &str, const std::vector< std::string > &vect) noexcept{
    if(vect.empty()){
        return "";
    }
    std::string result = vect[0];
    for(size_t i = 1; i < vect.size(); i++){
        result += str + vect[i];
    }
    return result;
}

std::string ExpandTabs(const std::string &str, int tabsize) noexcept{
    std::string result;
    int column = 0;

    for(char c : str){
        if(c == '\t'){
            int spaces = tabsize - (column % tabsize);
            result.append(spaces, ' ');
            column += spaces;
        }
        else{
            result += c;
            column++;
        }
    }
    return result;
}

int EditDistance(const std::string &left, const std::string &right, bool ignorecase) noexcept{
    std::string left_str = ignorecase ? Lower(left) : left;
    std::string right_str = ignorecase ? Lower(right) : right;

    size_t m = left_str.length();
    size_t n = right_str.length();

    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));

    for(size_t i = 0; i <= m; i++){
        dp[i][0] = i;
    }
    for(size_t j = 0; j <= n; j++){
        dp[0][j] = j;
    }

    for(size_t i = 1; i <= m; i++){
        for(size_t j = 1; j <= n; j++){
            if(left_str[i-1] == right_str[j-1]){
                dp[i][j] = dp[i-1][j-1];
            }
            else{
                dp[i][j] = 1 + std::min({dp[i-1][j], dp[i][j-1], dp[i-1][j-1]});
            }
        }
    }

    return dp[m][n];
}

};