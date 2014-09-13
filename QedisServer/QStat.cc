#include "QStat.h"
#include <sys/time.h>
#include <fstream>
#include <sstream>

std::map<int, int>  g_stat;
std::map<int, int>  g_processStat;
std::map<int, int>  g_sendStat;

QStat::QStat(int cmdIndex) : m_cmdIndex(cmdIndex)
{
}

void QStat::Begin()
{
    return;
    timeval  begin;
    gettimeofday(&begin, 0);
    m_beginUs = begin.tv_sec * 1000000 + begin.tv_usec;
}

QStat::~QStat()
{
}

void QStat::End(StaticState state)
{
    return;
    if (m_cmdIndex >= 0)
    {
        timeval  end;
        gettimeofday(&end, 0);
        unsigned int used = end.tv_sec * 1000000 + end.tv_usec - m_beginUs;
        switch (state)
        {
        case PARSE_STATE:
            ++ g_stat[used];
            break;

        case PROCESS_STATE:
            ++ g_processStat[used];
            break;

        case SEND_STATE: 
            ++ g_sendStat[used];
            break;
        }
    }
}


void QStat::Output(StaticState state, const char* p)
{
    std::fstream   file(p, std::ios::out | std::ios::app | std::ios::binary);
    if (!file)  return;

    std::map<int, int>* pStat = 0;

    switch (state)
    {
    case PARSE_STATE:
        pStat = &g_stat;
        file.write("parse state\n", 12);
        break;

    case PROCESS_STATE:
        pStat = &g_processStat;
        file.write("prces state\n", 12);
        break;

    case SEND_STATE: 
        pStat = &g_sendStat;
        file.write("send  state\n", 12);
        break;

    default:
        file.write("Wrong state!", 10);
        return;
    }

    int totalTime = 0;
    int totalCount = 0;
    std::map<int, int>::const_iterator it(pStat->begin());
    for (; it != pStat->end(); ++ it)
    {
        std::ostringstream  os;
        os << it->second << " cmds used usecs " << it->first << std::endl;
        file.write(os.str().c_str(), os.str().size());

        totalCount += it->second;
        totalTime  += it->second * it->first;
    }
        
    std::ostringstream  os;
    os << " total cmds " << totalCount << " used time " << totalTime << std::endl;
    file.write(os.str().c_str(), os.str().size());
}

