#ifndef BERT_QDB_H
#define BERT_QDB_H

#include "MemoryFile.h"
#include "QStore.h"

extern const char* const g_qdbFile;
class QDBSaver
{
public:
    void    Save(const char* qdbFile);
    void    SaveType(const QObject& obj);
    void    SaveKey(const QString& key);
    void    SaveObject(const QObject& obj);
    void    SaveString(const QString& str);
    void    SaveLength(uint64_t len);   // big endian
    void    SaveString(int64_t intVal);
    bool    SaveLZFString(const QString& str);
    
    static  void SaveDoneHandler(int exitcode, int bysignal);

private:
    void    _SaveDoubleValue(double val);
    
    void    _SaveList(const PLIST& l);
    void    _SaveSet(const PSET& s);
    void    _SaveHash(const PHASH& h);
    void    _SaveSSet(const PSSET& ss);
   
    MemoryFile  m_qdb;
};

extern time_t g_lastQDBSave;
extern pid_t  g_qdbPid;

class QDBLoader
{
public:
    int Load(const char* filename);

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
    QObject _LoadIntset();
    
    MemoryFile m_qdb;
};

#endif
