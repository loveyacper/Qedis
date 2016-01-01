#include "QSortedSet.h"
#include "QStore.h"
#include "Log/Logger.h"
#include <cassert>

namespace qedis
{

QSortedSet::Member2Score::iterator  QSortedSet::FindMember(const QString& member)
{
    return  members_.find(member);
}

void  QSortedSet::AddMember(const QString& member, double score)
{
    assert (FindMember(member) == members_.end());
        
    members_.insert(Member2Score::value_type(member, score));
    scores_[score].insert(member);
}

double    QSortedSet::UpdateMember(const Member2Score::iterator& itMem, double delta)
{
    auto oldScore = itMem->second;
    auto newScore = oldScore + delta;
    itMem->second = newScore;

    Score2Members::iterator  itScore(scores_.find(oldScore));
    assert (itScore != scores_.end());

    size_t ret = itScore->second.erase(itMem->first);
    assert (ret == 1);

    bool succ = scores_[newScore].insert(itMem->first).second;
    assert (succ);

    return newScore;
}

int     QSortedSet::Rank(const QString& member) const
{
    double    score;
    Member2Score::const_iterator  itMem(members_.find(member));
    if (itMem != members_.end())
    {
        score = itMem->second;
    }
    else
    {
        return -1;
    }

    int  rank = 0;
    for (auto it(scores_.begin());
              it != scores_.end();
              rank += it->second.size(), ++ it)
    {
        if (it->first == score)
        {
            auto iter(it->second.begin());
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

    return static_cast<int>(members_.size() - (rank + 1));
}

bool  QSortedSet::DelMember(const QString& member)
{
    double  score = 0;
    Member2Score::const_iterator  itMem(members_.find(member));
    if (itMem != members_.end())
    {
        score  = itMem->second;
        members_.erase(itMem);
    }
    else
    {
        return false;
    }

    auto it(scores_.find(score));
    assert (it != scores_.end());
            
    auto num = it->second.erase(member);
    assert (num == 1);

    return  true;
}

QSortedSet::Member2Score::value_type
QSortedSet::GetMemberByRank(size_t  rank) const
{
    if (rank >= members_.size())
        rank = members_.size() - 1;

    double  score = 0;
    QString member;

    size_t  iterRank = 0;
   // Score2Members::const_iterator   it(scores_.begin());

    for ( auto it(scores_.begin());
          it != scores_.end();
          iterRank += it->second.size(), ++ it)
    {
        if (iterRank + it->second.size() > rank)
        {
            assert(iterRank <= rank);
            
            score = it->first;
            auto itMem(it->second.begin());
            for (; iterRank != rank; ++ iterRank, ++ itMem)
            {
            }
            
            DBG << "Get rank " << rank << ", name " << itMem->c_str();
            return std::make_pair(*itMem, score);
        }
    }

    return std::make_pair(member, score);
}


size_t QSortedSet::Size() const
{
    return   members_.size();
}


std::vector<QSortedSet::Member2Score::value_type >
QSortedSet::RangeByRank(long start, long end) const
{
    AdjustIndex(start, end, Size());
    if (start > end)
    {
        return   std::vector<Member2Score::value_type >();
    }
    
    std::vector<Member2Score::value_type >  res;
    
    for (long rank = start; rank <= end; ++ rank)
    {
        res.push_back(GetMemberByRank(rank));
    }
    
    return res;
}

std::vector<QSortedSet::Member2Score::value_type >
QSortedSet::RangeByScore(double minScore, double maxScore)
{
    if (minScore > maxScore)
        return std::vector<Member2Score::value_type >();
    
    Score2Members::const_iterator  itMin = scores_.lower_bound(minScore);
    if (itMin == scores_.end())
        return std::vector<Member2Score::value_type >();
    
    std::vector<Member2Score::value_type>  res;
    Score2Members::const_iterator  itMax = scores_.upper_bound(maxScore);
    for (; itMin != itMax; ++ itMin)
    {
        for (const auto& e : itMin->second)
        {
            res.push_back(std::make_pair(e, itMin->first));
        }
    }
    return  res;
}

QObject  CreateSSetObject()
{
    QObject obj(QType_sortedSet);
    obj.value = std::make_shared<QSortedSet>();
    return std::move(obj);
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
        QObject val(CreateSSetObject());  \
        value = QSTORE.SetValue(name, val);  \
    }

QError  zadd(const std::vector<QString>& params, UnboundedBuffer* reply)
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
        double    score = 0;
        if (!Strtod(params[i].c_str(), params[i].size(), &score))
        {
            ReplyError(QError_nan, reply);
            return QError_nan;
        }

        auto it = sset->FindMember(params[i+1]);
        if (it == sset->end())
        {
            sset->AddMember(params[i+1], score);
            ++ newMembers;
        }
    }

    FormatInt(newMembers, reply);
    return   QError_ok;
}

QError  zcard(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    GET_SORTEDSET(params[1]);
    
    const PSSET& sset = value->CastSortedSet();

    FormatInt(static_cast<long>(sset->Size()), reply);
    return   QError_ok;
}

QError  zrank(const std::vector<QString>& params, UnboundedBuffer* reply)
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

QError  zrevrank(const std::vector<QString>& params, UnboundedBuffer* reply)
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

QError  zrem(const std::vector<QString>& params, UnboundedBuffer* reply)
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

QError  zincrby(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    GET_OR_SET_SORTEDSET(params[1]);

    double  delta;
    if (!Strtod(params[2].c_str(), params[2].size(), &delta))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }
    
    double newScore = delta;
    const PSSET& sset = value->CastSortedSet();
    auto itMem = sset->FindMember(params[3]);
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

QError  zscore(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    GET_SORTEDSET(params[1]);

    const PSSET& sset = value->CastSortedSet();
    auto itMem = sset->FindMember(params[2]);
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


static QError GenericRange(const std::vector<QString>& params, UnboundedBuffer* reply, bool reverse)
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
    
    auto res(sset->RangeByRank(start, end));
    if (res.empty())
    {
        FormatNull(reply);
        return QError_ok;
    }
    
    long nBulk = withScore ? res.size() * 2 : res.size();
    PreFormatMultiBulk(nBulk, reply);
    
    if (!reverse)
    {
        for (const auto& s : res)
        {
            FormatSingle(s.first, reply);
            if (withScore)
            {
                char score[64];
                int  len = Double2Str(score, sizeof score, s.second);
            
                FormatSingle(score, len, reply);
            }
        }
    }
    else
    {
        for (const auto& s : res)
        {
            FormatSingle(s.first, reply);
            if (withScore)
            {
                char score[64];
                int  len = Double2Str(score, sizeof score, s.second);
                
                FormatSingle(score, len, reply);
            }
        }
    }
    
    return   QError_ok;
}

// zrange key start stop [WITHSCORES]
QError  zrange(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    return GenericRange(params, reply, false);
}

// zrange key start stop [WITHSCORES]
QError  zrevrange(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    return GenericRange(params, reply, true);
}


static QError GenericScoreRange(const std::vector<QString>& params, UnboundedBuffer* reply, bool reverse)
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
    
    auto res(sset->RangeByScore(minScore, maxScore));
    if (res.empty())
    {
        FormatNull(reply);
        return QError_ok;
    }
    
    long nBulk = withScore ? res.size() * 2 : res.size();
    PreFormatMultiBulk(nBulk, reply);
    
    if (!reverse)
    {
        for (const auto& s : res)
        {
            FormatSingle(s.first, reply);
            if (withScore)
            {
                char score[64];
                int  len = Double2Str(score, sizeof score, s.second);
                
                FormatSingle(score, len, reply);
            }
        }
    }
    else
    {
        for (const auto& s : res)
        {
            FormatSingle(s.first, reply);
            if (withScore)
            {
                char score[64];
                int  len = Double2Str(score, sizeof score, s.second);
                
                FormatSingle(score, len, reply);
            }
        }
    }
    
    return   QError_ok;
}

QError  zrangebyscore(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    return GenericScoreRange(params, reply, false);
}

QError  zrevrangebyscore(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    return GenericScoreRange(params, reply, true);
}

static QError GenericRemRange(const std::vector<QString>& params, UnboundedBuffer* reply, bool useRank)
{
    GET_SORTEDSET(params[1]);
    
    double start, end;
    if (!Strtod(params[2].c_str(), params[2].size(), &start) ||
        !Strtod(params[3].c_str(), params[3].size(), &end))
    {
        ReplyError(QError_nan, reply);
        return  QError_nan;
    }
    
    std::vector<QSortedSet::Member2Score::value_type> res;
    const PSSET& sset = value->CastSortedSet();
    if (useRank)
    {
        long lstart = static_cast<long>(start);
        long lend   = static_cast<long>(end);
        AdjustIndex(lstart, lend, sset->Size());
        res = sset->RangeByRank(lstart, lend);
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
    
    for (const auto& s : res)
    {
        bool succ = sset->DelMember(s.first);
        assert(succ);
    }
    
    if (sset->Size() == 0)
        QSTORE.DeleteKey(params[1]);
    
    FormatInt(static_cast<long>(res.size()), reply);
    return   QError_ok;
}

QError zremrangebyrank(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    return GenericRemRange(params, reply, true);
}

QError zremrangebyscore(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    return GenericRemRange(params, reply, false);
}
    
}
