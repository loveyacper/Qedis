#ifndef BERT_QAOF_H
#define BERT_QAOF_H

#include "MemoryFile.h"
#include "OutputBuffer.h"
#include "QStore.h"

extern const char* const g_aofFile;
class QAOF
{
public:
    ~QAOF();

    void    SaveCommand(const std::vector<QString>& params);
    bool    Loop();
    static  void SaveDoneHandler(int exitcode, int bysignal);

private:
    MemoryFile      m_qdb;
    OutputBuffer    m_buf;
};

extern time_t g_lastQDBSave;
extern pid_t  g_qdbPid;

class QAOFLoader
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
