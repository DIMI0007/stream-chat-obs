#pragma once
#include "chat_message.hpp"
#include <QByteArray>
#include <QVector>
#include <limits>

// Minimal reader for the published YouTube StreamList protobuf schema.
// Unknown fields are skipped; malformed/oversized frames are rejected.
// Source: developers.google.com/youtube/data/live/streaming-live-chat
namespace sc::youtube_wire {
struct Field {quint32 number=0;int wire=0;quint64 integer=0;QByteArray bytes;};
class Reader {
public:
    explicit Reader(const QByteArray& input):data_(input){}
    bool next(Field& field){
        if(!valid_||position_==data_.size())return false;
        quint64 tag=0;if(!varint(tag)||!(tag>>3)||(tag>>3)>0x1fffffff){valid_=false;return false;}
        field={};field.number=quint32(tag>>3);field.wire=int(tag&7);
        if(field.wire==0)return varint(field.integer);
        if(field.wire==1||field.wire==5){const int length=field.wire==1?8:4;if(data_.size()-position_<length){valid_=false;return false;}position_+=length;return true;}
        if(field.wire==2){quint64 length=0;if(!varint(length)||length>quint64(data_.size()-position_)){valid_=false;return false;}field.bytes=data_.mid(position_,qsizetype(length));position_+=qsizetype(length);return true;}
        valid_=false;return false;
    }
    bool valid()const{return valid_&&position_==data_.size();}
private:
    bool varint(quint64& value){value=0;for(int i=0;i<10;++i){if(position_>=data_.size()){valid_=false;return false;}const auto byte=quint8(data_[position_++]);if(i==9&&byte>1){valid_=false;return false;}value|=quint64(byte&127)<<(7*i);if(!(byte&128))return true;}valid_=false;return false;}
    const QByteArray& data_;qsizetype position_=0;bool valid_=true;
};
inline QByteArray varint(quint64 value){QByteArray result;do{const auto byte=quint8(value&127);value>>=7;result.append(char(byte|(value?128:0)));}while(value);return result;}
inline QByteArray stringField(quint32 number,const QByteArray& value){return varint((quint64(number)<<3)|2)+varint(quint64(value.size()))+value;}
inline QByteArray frame(const QByteArray& payload){const auto n=quint32(payload.size());QByteArray result;result.append(char(0));result.append(char(n>>24));result.append(char(n>>16));result.append(char(n>>8));result.append(char(n));return result+payload;}
inline QByteArray request(const QString& chatId,const QString& pageToken){
    QByteArray payload=stringField(1,chatId.toUtf8());if(!pageToken.isEmpty())payload+=stringField(99,pageToken.toUtf8());
    for(const auto& part:{QByteArray("id"),QByteArray("snippet"),QByteArray("authorDetails")})payload+=stringField(100,part);
    return frame(payload);
}
struct Batch {QVector<ChatMessage> messages;QVector<QString> removedIds;QString nextPage;bool ended=false;};
inline bool decode(const QByteArray& payload,Batch& batch){
    Reader reader(payload);Field field;
    while(reader.next(field)){
        if(field.number==100602&&field.wire==2){if(field.bytes.size()>8192)return false;batch.nextPage=QString::fromUtf8(field.bytes);}
        if(field.number==2&&field.wire==2&&!field.bytes.isEmpty())batch.ended=true;
        if(field.number!=1007||field.wire!=2)continue;
        if(batch.messages.size()>=2000)return false;
        ChatMessage message;message.platform="YouTube";quint64 type=0;Reader item(field.bytes);Field part;
        while(item.next(part)){
            if(part.number==101&&part.wire==2)message.id=QString::fromUtf8(part.bytes).left(256);
            if(part.number==3&&part.wire==2){Reader author(part.bytes);Field property;while(author.next(property)){
                if(property.number==103&&property.wire==2)message.author=QString::fromUtf8(property.bytes).left(100);
                if(property.number==10101&&property.wire==2)message.authorId=QString::fromUtf8(property.bytes).left(256);
            }if(!author.valid())return false;}
            if(part.number==2&&part.wire==2){Reader snippet(part.bytes);Field property;while(snippet.next(property)){
                if(property.number==16&&property.wire==2)message.text=QString::fromUtf8(property.bytes).left(5000);
                if(property.number==1&&property.wire==0){type=property.integer;if(type==4)batch.ended=true;}
            }if(!snippet.valid())return false;}
        }
        if(!item.valid())return false;
        if(type==2&&!message.id.isEmpty())batch.removedIds.append(message.id);
        else if(!message.id.isEmpty()&&!message.text.isEmpty())batch.messages.append(message);
    }
    return reader.valid();
}
class Frames {
public:
    bool append(const QByteArray& bytes,QVector<QByteArray>& completed){
        if(bytes.size()>65536||buffer_.size()+bytes.size()>MaxFrame+65541)return false;buffer_+=bytes;
        while(buffer_.size()>=5){
            if(quint8(buffer_[0])!=0)return false;
            const quint32 length=(quint32(quint8(buffer_[1]))<<24)|(quint32(quint8(buffer_[2]))<<16)|(quint32(quint8(buffer_[3]))<<8)|quint32(quint8(buffer_[4]));
            if(length>MaxFrame)return false;
            if(buffer_.size()<qsizetype(length)+5)return true;
            completed.append(buffer_.mid(5,length));buffer_.remove(0,qsizetype(length)+5);
        }
        return true;
    }
    void clear(){buffer_.clear();}
    bool empty()const{return buffer_.isEmpty();}
    static constexpr qsizetype MaxFrame=4*1024*1024;
private:QByteArray buffer_;
};
}

