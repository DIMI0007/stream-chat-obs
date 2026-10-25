#include "../src/secure_store.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>
#include <cstdlib>
static void require(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);QTemporaryDir directory;require(directory.isValid(),"No temp directory");
    const auto path=directory.filePath("account.dat");sc::SecureStore vault(path);bool ok=false;
    require(vault.load(&ok).isEmpty()&&ok,"Empty store should load");
    const QJsonObject tokens{{"access_token","TEST_ONLY_NOT_A_REAL_TOKEN"},{"refresh_token","TEST_REFRESH"}};
    require(vault.save(tokens),"DPAPI save failed");QFile raw(path);require(raw.open(QIODevice::ReadOnly),"Cannot read encrypted file");
    const auto encrypted=raw.readAll();raw.close();require(!encrypted.contains("TEST_ONLY_NOT_A_REAL_TOKEN")&&!encrypted.contains("TEST_REFRESH"),"Plain credentials leaked to disk");
    require(vault.load(&ok)==tokens&&ok,"DPAPI round-trip failed");
    require(raw.open(QIODevice::WriteOnly|QIODevice::Truncate),"Cannot simulate corruption");raw.write("invalid");raw.close();
    require(vault.load(&ok).isEmpty()&&!ok,"Corrupt store silently accepted");
    require(vault.save({})&&vault.load(&ok).isEmpty()&&ok,"Sign-out did not clear persisted credentials");
    std::cout<<"Windows DPAPI account storage, corruption and sign-out passed\n";
}

