#pragma once
#include <QString>
#include <QVector>
#include <QDateTime>
#include <algorithm>
namespace sc {
struct ChatFragment {QString text,emoteId;};
struct ChatMessage {QString platform,id,authorId,author,text;QVector<ChatFragment> fragments;QDateTime sentAt;QString avatarUrl;};
inline QDateTime parsePlatformTime(QString value){
    if(value.size()>64)return {};
    const auto dot=value.indexOf('.');
    if(dot>=0){
        qsizetype end=dot+1;
        while(end<value.size()&&value[end].isDigit())++end;
        if(end==dot+1)return {};
        value=value.left(dot+1)+value.mid(dot+1,std::min(qsizetype(3),end-dot-1))+value.mid(end);
    }
    auto date=QDateTime::fromString(value,Qt::ISODateWithMs);
    if(!date.isValid())date=QDateTime::fromString(value,Qt::ISODate);
    return date;
}
}
