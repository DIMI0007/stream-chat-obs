#include "../src/kick_chat.hpp"
#include "../src/traffic.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>
#include <cstdlib>
static void require(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);QTemporaryDir dir;require(dir.isValid(),"Temporary folder unavailable");
    const auto path=dir.filePath("kick-account.dat");
    sc::KickChat kick(path);kick.setVisible(true);
    require(kick.configure("TEST-CLIENT","TEST-SECRET","test_channel"),"Configuration failed");
    kick.signIn();bool rejected=false;
    QObject::connect(&kick,&sc::KickChat::sent,&app,[&](bool ok,const QString&){rejected=!ok;});
    kick.send("must not leave this process");QCoreApplication::processEvents();
    require(rejected&&!kick.ready()&&!kick.canSend()&&!kick.busy(),"Disabled Kick started login or accepted send");
    require(sc::traffic::sent.load()==0&&sc::traffic::received.load()==0,"Disabled Kick generated traffic");
    require(!kick.configure("TEST-CLIENT","TEST-SECRET","../invalid")&&kick.channel()=="test_channel","Invalid configuration replaced valid channel");
    QFile file(path);require(file.open(QIODevice::ReadOnly),"Encrypted account missing");
    require(!file.readAll().contains("TEST-SECRET"),"Client Secret persisted as plaintext");file.close();
    kick.setVisible(false);kick.setEnabled(true);QCoreApplication::processEvents();
    require(!kick.busy()&&!kick.ready()&&sc::traffic::sent.load()==0,"Hidden Kick connected");
    kick.setEnabled(false);sc::KickChat restored(path);
    require(restored.channel()=="test_channel"&&restored.clientId()=="TEST-CLIENT"&&restored.clientSecret()=="TEST-SECRET","Disabled Kick lost protected settings");
    std::cout<<"Kick default-off, hidden lifecycle, rejected sends and encrypted settings passed\n";
}
