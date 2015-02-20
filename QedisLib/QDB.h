#ifndef BERT_QDB_H
#define BERT_QDB_H

#include "MemoryFile.h"
#include "QStore.h"

class QDBSaver
{
public:
    void    Save();
    void    SaveType(const QObject& obj);
    void    SaveKey(const QString& key);
    void    SaveObject(const QObject& obj);
    void    SaveString(const QString& str);
    void    SaveLength(uint64_t len);   // big endian
    void    SaveString(int64_t intVal);
    bool    SaveLZFString(const QString& str);

private:
    void    _SaveDoubleValue(double val);
    
    void    _SaveList(const PLIST& l);
    void    _SaveSet(const PSET& s);
    void    _SaveHash(const PHASH& h);
    void    _SaveSSet(const PSSET& ss);
   
    MemoryFile  m_qdb;
};

#endif
