#include <cassert>
#include "ClientProtocol.h"

#define CRLF "\r\n"

// helpers
static
const char* Strstr(const char* ptr, size_t nBytes, const char* pattern, size_t nBytes2)
{
    if (!pattern || *pattern == 0)
        return nullptr;
            
    const char* ret = std::search(ptr, ptr + nBytes, pattern, pattern + nBytes2);
    return ret == ptr + nBytes ? nullptr : ret;
}

static
const char* SearchCRLF(const char* ptr, size_t nBytes)
{
    return Strstr(ptr, nBytes, CRLF, 2);
}

ClientProtocol::ClientProtocol()
{
    Reset();
}
    
CParseResult ClientProtocol::Parse(const char*& ptr, const char* end)
{
    assert (end - ptr >= 1);

    const char* data = ptr;
    if (type_ == ResponseType::None)
    {
        switch (data[0])
        {
            case '+':
                type_ = ResponseType::Fine;
                break;

            case '-':
                type_ = ResponseType::Error;
                break;

            case '$':
                type_ = ResponseType::String;
                break;

            case ':':
                type_ = ResponseType::Number;
                break;

            case '*':
                type_ = ResponseType::Multi;
                break;

            default:
                assert (!!!"wrong type");
                return CParseResult::error;
        }
    
        content_.push_back(data[0]);
        ++ data;
    }

    const char* res = SearchCRLF(data, end - data);
    if (!res) return CParseResult::waiting;

    bool ready = false;
    switch (type_)
    {
        case ResponseType::Fine:
        case ResponseType::Error:
        case ResponseType::String:
        case ResponseType::Number:
            ready = true;
            break;

        case ResponseType::Multi:
            // TODO Multi
            break;

        default:
            assert (!!!"bug wrong type");
            break;
    }

    data = res + 2;
    content_.append(ptr, data);
    return CParseResult::ok;
}

void ClientProtocol::Reset()
{
    type_ = ResponseType::None;
    content_.clear();
    //params_.clear();
    len_ = -1;
}

