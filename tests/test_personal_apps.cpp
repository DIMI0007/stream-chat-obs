#include "../src/twitch_chat.hpp"
#include "../src/youtube_chat.hpp"
#include "../src/traffic.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>
#include <cstdlib>
static void require(bool ok,const char* reason){if(!ok){std::cerr<<reason<<'\n';std::exit(1);}}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);QTemporaryDir dir;require(dir.isValid(),"Temporary folder unavailable");
    const auto twPath=dir.filePath("twitch.dat"),ytPath=dir.filePath("youtube.dat");
    sc::TwitchChat twitch(twPath);sc::YouTubeChat youtube(ytPath);
    require(!twitch.configured()&&!youtube.configured(),"Shared app credentials were preconfigured");
    twitch.signIn();youtube.signIn();QCoreApplication::processEvents();
    require(sc::traffic::sent.load()==0&&sc::traffic::received.load()==0,"Missing credentials started network login");
    require(twitch.configure("testpersonalclient12345"),"Twitch app rejected");
    require(youtube.configure("test-personal.apps.googleusercontent.com","TEST-PERSONAL-SECRET"),"Google desktop app rejected");
    require(!twitch.configure("bad id\n")&&!youtube.configure("wrong-client","secret"),"Invalid app accepted");
    require(twitch.configured()&&youtube.configured()&&!youtube.signedIn(),"Saving app unexpectedly signed in");
    twitch.signOut();youtube.signOut();
    sc::TwitchChat restoredTw(twPath);sc::YouTubeChat restoredYt(ytPath);
    require(restoredTw.clientId()=="testpersonalclient12345"&&restoredYt.clientSecret()=="TEST-PERSONAL-SECRET","Sign-out lost personal application settings");
    QFile encrypted(ytPath);require(encrypted.open(QIODevice::ReadOnly)&&!encrypted.readAll().contains("TEST-PERSONAL-SECRET"),"Google Client Secret stored unencrypted");
    encrypted.close();
    sc::SecureStore twVault(twPath),ytVault(ytPath);
    auto tw=twVault.load();tw["access_token"]="old-token";tw["refresh_token"]="old-refresh";require(twVault.save(tw),"Cannot seed Twitch token fixture");
    auto yt=ytVault.load();yt["access_token"]="old-token";yt["refresh_token"]="old-refresh";require(ytVault.save(yt),"Cannot seed Google token fixture");
    sc::TwitchChat changedTw(twPath);sc::YouTubeChat changedYt(ytPath);
    require(changedTw.configure("newpersonalclient12345")&&changedYt.configure("new-personal.apps.googleusercontent.com","NEW-PERSONAL-SECRET"),"App switch failed");
    require(!twVault.load().contains("access_token")&&!ytVault.load().contains("refresh_token")&&!changedYt.signedIn(),"App switch reused another application's token");
    std::cout<<"Personal Twitch/YouTube credentials, no shared defaults, DPAPI, sign-out and app-switch isolation passed\n";
}
