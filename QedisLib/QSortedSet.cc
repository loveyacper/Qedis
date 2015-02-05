#include "QSortedSet.h"
#include "QStore.h"
#include "Log/Logger.h"
#include <cassert>

using namespace std;

QSortedSet::Member2Score::iterator  QSortedSet::FindMember(const QString& member)
{
    return  m_members.find(member);
}

void  QSortedSet::AddMember(const QString& member, long score)
{
    assert (FindMember(member) == m_members.end());
        
    m_members.insert(Member2Score::value_type(member, score));
    m_scores[score].insert(member);
}

long    QSortedSet::UpdateMember(const Member2Score::iterator& itMem, long delta)
{
    long oldScore = itMem->second;
    long newScore = oldScore + delta;
    itMem->second = newScore;

    Score2Members::iterator  itScore(m_scores.find(oldScore));
    assert (itScore != m_scores.end());

    size_t ret = itScore->second.erase(itMem->first);
    assert (ret == 1);

    bool succ = m_scores[newScore].insert(itMem->first).second;
    assert (succ);

    return newScore;
}

int     QSortedSet::Rank(const QString& member) const
{
    long    score;
    Member2Score::const_iterator  itMem(m_members.find(member));
    if (itMem != m_members.end())
    {
        score = itMem->second;
    }
    else
    {
        return -1;
    }

    int  rank = 0;
    for (Score2Members::const_iterator it(m_scores.begin());
                                       it != m_scores.end();
                                       rank += it->second.size(), ++ it)
    {
        if (it->first == score)
        {
            set<QString>::const_iterator iter(it->second.begin());
            for (; iter != it->second.end(); ++ iter, ++ rank)
            {
                if (*iter == member)
                    return rank;
            }
            
            assert (!!!"Why can not find member");
        }
    }
            
    assert (!!!"Why can not find score");
    return  -1;
}


int  QSortedSet::RevRank(const QString& member) const
{
    int rank = Rank(member);
    if (rank == -1)
        return  rank;

    return m_members.size() - (rank + 1);
}

bool  QSortedSet::DelMember(const QString& member)
{
    long  score = 0;
    Member2Score::const_iterator  itMem(m_members.find(member));
    if (itMem != m_members.end())
    {
        score  = itMem->second;
        m_members.erase(itMem);
    }
    else
    {
        return false;
    }

    Score2Members::iterator it(m_scores.find(score));
    assert (it != m_scores.end());
            
    size_t num = it->second.erase(member);
    assert (num == 1);

    return  true;
}

std::pair<QString, long> QSortedSet::GetMemberByRank(size_t  rank) const
{
    if (rank >= m_members.size())
        rank = m_members.size() - 1;

    long    score = 0;
    QString member;

    size_t  iterRank = 0;
    Score2Members::const_iterator   it(m_scores.begin());

    for ( ;
          it != m_scores.end();
          iterRank += it->second.size(), ++ it)
    {
        if (iterRank + it->second.size() > rank)
        {
            assert(iterRank <= rank);
            
            score = it->first;
            set<QString>::const_iterator itMem(it->second.begin());
            for (; iterRank != rank; ++ iterRank, ++ itMem)
            {
            }
            
            LOG_DBG(g_log) << "Get rank " << rank << ", name " << itMem->c_str();
            return std::make_pair(*itMem, score);
        }
    }

    return std::make_pair(member, score);
}


size_t QSortedSet::Size() const
{
    return   m_members.size();
}


std::vector<std::pair<QString, long> > QSortedSet::RangeByRank(long start, long end) const
{
    AdjustIndex(start, end, Size());
    if (start > end)
    {
        return   std::vector<std::pair<QString, long> >();
        
    }
    
    std::vector<std::pair<QString, long> >  res;
    
    for (long rank = start; rank <= end; ++ rank)
    {
        res.push_back(GetMemberByRank(rank));
    }
    
    return res;
}

std::vector<std::pair<QString, long> > QSortedSet::RangeByScore(long minScore, long maxScore)
{
    if (minScore > maxScore)
        return std::vector<std::pair<QString, long> >();
    
    Score2Members::const_iterator  itMin = m_scores.lower_bound(minScore);
    if (itMin == m_scores.end())
        return std::vector<std::pair<QString, long> >();
    
    std::vector<std::pair<QString, long> >  res;
    Score2Members::const_iterator  itMax = m_scores.upper_bound(maxScore);
    for (; itMin != itMax; ++ itMin)
    {
        for (std::set<QString>::const_iterator it(itMin->second.begin());
             it != itMin->second.end();
             ++ it)
        {
            res.push_back(std::make_pair(*it, itMin->first));
        }
    }
    return  res;
}

// commands
#define GET_SORTEDSET(name)  \
    QObject* value;  \
    QError err = QSTORE.GetValueByType(name, value, QType_sortedSet);  \
    if (err != QError_ok)  {  \
        ReplyError(err, reply); \
        return err;  \
}

#define GET_OR_SET_SORTEDSET(name)  \
    QObject* value;  \
    QError err = QSTORE.GetValueByType(name, value, QType_sortedSet);  \
    if (err != QError_ok && err != QError_notExist)  {  \
        ReplyError(err, reply); \
        return err;  \
    }   \
    if (err == QError_notExist) { \
        QObject val(QType_sortedSet);  \
        val.value.reset(new QSortedSet);  \
        value = QSTORE.SetValue(name, val);  \
    }

QError  zadd(const vector<QString>& params, UnboundedBuffer& reply)
{
    if (params.size() % 2 != 0)
    {
        ReplyError(QError_syntax, reply);
        return  QError_syntax;
    }

    GET_OR_SET_SORTEDSET(params[1]);
    
    size_t  newMembers = 0;
    const PSSET& sset = value->CastSortedSet();
    for (size_t i = 2; i < params.size(); i += 2)
    {
        long    score = 0;
        if (!Strtol(params[i].c_str(), params[i].size(), &score))
        {
            ReplyError(QError_nan, reply);
            return QError_nan;
        }

        QSortedSet::Member2Score::iterator  it = sset->FindMember(params[i+1]);
        if (it == sset->end())
        {
            sset->AddMember(params[i+1], score);
            ++ newMembers;
        }
    }

    FormatInt(newMembers, reply);
    return   QError_ok;
}

QError  zcard(const vector<QString>& params, UnboundedBuffer& reply)
{
    GET_SORTEDSET(params[1]);
    
    const PSSET& sset = value->CastSortedSet();

    FormatInt(static_cast<long>(sset->Size()), reply);
    return   QError_ok;
}

QError  zrank(const vector<QString>& params, UnboundedBuffer& reply)
{
    GET_SORTEDSET(params[1]);
    
    const PSSET& sset = value->CastSortedSet();

    int rank = sset->Rank(params[2]);
    if (rank != -1)
        FormatInt(rank, reply);
    else
        FormatNull(reply);

    return   QError_ok;
}

QError  zrevrank(const vector<QString>& params, UnboundedBuffer& reply)
{
    GET_SORTEDSET(params[1]);
    
    const PSSET& sset = value->CastSortedSet();

    int rrank = sset->RevRank(params[2]);
    if (rrank != -1)
        FormatInt(rrank, reply);
    else
        FormatNull(reply);

    return   QError_ok;
}

QError  zrem(const vector<QString>& params, UnboundedBuffer& reply)
{
    GET_SORTEDSET(params[1]);
    
    const PSSET& sset = value->CastSortedSet();

    long cnt = 0;
    for (size_t i = 2; i < params.size(); ++ i)
    {
        if (sset->DelMember(params[i]))
            ++ cnt;
    }

    FormatInt(cnt, reply);
    return   QError_ok;
}

QError  zincrby(const vector<QString>& params, UnboundedBuffer& reply)
{
    GET_OR_SET_SORTEDSET(params[1]);

    long  delta;
    if (!Strtol(params[2].c_str(), params[2].size(), &delta))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }
    
    long newScore = delta;
    const PSSET& sset = value->CastSortedSet();
    const QSortedSet::Member2Score::iterator& itMem = sset->FindMember(params[3]);
    if (itMem == sset->end())
    {
        sset->AddMember(params[3], delta);
    }
    else
    {
        newScore = sset->UpdateMember(itMem, delta);
    }

    FormatInt(newScore, reply);
    return   QError_ok;
}

QError  zscore(const vector<QString>& params, UnboundedBuffer& reply)
{
    GET_SORTEDSET(params[1]);

    const PSSET& sset = value->CastSortedSet();
    const QSortedSet::Member2Score::iterator& itMem = sset->FindMember(params[2]);
    if (itMem == sset->end())
    {
        FormatNull(reply);
    }
    else
    {
        FormatInt(itMem->second, reply);
    }

    return   QError_ok;
}


static QError GenericRange(const vector<QString>& params, UnboundedBuffer& reply, bool reverse)
{
    GET_SORTEDSET(params[1]);
    
    bool withScore = false;
    if (params.size() == 5 && strncasecmp(params[4].c_str(), "withscores", 10) == 0)
    {
        withScore = true;
    }
    else if (params.size() >= 5)
    {
        ReplyError(QError_syntax, reply);
        return  QError_syntax;
    }
    
    long start, end;
    if (!Strtol(params[2].c_str(), params[2].size(), &start) ||
        !Strtol(params[3].c_str(), params[3].size(), &end))
    {
        ReplyError(QError_param, reply);
        return  QError_param;
    }
    
    const PSSET& sset = value->CastSortedSet();
    
    std::vector<std::pair<QString, long> > res(sset->RangeByRank(start, end));
    if (res.empty())
    {
        FormatNull(reply);
        return QError_ok;
    }
    
    long nBulk = withScore ? res.size() * 2 : res.size();
    PreFormatMultiBulk(nBulk, reply);
    
    if (!reverse)
    {
        for (std::vector<std::pair<QString, long> >::const_iterator it(res.begin());
             it != res.end();
             ++ it)
        {
            FormatSingle(it->first.c_str(), it->first.size(), reply);
            if (withScore)
            {
                char score[64];
                int  len = Int2Str(score, sizeof score, it->second);
            
                FormatSingle(score, len, reply);
            }
        }
    }
    else
    {
        for (std::vector<std::pair<QString, long> >::reverse_iterator it(res.rbegin());
             it != res.rend();
             ++ it)
        {
            FormatSingle(it->first.c_str(), it->first.size(), reply);
            if (withScore)
            {
                char score[64];
                int  len = Int2Str(score, sizeof score, it->second);
                
                FormatSingle(score, len, reply);
            }
        }
    }
    
    return   QError_ok;
}

// zrange key start stop [WITHSCORES]
QError  zrange(const vector<QString>& params, UnboundedBuffer& reply)
{
    return GenericRange(params, reply, false);
}

// zrange key start stop [WITHSCORES]
QError  zrevrange(const vector<QString>& params, UnboundedBuffer& reply)
{
    return GenericRange(params, reply, true);
}


static QError GenericScoreRange(const vector<QString>& params, UnboundedBuffer& reply, bool reverse)
{
    GET_SORTEDSET(params[1]);
    
    bool withScore = false;
    if (params.size() == 5 && strncasecmp(params[4].c_str(), "withscores", 10) == 0)
    {
        withScore = true;
    }
    else if (params.size() >= 5)
    {
        ReplyError(QError_syntax, reply);
        return  QError_syntax;
    }
    
    long minScore, maxScore;
    if (!Strtol(params[2].c_str(), params[2].size(), &minScore) ||
        !Strtol(params[3].c_str(), params[3].size(), &maxScore))
    {
        ReplyError(QError_nan, reply);
        return  QError_nan;
    }
    
    const PSSET& sset = value->CastSortedSet();
    
    std::vector<std::pair<QString, long> > res(sset->RangeByScore(minScore, maxScore));
    if (res.empty())
    {
        FormatNull(reply);
        return QError_ok;
    }
    
    long nBulk = withScore ? res.size() * 2 : res.size();
    PreFormatMultiBulk(nBulk, reply);
    
    if (!reverse)
    {
        for (std::vector<std::pair<QString, long> >::const_iterator it(res.begin());
             it != res.end();
             ++ it)
        {
            FormatSingle(it->first.c_str(), it->first.size(), reply);
            if (withScore)
            {
                char score[64];
                int  len = Int2Str(score, sizeof score, it->second);
                
                FormatSingle(score, len, reply);
            }
        }
    }
    else
    {
        for (std::vector<std::pair<QString, long> >::reverse_iterator it(res.rbegin());
             it != res.rend();
             ++ it)
        {
            FormatSingle(it->first.c_str(), it->first.size(), reply);
            if (withScore)
            {
                char score[64];
                int  len = Int2Str(score, sizeof score, it->second);
                
                FormatSingle(score, len, reply);
            }
        }
    }
    
    return   QError_ok;
}

QError  zrangebyscore(const vector<QString>& params, UnboundedBuffer& reply)
{
    return GenericScoreRange(params, reply, false);
}

QError  zrevrangebyscore(const vector<QString>& params, UnboundedBuffer& reply)
{
    return GenericScoreRange(params, reply, true);
}

static QError GenericRemRange(const vector<QString>& params, UnboundedBuffer& reply, bool useRank)
{
    GET_SORTEDSET(params[1]);
    
    long start, end;
    if (!Strtol(params[2].c_str(), params[2].size(), &start) ||
        !Strtol(params[3].c_str(), params[3].size(), &end))
    {
        ReplyError(QError_nan, reply);
        return  QError_nan;
    }
    
    
    std::vector<std::pair<QString, long> > res;
    const PSSET& sset = value->CastSortedSet();
    if (useRank)
    {
        AdjustIndex(start, end, sset->Size());
        res = sset->RangeByRank(start, end);
    }
    else
    {
        res = sset->RangeByScore(start, end);
    }
    
    if (res.empty())
    {
        Format0(reply);
        return QError_ok;
    }
    
    for (std::vector<std::pair<QString, long> >::const_iterator it(res.begin());
             it != res.end();
             ++ it)
    {
        bool succ = sset->DelMember(it->first);
        assert(succ);
    }
    
    if (sset->Size() == 0)
        QSTORE.DeleteKey(params[1]);
    
    FormatInt(static_cast<long>(res.size()), reply);
    return   QError_ok;
}

QError zremrangebyrank(const vector<QString>& params, UnboundedBuffer& reply)
{
    return GenericRemRange(params, reply, true);
}

QError zremrangebyscore(const vector<QString>& params, UnboundedBuffer& reply)
{
    return GenericRemRange(params, reply, false);
}
