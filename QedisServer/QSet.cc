#include "QSet.h"
#include "QStore.h"
#include "Log/Logger.h"
#include <iostream>
#include <cassert>

using namespace std;


#define GET_SET(setname)  \
    QObject* value;  \
    QError err = QSTORE.GetValueByType(setname, value, QType_set);  \
    if (err != QError_ok)  {  \
         if (err == QError_notExist)    \
             FormatNull(reply); \
         else   \
             ReplyError(err, reply);    \
        return err;  \
    }

#define GET_OR_SET_SET(setname)  \
    QObject* value;  \
    QError err = QSTORE.GetValueByType(setname, value, QType_set);  \
    if (err != QError_ok && err != QError_notExist)  {  \
        ReplyError(err, reply); \
        return err;  \
    }   \
    if (err == QError_notExist) { \
        QObject val(QType_set);  \
        val.value.Reset(new QSet);  \
        value = QSTORE.SetValue(setname, val);  \
    }

static bool RandomMember(const QSet& set, QString& res)
{
    if (set.empty())
    {
        LOG_ERR(g_logger) << "set is empty";
        return false;
    }
        
    LOG_INF(g_logger) << "set bucket_count " << set.bucket_count();

    while (true)
    {
        size_t bucket = rand() % set.bucket_count();
        LOG_INF(g_logger) << "set bucket " << bucket << ", and bucket size " << set.bucket_size(bucket);
        if (set.bucket_size(bucket) == 0)
            continue;

        int lucky = rand() % set.bucket_size(bucket);
        QSet::const_local_iterator it = set.begin(bucket);
        while (lucky > 0)
        {
            ++ it;
            -- lucky;
        }
        
        res = *it;
        return true;
    }

    return false;
}

QError  spop(const vector<QString>& params, UnboundedBuffer& reply)
{
    GET_SET(params[1]);

    const PSET& set  = value->CastSet();

    QString res;
    if (RandomMember(*set, res))
    {
        FormatSingle(res.c_str(), res.size(), reply);
        set->erase(res);
    }
    else
    {
        FormatNull(reply);
    }
    
    return   QError_ok;
}


QError  srandmember(const vector<QString>& params, UnboundedBuffer& reply)
{
    GET_SET(params[1]);

    const PSET& set  = value->CastSet();

    QString res;
    if (RandomMember(*set, res))
    {
        FormatSingle(res.c_str(), res.size(), reply);
    }
    else
    {
        FormatNull(reply);
    }

    return   QError_ok;
}

QError  sadd(const vector<QString>& params, UnboundedBuffer& reply)
{
    GET_OR_SET_SET(params[1]);
    
    int  res = 0;
    const PSET&  set = value->CastSet();
    for (size_t i = 2; i < params.size(); ++ i)
        if (set->insert(params[i]).second)
            ++ res;
    
    FormatInt(res, reply);

    return   QError_ok;
}

QError  scard(const vector<QString>& params, UnboundedBuffer& reply)
{
    GET_SET(params[1]);

    const PSET&  set  = value->CastSet();
    long size = static_cast<long>(set->size());
    cout << "scard fine= " << size << endl;
    
    FormatInt(size, reply);
    return   QError_ok;
}

QError  srem(const vector<QString>& params, UnboundedBuffer& reply)
{
    GET_SET(params[1]);

    const PSET& set  = value->CastSet();
    int res = 0;
    for (size_t i = 2; i < params.size(); ++ i)
    {
        if (set->erase(params[i]) != 0)
            ++ res;
    }
    
    FormatInt(res, reply);
    return   QError_ok;
}

QError  sismember(const vector<QString>& params, UnboundedBuffer& reply)
{
    GET_SET(params[1]);
    
    const PSET& set  = value->CastSet();
    long res = static_cast<long>(set->count(params[2]));
    
    FormatInt(res, reply);

    return   QError_ok;
}

QError  smembers(const vector<QString>& params, UnboundedBuffer& reply)
{
    GET_SET(params[1]);

    const PSET& set = value->CastSet();

    PreFormatMultiBulk(set->size(), reply);
    for (QSet::const_iterator it(set->begin()); it != set->end(); ++ it)
        FormatBulk(it->c_str(), it->size(), reply);

    return   QError_ok;
}

QError  smove(const vector<QString>& params, UnboundedBuffer& reply)
{
    return QError_ok;
#if 0
    assert (params[0] == "smove");
    
    GET_SET(params[1]);
    
    const PSET& set = value->CastSet();
    int  ret = static_cast<int>(set->erase(params[3]));
    
    if (ret != 0)
    {
        QObject  dst(QType_set);
        err = QSTORE.GetValueByType(params[2], dst, QType_set);
        if (err == QError_notExist)
        {
            err = QError_ok;
            dst.value.Reset(new QSet());
            QSTORE.SetValue(params[2], dst);
        }
        
        if (err == QError_ok)
        {
            PSET set = dst.CastSet();
            set->insert(params[3]);
        }
    }
    
    FormatInt(ret, reply);
    return err;
#endif
}



QSet& QSet_diff(const QSet& l, const QSet& r, QSet& result)
{
    for (QSet::const_iterator it(l.begin()); 
            it != l.end();
            ++ it)
    {
        if (r.find(*it) == r.end())
        {
            result.insert(*it);
        }
    }

    return result;
}

QSet& QSet_inter(const QSet& l, const QSet& r, QSet& result)
{
    for (QSet::const_iterator it(l.begin()); 
            it != l.end();
            ++ it)
    {
        if (r.find(*it) != r.end())
        {
            result.insert(*it);
        }
    }

    return result;
}


QSet& QSet_union(const QSet& l, const QSet& r, QSet& result)
{
    for (QSet::const_iterator it(r.begin());
         it != r.end();
         ++ it)
    {
        result.insert(*it);
    }
    
    for (QSet::const_iterator it(l.begin());
         it != l.end();
         ++ it)
    {
        result.insert(*it);
    }
    
    return result;
}

enum SetOperation
{
    SetOperation_diff,
    SetOperation_inter,
    SetOperation_union,
};

static void  _set_operation(const vector<QString>& params,
                            int offset,
                            QSet& res,
                            SetOperation oper)
{
#if 0
    QObject  value;
    QError err = QSTORE.GetValueByType(params[offset], value, QType_set);
    if (err != QError_ok && oper != SetOperation_union)
        return;

    const PSET& set = value.CastSet();
    if (set)
        res = *set;
    
    for (int i = offset + 1; i < params.size(); ++ i)
    {
        QObject  value;
        QError err = QSTORE.GetValueByType(params[i], value, QType_set);
        if (err != QError_ok)
        {
            if (oper == SetOperation_inter)
            {
                res.clear();
                return;
            }
            continue;
        }
        
        QSet tmp;
        const PSET r = value.CastSet();
        if (oper == SetOperation_diff)
            QSet_diff(res, *r, tmp);
        else if (oper == SetOperation_inter)
            QSet_inter(res, *r, tmp);
        else if (oper == SetOperation_union)
            QSet_union(res, *r, tmp);
        
        res.swap(tmp);
        
        if (oper != SetOperation_union && res.empty())
            return;
    }
#endif
}

QError  sdiffstore(const vector<QString>& params, UnboundedBuffer& reply)
{
    QSet* res = new QSet();
    _set_operation(params, 2, *res, SetOperation_diff);

    PreFormatMultiBulk(res->size(), reply);
    for (QSet::const_iterator it(res->begin()); it != res->end(); ++ it)
        FormatBulk(it->c_str(), it->size(), reply);

    QObject value(QType_set);
    value.value.Reset(res);
    
    QSTORE.SetValue(params[1], value);
    return QError_ok;
}

QError  sdiff(const vector<QString>& params, UnboundedBuffer& reply)
{
    QSet res;
    _set_operation(params, 1, res, SetOperation_diff);
    
    PreFormatMultiBulk(res.size(), reply);
    for (QSet::const_iterator it(res.begin()); it != res.end(); ++ it)
        FormatBulk(it->c_str(), it->size(), reply);
    
    return QError_ok;
}


QError  sinter(const vector<QString>& params, UnboundedBuffer& reply)
{
    QSet res;
    _set_operation(params, 1, res, SetOperation_inter);
    
    
    PreFormatMultiBulk(res.size(), reply);
    for (QSet::const_iterator it(res.begin()); it != res.end(); ++ it)
        FormatBulk(it->c_str(), it->size(), reply);
    
    return QError_ok;
}

QError  sinterstore(const vector<QString>& params, UnboundedBuffer& reply)
{
    QSet* res = new QSet;
    _set_operation(params, 2, *res, SetOperation_inter);
    
    PreFormatMultiBulk(res->size(), reply);
    for (QSet::const_iterator it(res->begin()); it != res->end(); ++ it)
        FormatBulk(it->c_str(), it->size(), reply);

    QObject  value(QType_set);
    value.value.Reset(res);

    QSTORE.SetValue(params[1], value);
    return QError_ok;
}


QError  sunion(const vector<QString>& params, UnboundedBuffer& reply)
{
    QSet res;
    _set_operation(params, 1, res, SetOperation_union);
    
    PreFormatMultiBulk(res.size(), reply);
    for (QSet::const_iterator it(res.begin()); it != res.end(); ++ it)
        FormatBulk(it->c_str(), it->size(), reply);
    
    return QError_ok;
}

QError  sunionstore(const vector<QString>& params, UnboundedBuffer& reply)
{
    QSet* res = new QSet;
    _set_operation(params, 2, *res, SetOperation_union);
    
    PreFormatMultiBulk(res->size(), reply);
    for (QSet::const_iterator it(res->begin()); it != res->end(); ++ it)
        FormatBulk(it->c_str(), it->size(), reply);
    
    QObject  value(QType_set);
    value.value.Reset(res);
    
    QSTORE.SetValue(params[1], value);
    return QError_ok;
}


