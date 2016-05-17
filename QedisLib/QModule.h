#ifndef BERT_QMODULE_H
#define BERT_QMODULE_H

#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <memory>
#include "QString.h"

namespace qedis
{

class ModuleExist : public std::logic_error
{
public:
    explicit ModuleExist(const QString& what) : std::logic_error(what)
    {
    }
};
    
class ModuleNotExist : public std::logic_error
{
public:
    explicit ModuleNotExist(const QString& what) : std::logic_error(what)
    {
    }
};
    
class ModuleNoLoad : public std::logic_error
{
public:
    explicit ModuleNoLoad(const QString& what) : std::logic_error(what)
    {
    }
};
    
class ModuleNoUnLoad : public std::logic_error
{
public:
    explicit ModuleNoUnLoad(const QString& what) : std::logic_error(what)
    {
    }
};
    

class QModule
{
public:
    QModule();
    ~QModule();

    const QString& Name() const;

    void Load(const char* so, bool lazy = false);
    void UnLoad();
    void* Symbol(const char* symbol);

private:
    QString soname_;
    void* handler_;
};

class QModuleManager
{
public:
    static QModuleManager& Instance();
    
    QModuleManager(const QModuleManager& ) = delete;
    void operator= (const QModuleManager& ) = delete;

    QModule* Load(const char* so, bool lazy = false) throw(std::logic_error, std::runtime_error);
    void UnLoad(const char* so);

    QModule* GetModule(const char* so);

    std::vector<QString> NameList() const;

private:
    QModuleManager() {}

    std::unordered_map<QString, std::shared_ptr<QModule> >  modules_;
};

#define MODULES  ::qedis::QModuleManager::Instance()

}

#endif

