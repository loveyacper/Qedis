#ifndef BERT_QCLIENT_H
#define BERT_QCLIENT_H

#include "StreamSocket.h"
#include "QCommon.h"
#include "QString.h"

#include "QStat.h"

enum  ParseCmdState
{
    InitState,
    ProcessMultiBulkState,
    ProcessDollarState,
    ProcessArglenState,
    ProcessArgState,
    ReadyState,
};

class DB;

class QClient: public StreamSocket
{
private:
    HEAD_LENGTH_T _HandleHead(AttachedBuffer& buf, BODY_LENGTH_T* bodyLen);
    void _HandlePacket(AttachedBuffer& buf);

public:
    QClient();
    bool SelectDB(int db);
    static QClient*  Current() {  return s_pCurrentClient; }

private:
    void        _Reset();

    ParseCmdState  m_state;
    int   m_multibulk;
    int   m_paramLen;
    
    std::vector<QString> m_params;
    UnboundedBuffer  m_reply;

    int   m_db;
    
    QStat  m_stat;
    static  QClient*  s_pCurrentClient;
};

#endif

