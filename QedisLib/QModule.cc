#include <sstream>
#include <dlfcn.h>
#include <assert.h>

#include "QCommon.h"
#include "QModule.h"

namespace qedis
{

QModule::QModule() : handler_(nullptr)
{
}

QModule::~QModule() 
{
    UnLoad();
}

const QString& QModule::Name() const
{
    return soname_;
}

void QModule::Load(const char* so, bool lazy)
{
    assert (!handler_);

    ::dlerror(); // clear errors
    handler_ = ::dlopen(so, lazy ? RTLD_LAZY : RTLD_NOW);
    if (!handler_)
    {
        // try local path
        std::string localName("./");
        localName += so;

        handler_ = ::dlopen(localName.c_str(), lazy ? RTLD_LAZY : RTLD_NOW);

        if (!handler_)
        {
            std::ostringstream oss;
            oss << "open [" << so << "] failed because:" << ::dlerror();

            throw std::runtime_error(oss.str());
        }
    }
}

void QModule::UnLoad()
{
    if (handler_)
    {
        ::dlclose(handler_);
        handler_ = nullptr;
    }
}

void* QModule::Symbol(const char* symbol)
{
    if (handler_)
    {
        ::dlerror();
        return ::dlsym(handler_, symbol);
    }

    return nullptr;
}

QModuleManager& QModuleManager::Instance()
{
    static QModuleManager mgr;
    return mgr;
}
    

QModule* QModuleManager::Load(const char* so, bool lazy) throw(std::logic_error, std::runtime_error)
{
    auto module = std::make_shared<QModule>();
    module->Load(so, lazy);
    
    if (!modules_.insert({QString(so), module}).second)
        throw ModuleExist(so);

    // try call init function
    using InitFunc = bool (*)();
    
    InitFunc initmodule = (InitFunc)(module->Symbol("QedisModule_OnLoad"));
    if (!initmodule || !initmodule())
    {
        module->UnLoad();
        modules_.erase(so);
        throw ModuleNoLoad(so);
    }

    return module.get();
}

void QModuleManager::UnLoad(const char* so)
{
    auto it = modules_.find(so);
    if (it == modules_.end())
        throw ModuleNotExist(so);
    
    QEDIS_DEFER
    {
        it->second->UnLoad();
        modules_.erase(it);
    };
    
    // try call uninit function
    using UnInitFunc = void (*)();
    
    UnInitFunc uninit = (UnInitFunc)(it->second->Symbol("QedisModule_OnUnLoad"));
    if (uninit)
    {
        uninit();
    }
    else
    {
        throw ModuleNoUnLoad(so);
    }
}

QModule* QModuleManager::GetModule(const char* so)
{
    auto it = modules_.find(so);
    if (it == modules_.end())
        return nullptr;

    return it->second.get();
}

std::vector<QString> QModuleManager::NameList() const
{
    std::vector<QString>  namelist;
    for (const auto& kv : modules_)
    {
        namelist.push_back(kv.first);
    }

    return namelist;
}

// MODULE LOAD /path/to/mymodule.so
// MODULE LIST
// MODULE UNLOAD mymodule
QError module(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    // MODULE LOAD /path/to/mymodule.{so,dylib}
    if (strncasecmp(params[1].c_str(), "load", 4) == 0)
    {
        if (params.size() != 3)
        {
            ReplyError(QError_syntax, reply);
            return QError_syntax;
        }

        try
        {
            MODULES.Load(params[2].c_str());
        }
        catch (const ModuleNoLoad& e)
        {
            ReplyError(QError_moduleinit, reply);
            return QError_moduleinit;
        }
        catch (const ModuleExist& e)
        {
            ReplyError(QError_modulerepeat, reply);
            return QError_modulerepeat;
        }
        catch (...)
        {
            ReplyError(QError_nomodule, reply);
            return QError_nomodule;
        }
    }
    // MODULE LIST
    else if (strncasecmp(params[1].c_str(), "list", 4) == 0)
    {
        auto names = MODULES.NameList();
        PreFormatMultiBulk(names.size(), reply);
        for (const auto& name : names)
        {
            FormatBulk(name, reply);
        }

        return QError_ok;
    }
    // MODULE UNLOAD mymodule
    else if (strncasecmp(params[1].c_str(), "unload", 6) == 0)
    {
        if (params.size() != 3)
        {
            ReplyError(QError_syntax, reply);
            return QError_syntax;
        }

        try
        {
            MODULES.UnLoad(params[2].c_str());
        }
        catch (const ModuleNotExist& )
        {
            ReplyError(QError_nomodule, reply);
            return QError_nomodule;
        }
        catch (const ModuleNoUnLoad& )
        {
            ReplyError(QError_moduleuninit, reply);
            return QError_moduleuninit;
        }
    }
    else
    {
        ReplyError(QError_syntax, reply);
        return QError_syntax;
    }
       
    FormatOK(reply);
    return QError_ok;
}

}

