#include "../src/youtube_wire.hpp"
#include <QCoreApplication>
#include <iostream>
#include <cstdlib>

static void require(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}

int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    using namespace sc::youtube_wire;

    // Reference payload built from Google's published StreamList protobuf fields.
    const QByteArray payload=QByteArray::fromHex("fa3e19aa06026d31120a080182010568656c6c6f1a06ba0603416461");
    Batch batch;require(decode(payload,batch),"Published-field response was rejected");
    require(batch.messages.size()==1,"Expected one chat message");
    const auto message=batch.messages.first();
    require(message.platform=="YouTube"&&message.id=="m1"&&message.author=="Ada"&&message.text=="hello","Decoded chat fields differ");

    const QByteArray requestBody=request("chat-123","page-456");
    require(requestBody.size()>5&&quint8(requestBody[0])==0,"Request is not an uncompressed gRPC frame");
    const QByteArray requestPayload=requestBody.mid(5);
    Reader requestReader(requestPayload);Field field;int parts=0;bool chat=false,page=false;
    while(requestReader.next(field)){
        if(field.number==1&&field.bytes=="chat-123")chat=true;
        if(field.number==99&&field.bytes=="page-456")page=true;
        if(field.number==100)++parts;
    }
    require(requestReader.valid()&&chat&&page&&parts==3,"StreamList request fields are incomplete");

    Frames frames;QVector<QByteArray> completed;const QByteArray framed=frame(payload);
    require(frames.append(framed.left(3),completed)&&completed.isEmpty(),"Partial header was rejected");
    require(frames.append(framed.mid(3,7),completed)&&completed.isEmpty(),"Partial payload was rejected");
    require(frames.append(framed.mid(10),completed)&&completed.size()==1&&completed.first()==payload&&frames.empty(),"Chunked frame was not reassembled");

    Frames compressed;QByteArray unsupported=framed;unsupported[0]=char(1);
    require(!compressed.append(unsupported,completed),"Compressed gRPC frame was accepted");
    Frames oversized;QByteArray header(5,'\0');const quint32 length=quint32(Frames::MaxFrame+1);
    header[1]=char(length>>24);header[2]=char(length>>16);header[3]=char(length>>8);header[4]=char(length);
    require(!oversized.append(header,completed),"Oversized gRPC frame was accepted");
    Batch malformed;require(!decode(QByteArray(11,char(0x80)),malformed),"Malformed varint was accepted");

    Batch extended;require(decode(stringField(50,"ignored")+payload,extended)&&extended.messages.size()==1,"Unknown protobuf field was not skipped safely");
    Batch ended;require(decode(stringField(2,"offline"),ended)&&ended.ended,"Offline marker was not recognized");
    const QByteArray tombstone=stringField(1007,stringField(101,"deleted-id")+stringField(2,varint(8*1+0)+varint(2)));
    Batch removed;require(decode(tombstone,removed)&&removed.messages.isEmpty()&&removed.removedIds.size()==1&&removed.removedIds.first()=="deleted-id","Tombstone was not converted into a removal");
    std::cout<<"YouTube StreamList request, response and bounded gRPC framing passed\n";
}

