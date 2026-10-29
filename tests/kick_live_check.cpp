// Isolated account smoke check. Never sends chat messages or changes OBS files.
#include "../src/kick_chat.hpp"
#include <QApplication>
#include <QFileInfo>
#include <QTextStream>
#include <QDesktopServices>
#include <QSaveFile>

class CaptureBrowser final:public QObject{
    Q_OBJECT
public:
    QString output;
public Q_SLOTS:
    void open(const QUrl& url){
        if(url.scheme()!="https"||url.host()!="id.kick.com"||url.path()!="/oauth/authorize")return;
        QSaveFile file(output);if(file.open(QIODevice::WriteOnly)){file.write(url.toEncoded());file.commit();}
        QTextStream(stdout)<<"Authorization URL prepared for the test browser"<<Qt::endl;
    }
};

int main(int argc,char** argv){
    QApplication app(argc,argv);const auto args=app.arguments();
    if(args.size()<3)return 2;
    const QString path=QFileInfo(args[1]).absoluteFilePath(),mode=args[2];
    if(mode!="--read"&&mode!="--sign-in"&&mode!="--refresh"&&mode!="--send")return 3;
    if(mode=="--send"&&args.size()!=4)return 6;
    if(mode=="--refresh"){
        sc::SecureStore store(path);bool ok=false;auto account=store.load(&ok);
        if(!ok||account.value("refresh_token").toString().isEmpty())return 4;
        account["expires_at"]=0;if(!store.save(account))return 5;
    }
    CaptureBrowser browser;browser.output=QFileInfo(path).absolutePath()+"/kick-authorize-url.txt";
    if(mode=="--sign-in")QDesktopServices::setUrlHandler("https",&browser,"open");
    sc::KickChat kick(path);int received=0;bool passed=false,submitted=false;
    auto report=[&]{QTextStream out(stdout);out<<kick.status(true)<<" | read="<<kick.ready()<<" send="<<kick.canSend()<<Qt::endl;
        if(kick.ready()&&(mode=="--read"||kick.canSend())){
            if(mode=="--send"){if(!submitted){submitted=true;QTimer::singleShot(0,&kick,[&]{kick.send(args[3]);});}}
            else {passed=true;QTimer::singleShot(3000,&app,&QCoreApplication::quit);}
        }};
    QObject::connect(&kick,&sc::KickChat::sent,&app,[&](bool confirmed,const QString&){
        QTextStream(stdout)<<"Send confirmed: "<<confirmed<<Qt::endl;passed=confirmed;QTimer::singleShot(3000,&app,&QCoreApplication::quit);
    });
    QObject::connect(&kick,&sc::KickChat::changed,&app,report);
    QObject::connect(&kick,&sc::KickChat::message,&app,[&](const sc::ChatMessage&){QTextStream(stdout)<<"Received messages: "<<++received<<Qt::endl;});
    kick.setEnabled(true);kick.setVisible(true);
    if(mode=="--sign-in")QTimer::singleShot(500,&kick,&sc::KickChat::signIn);
    QTimer::singleShot(180000,&app,&QCoreApplication::quit);app.exec();return passed?0:1;
}
#include "kick_live_check.moc"
