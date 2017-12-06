#ifndef BERT_COMMAND_H
#define BERT_COMMAND_H

#include <vector>
#include <map>
#include <string>

enum CommandAttr
{
    Attr_read  = 0x1,
    Attr_write = 0x1 << 1,
    Attr_multikey = 0x1 << 2,
};


using CommandHandler = std::string (const std::vector<std::string>& params);

// proxy commands
CommandHandler  ping;
CommandHandler  info;

struct CommandInfo
{
    std::string cmd;
    int attr = 0;
    int params = 0;
    CommandHandler* handler = nullptr;

    bool CheckParamsCount(int nParams) const;
};

class CommandTable
{
public:
    static void Init();
    static const CommandInfo* GetCommandInfo(const std::string& cmd);

private:
    static const CommandInfo s_info[];
    static std::map<std::string, const CommandInfo* > s_handlers;
};

enum QedisError
{
    QError_nop       = -1,
    QError_ok        = 0,
    QError_param     = 1,
    QError_unknowCmd = 2,
    QError_syntax    = 3,
    QError_notready  = 4,
    QError_timeout   = 5,
    QError_dead      = 6,
    QError_max,
};

extern struct QedisErrorInfo
{
    size_t len;
    const char* errorStr;
} g_errorInfo[];


#endif

