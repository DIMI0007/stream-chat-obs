#include "../src/oauth_loopback.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
#include <cstdlib>

static void require(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
static QByteArray exchange(const QUrl& url,const QByteArray& request){
    QTcpSocket socket;socket.connectToHost(QHostAddress::LocalHost,quint16(url.port()));
    QElapsedTimer elapsed;elapsed.start();
    while(socket.state()!=QAbstractSocket::ConnectedState&&elapsed.elapsed()<2000){QCoreApplication::processEvents();QThread::msleep(1);}
    require(socket.state()==QAbstractSocket::ConnectedState,"Local test connection failed");
    socket.write(request);QByteArray response;
    while(elapsed.elapsed()<2500){QCoreApplication::processEvents();response+=socket.readAll();if(socket.state()==QAbstractSocket::UnconnectedState)break;QThread::msleep(1);}
    return response;
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);OAuthLoopback receiver;QString code;
    QObject::connect(&receiver,&OAuthLoopback::received,&app,[&](const QUrlQuery& query){code=query.queryItemValue("code",QUrl::FullyDecoded);});
    require(receiver.begin(0,"/google/callback",false),"Code receiver did not start");
    const auto url=receiver.redirect();const auto host=url.authority().toUtf8();
    require(exchange(url,"GET /google/callback?state=wrong&code=BAD HTTP/1.1\r\nHost: "+host+"\r\n\r\n").startsWith("HTTP/1.1 400")&&receiver.active(),"Invalid state accepted or receiver stopped");
    require(exchange(url,"GET /google/callback HTTP/1.1\r\nHost: attacker.example\r\n\r\n").startsWith("HTTP/1.1 400"),"Foreign Host accepted");
    const auto response=exchange(url,"GET /google/callback?state="+receiver.state().toUtf8()+"&code=TEST%2BCODE HTTP/1.1\r\nHost: "+host+"\r\n\r\n");
    require(response.startsWith("HTTP/1.1 200")&&code=="TEST+CODE"&&!receiver.active(),"Valid code callback or one-use shutdown failed");
    require(receiver.begin(0,"/kick/callback",false,true),"Kick localhost receiver did not start");
    const auto kickUrl=receiver.redirect();require(kickUrl.host()=="localhost","Kick redirect hostname changed");
    require(exchange(kickUrl,"GET /kick/callback?state=wrong&code=BAD HTTP/1.1\r\nHost: "+kickUrl.authority().toUtf8()+"\r\n\r\n").startsWith("HTTP/1.1 400")&&receiver.active(),"Kick invalid state accepted");
    const auto kickResponse=exchange(kickUrl,"GET /kick/callback?state="+receiver.state().toUtf8()+"&code=KICK-CODE HTTP/1.1\r\nHost: "+kickUrl.authority().toUtf8()+"\r\n\r\n");
    require(kickResponse.startsWith("HTTP/1.1 200")&&code=="KICK-CODE"&&!receiver.active(),"Kick callback failed");
    std::cout<<"Google and Kick loopback OAuth Host, state, decoding and one-use lifecycle passed\n";
}

