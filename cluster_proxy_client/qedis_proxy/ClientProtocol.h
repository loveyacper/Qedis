#ifndef BERT_CLIENTPROTOCOL_H
#define BERT_CLIENTPROTOCOL_H

#include <string>
#include <vector>

enum class ResponseType
{
    None,
    Fine,   // +
    Error,  // -
    String, // $
    Number, // :
    Multi,  // *
};

enum class CParseResult : int8_t 
{ 
    ok, 
    waiting, 
    error,
};

class ClientProtocol
{
public:
    ClientProtocol();

    void Reset();

    CParseResult Parse(const char*& data, const char* end);
    //const std::vector<std::string>& GetParams() const { return params_; }
    const std::string& GetParam() const { return content_; }

private:
    ResponseType type_;
    std::string content_;
    //std::vector<std::string> params_;
    int len_ = -1;
};

#endif

