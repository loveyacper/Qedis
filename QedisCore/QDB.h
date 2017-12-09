#ifndef BERT_QDB_H
#define BERT_QDB_H

#include "Log/MemoryFile.h"
#include "QStore.h"

namespace qedis
{

class QDBSaver
{
public:
    explicit
    QDBSaver(const char* file = nullptr);
    void    Save(const char* qdbFile);
    void    SaveType(const QObject& obj);
    void    SaveKey(const QString& key);
    void    SaveObject(const QObject& obj);
    void    SaveString(const QString& str);
    void    SaveLength(uint64_t len);   // big endian
    void    SaveString(int64_t intVal);
    bool    SaveLZFString(const QString& str);
    
    static  void SaveDoneHandler(int exit, int signal);

private:
    void    _SaveDoubleValue(double val);
    
    void    _SaveList(const PLIST& l);
    void    _SaveSet(const PSET& s);
    void    _SaveHash(const PHASH& h);
    void    _SaveSSet(const PSSET& ss);
   
    OutputMemoryFile  qdb_;
};

extern time_t g_lastQDBSave;
extern pid_t  g_qdbPid;

class QDBLoader
{
public:
    explicit
    QDBLoader(const char* data = nullptr, size_t len = 0);
    int Load(const char* filename);

    int8_t  LoadByte() { return qdb_.Read<int8_t>(); }
    size_t  LoadLength(bool& special);
    QObject LoadSpecialStringObject(size_t  specialVal);
    QString LoadString(size_t strLen);
    QString LoadLZFString();

    QString LoadKey();
    QObject LoadObject(int8_t type);

private:
    QString _LoadGenericString();
    QObject _LoadList();
    QObject _LoadSet();
    QObject _LoadHash();
    QObject _LoadSSet();
    double  _LoadDoubleValue();
    QObject _LoadZipList(int8_t type);
    QObject _LoadZipList(const QString& zl, int8_t type);
    QObject _LoadIntset();
    QObject _LoadQuickList();

    void    _LoadAux();
    void    _LoadResizeDB();
    
    InputMemoryFile qdb_;
};
    
std::string DumpObject(const QObject& val);
QObject RestoreObject(const char* data, size_t len);

}

#endif
