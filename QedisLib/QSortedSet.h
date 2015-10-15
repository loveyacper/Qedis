#ifndef BERT_QSORTEDSET_H
#define BERT_QSORTEDSET_H

#include "QString.h"
#include "QHelper.h"
#include <map>
#include <set>
#include <vector>
#include <unordered_map>


QObject  CreateSSetObject();

class QSortedSet
{
public:
    typedef std::set<QString> Members;
    typedef std::map<double, Members>  Score2Members;

    typedef std::unordered_map<QString, double,
            my_hash,
            std::equal_to<QString> >   Member2Score;

    Member2Score::iterator FindMember(const QString& member);
    Member2Score::const_iterator begin() const {  return m_members.begin(); };
    Member2Score::iterator begin() {  return m_members.begin(); };
    Member2Score::const_iterator end() const {  return m_members.end(); };
    Member2Score::iterator end() {  return m_members.end(); };
    void    AddMember   (const QString& member, double score);
    double  UpdateMember(const Member2Score::iterator& itMem, double delta);

    int     Rank        (const QString& member) const;// 0-based
    int     RevRank     (const QString& member) const;// 0-based
    bool    DelMember   (const QString& member);
    Member2Score::value_type
        GetMemberByRank(std::size_t rank) const;
    
    std::vector<Member2Score::value_type > RangeByRank(long start, long end) const;

    std::vector<Member2Score::value_type >
        RangeByScore(double minScore, double maxScore);
    std::size_t Size    ()  const;

private:
    Score2Members   m_scores;
    Member2Score    m_members;
};

#endif 

