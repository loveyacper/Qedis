#ifndef BERT_QSTAT_H
#define BERT_QSTAT_H

#include <map>

extern std::map<int, int>  g_stat;
extern std::map<int, int>  g_processStat;
extern std::map<int, int>  g_sendStat;

enum StaticState
{
    PARSE_STATE,
    PROCESS_STATE,
    SEND_STATE,
};

class  QStat
{
public:
    QStat(int c = 0);
   ~QStat();

   void Begin();
   void End(StaticState  state);

   static void Output(StaticState state, const char* = "cmd_stat_info");
private:
   int m_cmdIndex;
   unsigned int m_beginUs;
};

#endif

