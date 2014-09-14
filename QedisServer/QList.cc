#include "QList.h"
#include "QStore.h"
#include "Log/Logger.h"
#include <iostream>
#include <cassert>

using namespace std;

enum ListPosition
{
    ListPosition_head,
    ListPosition_tail,
};

static QError  push(const vector<QString>& params, UnboundedBuffer& reply, ListPosition pos, bool createIfNotExist = true)
{
    QObject  value(QType_list);
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        if (err != QError_notExist)
        {
            ReplyErrorInfo(err, reply);
            return err;
        }
        else if (createIfNotExist)
        {
            value.value.Reset(new QList());
            QSTORE.SetValue(params[1], value);
        }
        else
        {
            ReplyErrorInfo(err, reply);
            return err;
        }
    }

    const PLIST& list = value.CastList();
    for (unsigned int i = 2; i < params.size(); ++ i)
    {
        if (pos == ListPosition_head)
            list->push_front(params[i]);
        else
            list->push_back(params[i]);
    }
    
    FormatInt(list->size(), reply);
    return   QError_ok;
}


static QError  pop(const vector<QString>& params, UnboundedBuffer& reply, ListPosition pos)
{
    QObject  value(QType_list);
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        FormatBulk(0, -1, reply);
        return  err;
    }
    
    const PLIST&    list   = value.CastList();
    const QString*  result = 0;

    if (pos == ListPosition_head)
    {
        result = &list->front();
        list->pop_front();
    }
    else
    {
        result = &list->back();
        list->pop_back();
    }
    
    if (list->empty())
    {
        QSTORE.DeleteKey(params[1]);
    }
    
    FormatSingle(result->c_str(), result->size(), reply);
    return   QError_ok;
}

QError  lpush(const vector<QString>& params, UnboundedBuffer& reply)
{
    return push(params, reply, ListPosition_head);
}

QError  rpush(const vector<QString>& params, UnboundedBuffer& reply)
{
    return push(params, reply, ListPosition_tail);
}

QError  lpushx(const vector<QString>& params, UnboundedBuffer& reply)
{
    return push(params, reply, ListPosition_head, false);
}

QError  rpushx(const vector<QString>& params, UnboundedBuffer& reply)
{
    return push(params, reply, ListPosition_tail, false);
}

QError  lpop(const vector<QString>& params, UnboundedBuffer& reply)
{
    return pop(params, reply, ListPosition_head);
}

QError  rpop(const vector<QString>& params, UnboundedBuffer& reply)
{
    return pop(params, reply, ListPosition_tail);
}

QError  lindex(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject  value(QType_list);
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        FormatBulk(0, -1, reply);
        return  err;
    }
    
    const PLIST&    list   = value.CastList();
    long idx;
    if (!Str2Int(params[2].c_str(), params[2].size(), idx))
    {
        FormatBulk(0, -1, reply);
        return  QError_notExist;
    }
    
    if (idx < 0)
        idx += list->size();
    
    if (idx < 0 || idx >= list->size())
    {
        FormatBulk(0, -1, reply);
        return  QError_paramNotMatch;
    }
    
    const QString* result = 0;
    
    if (2 * idx < list->size())
    {
        QList::const_iterator it = list->begin();
        while (idx -- > 0)
        {
            ++ it;
        }
        result = &*it;
    }
    else
    {
        QList::const_reverse_iterator  it = list->rbegin();
        idx = list->size() - 1 - idx;
        while (idx -- > 0)
        {
            ++ it;
        }
        result = &*it;
    }
    
    FormatSingle(result->c_str(), result->size(), reply);
    return   QError_ok;
}


QError  lset(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject  value(QType_list);
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        ReplyErrorInfo(QError_notExist, reply);
        return  err;
    }
    
    const PLIST&    list = value.CastList();
    long idx;
    if (!Str2Int(params[2].c_str(), params[2].size(), idx))
    {
        ReplyErrorInfo(QError_paramNotMatch, reply);
        return  QError_notExist;
    }
    
    if (idx < 0)
        idx += list->size();
    
    if (idx < 0 || idx >= list->size())
    {
        FormatBulk(0, -1, reply);
        return  QError_paramNotMatch;
    }
    
    QString* result = 0;
    
    if (2 * idx < list->size())
    {
        QList::iterator it = list->begin();
        while (idx -- > 0)
        {
            ++ it;
        }
        result = &*it;
    }
    else
    {
        QList::reverse_iterator it = list->rbegin();
        idx = list->size() - 1 - idx;
        while (idx -- > 0)
        {
            ++ it;
        }
        result = &*it;
    }
    
    *result = params[3];
    
    FormatSingle("OK", 2, reply);
    return   QError_ok;
}


QError  llen(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject  value(QType_list);
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        if (err == QError_wrongType)
        {
            ReplyErrorInfo(err, reply);
        }
        else
        {
            FormatInt(0, reply);
        }

        return  err;
    }
    
    const PLIST&    list   = value.CastList();
    FormatInt(list->size(), reply);
    return   QError_ok;
}

static void AdjustIndex(long& start, long& end, size_t  size)
{
    if (size == 0)
    {
        end = 0, start = 1;
        return;
    }
    
    if (start < 0)  start += size;
    if (start < 0)  start = 0;
    if (end < 0)    end += size;
    
    if (start > end || start >= size)
        end = 0, start = 1;
    
    if (end >= size)  end = size - 1;
}

static void Index2Iterator(long start, long end,
                           const QList&  list,
                           QList::const_iterator& beginIt,
                           QList::const_iterator& endIt)
{
    assert (start >= 0 && end >= 0 && start <= end);
    
    beginIt = list.begin();
    while (start -- > 0)   ++ beginIt;

    endIt  = list.begin();
    while (end -- > 0)  ++ endIt;
}

static size_t GetRange(long start, long end,
                           const QList&  list,
                           QList::const_iterator& beginIt,
                           QList::const_iterator& endIt)
{
    size_t   rangeLen = 0;
    if (start > end)
    {
        beginIt = endIt = list.end();  // empty
    }
    else if (start != 0 || end + 1 != list.size())
    {
        rangeLen = end - start + 1;
        Index2Iterator(start, end, list, beginIt, endIt);
    }
    else
    {
        rangeLen= list.size();
        beginIt = list.begin();
        endIt   = -- list.end();  // entire list
    }
    
    return rangeLen;
}

QError  ltrim(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject  value(QType_list);
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        ReplyErrorInfo(err, reply);
        return  err;
    }
    
    long start, end;
    if (!Strtol(params[2].c_str(), params[2].size(), &start) ||
        !Strtol(params[3].c_str(), params[3].size(), &end))
    {
        ReplyErrorInfo(QError_paramNotMatch, reply);
        return err;
    }
    
    const PLIST&    list   = value.CastList();
    AdjustIndex(start, end, list->size());
    
    QList::const_iterator beginIt, endIt;
    GetRange(start, end, *list, beginIt, endIt);
    
    if (beginIt != list->end())
    {
        assert (endIt != list->end());
        list->erase(list->begin(), beginIt);
        list->erase(++ endIt, list->end());
    }
    
    FormatSingle("OK", 2, reply);
    return   QError_ok;
}

QError  lrange(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject  value(QType_list);
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        ReplyErrorInfo(err, reply);
        return  err;
    }
    
    long start, end;
    if (!Strtol(params[2].c_str(), params[2].size(), &start) ||
        !Strtol(params[3].c_str(), params[3].size(), &end))
    {
        ReplyErrorInfo(QError_paramNotMatch, reply);
        return err;
    }
    
    const PLIST&    list   = value.CastList();
    AdjustIndex(start, end, list->size());
    
    QList::const_iterator beginIt, endIt;
    size_t rangeLen = GetRange(start, end, *list, beginIt, endIt);
    
    PreFormatMultiBulk(rangeLen, reply);
    
    if (beginIt != list->end())
    {
        assert (endIt != list->end());

        while (true)
        {
            FormatBulk(beginIt->c_str(), beginIt->size(), reply);
            if (beginIt == endIt)
                break;
            
            ++ beginIt;
        }
    }
    
    return   QError_ok;
}

QError  linsert(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject  value(QType_list);
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        FormatInt(0, reply);
        return err;
    }
    
    bool before = false;
    if (params[2] == "before")
        before = true;
    else if (params[2] == "after")
        before = false;
    else
    {
        ReplyErrorInfo(QError_wrongType, reply);
        return QError_wrongType;
    }
    
    const PLIST&    list = value.CastList();
    QList::iterator it = std::find(list->begin(), list->end(), params[3]);
    if (it == list->end())
    {
        FormatInt(-1, reply);
        return QError_notExist;
    }
    
    if (before)
        list->insert(it, params[4]);
    else
        list->insert(++ it, params[4]);
    
    FormatInt(static_cast<long>(list->size()), reply);
    
    return   QError_ok;
}


QError  lrem(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject  value(QType_list);
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        FormatInt(0, reply);
        return err;
    }
    
    long count;
    if (!Strtol(params[2].c_str(), params[2].size(), &count))
    {
        ReplyErrorInfo(QError_paramNotMatch, reply);
        return err;
    }
    
    const PLIST&    list = value.CastList();
    
    ListPosition  start = ListPosition_head;
    
    if (count < 0)
    {
        count = -count;
        start = ListPosition_tail;
    }
    else if (count == 0)
    {
        count = list->size(); // remove all elements equal to param[3]
    }
    
    long resultCount = 0;
    if (start == ListPosition_head)
    {
        QList::iterator it = list->begin();
        while (it != list->end() && resultCount < count)
        {
            if (*it == params[3])
            {
                list->erase(it ++);
                ++ resultCount;
            }
            else
            {
                ++ it;
            }
        }
    }
    else
    {
        QList::reverse_iterator it = list->rbegin();
        while (it != list->rend() && resultCount < count)
        {
            if (*it == params[3])
            {
                list->erase((++it).base()); // Effective STL, item 28
                ++ resultCount;
            }
            else
            {
                ++ it;
            }
        }
    }

    FormatInt(resultCount, reply);
    return   QError_ok;
}


QError  rpoplpush(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject  src(QType_list);
    
    QError err = QSTORE.GetValueByType(params[1], src, QType_list);
    if (err != QError_ok)
    {
        FormatNull(reply);
        return err;
    }
    
    QObject  dst(QType_list);
    
    err = QSTORE.GetValueByType(params[2], dst, QType_list);
    if (err != QError_ok)
    {
        if (err != QError_notExist)
        {
            ReplyErrorInfo(err, reply);
            return err;
        }
        dst.value.Reset(new QList);
        QSTORE.SetValue(params[2], dst);
    }
    
    const PLIST& srclist = src.CastList();
    const PLIST& dstlist = dst.CastList();
    
    dstlist->splice(dstlist->begin(), *srclist, (++ srclist->rbegin()).base());
    
    FormatBulk(dstlist->begin()->c_str(),
               dstlist->begin()->size(),
               reply);
    
    return   QError_ok;
}
