#pragma once
#include "chat_message.hpp"
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextBoundaryFinder>

namespace sc::kick_wire {
inline bool validChannel(const QString& slug){return QRegularExpression("\\A[a-zA-Z0-9_-]{1,100}\\z").match(slug).hasMatch();}
inline bool validMessage(const QString& text){
    if(text.trimmed().isEmpty()||text.toUtf8().size()>2048)return false;
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme,text);int count=0;
    while(finder.toNextBoundary()!=-1)if(++count>500)return false;
    return count>0;
}
inline ChatMessage message(const QJsonObject& data){
    ChatMessage m;m.platform="Kick";m.id=data.value("id").toString();
    const auto sender=data.value("sender").toObject();
    m.author=sender.value("username").toString().left(100);
    m.authorId=sender.value("id").toVariant().toString();
    const auto content=data.value("content").toString();
    if(m.id.isEmpty()||m.id.size()>128||m.author.isEmpty()||content.size()>8192){m.id.clear();return m;}
    m.sentAt=parsePlatformTime(data.value("created_at").toString());
    static const QRegularExpression emote("\\[emote:([0-9]{1,20}):([^\\]\\r\\n]{1,100})\\]");
    auto matches=emote.globalMatch(content);qsizetype offset=0;
    while(matches.hasNext()){
        const auto match=matches.next();
        if(match.capturedStart()>offset)m.fragments.append({content.mid(offset,match.capturedStart()-offset),{}});
        m.fragments.append({":"+match.captured(2)+":","kick:"+match.captured(1)});offset=match.capturedEnd();
    }
    if(offset<content.size())m.fragments.append({content.mid(offset),{}});
    for(const auto& fragment:m.fragments)m.text+=fragment.text;
    return m;
}
}
