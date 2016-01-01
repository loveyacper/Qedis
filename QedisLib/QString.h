#ifndef BERT_QSTRING_H
#define BERT_QSTRING_H

#include <string>
#include <memory>

namespace qedis
{

typedef std::string  QString;

//typedef std::basic_string<char, std::char_traits<char>, Bert::Allocator<char> >  QString;

struct   QObject;
QObject  CreateStringObject(const QString&  value);
QObject  CreateStringObject(long value);
std::shared_ptr<QString>  GetDecodedString(const QObject* value);

}

#endif
