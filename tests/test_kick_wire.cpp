#include "../src/kick_wire.hpp"
#include <QCoreApplication>
#include <iostream>
#include <cstdlib>

static void require(bool ok,const char* reason){if(!ok){std::cerr<<reason<<'\n';std::exit(1);}}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    using namespace sc::kick_wire;
    require(validChannel("otmopozok1101")&&validChannel("some_channel-2"),"Valid channel rejected");
    for(const auto& bad:{QString(),QString("../other"),QString("https://kick.com/name"),QString("a?b=1"),QString("a\nb"),QString("a\n"),QString(101,'a')})
        require(!validChannel(bad),"Unsafe channel accepted");
    require(!validMessage(" \n\t")&&validMessage(QString(500,'a'))&&!validMessage(QString(501,'a')),"Message character limit failed");
    const QString cluster=QString::fromUtf8("e\xcc\x81");
    require(validMessage(cluster.repeated(500))&&!validMessage(cluster.repeated(501)),"Combining grapheme limit failed");
    const QString family=QString::fromUtf8("👨‍👩‍👧‍👦");
    require(validMessage(family.repeated(81))&&!validMessage(family.repeated(82)),"UTF-8 byte limit failed");
    QJsonObject data{{"id","id-1"},{"content","<b>hello</b> [emote:123:Happy]"},
        {"created_at","2026-10-29T12:34:56Z"},{"sender",QJsonObject{{"id",42},{"username","Viewer"}}}};
    const auto m=message(data);
    require(m.platform=="Kick"&&m.id=="id-1"&&m.author=="Viewer"&&m.authorId=="42","Sender mapping failed");
    require(m.text=="<b>hello</b> :Happy:"&&m.fragments.size()==2&&m.fragments.last().emoteId=="kick:123","Emote mapping or literal text changed");
    require(m.sentAt.isValid()&&m.sentAt.toUTC().toString("HH:mm:ss")=="12:34:56","Message timestamp failed");
    data["content"]=QString(8193,'a');require(message(data).id.isEmpty(),"Oversized content accepted");
    require(message(QJsonObject{}).id.isEmpty(),"Malformed message accepted");
    std::cout<<"Kick channel validation, graphemes, UTF-8 bytes, sender, timestamp and emotes passed\n";
}
