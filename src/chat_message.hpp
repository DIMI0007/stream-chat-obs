#pragma once
#include <QString>
#include <QVector>
namespace sc {
struct ChatFragment {QString text,emoteId;};
struct ChatMessage {QString platform,id,authorId,author,text;QVector<ChatFragment> fragments;};
}
