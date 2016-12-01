#ifndef BERT_QSTRING_H
#define BERT_QSTRING_H

#include <string>
#include <memory>

namespace qedis
{

using QString = std::string;

//typedef std::basic_string<char, std::char_traits<char>, Bert::Allocator<char> >  QString;

struct QObject;
std::shared_ptr<QString>  GetDecodedString(const QObject* value);

}

#endif
