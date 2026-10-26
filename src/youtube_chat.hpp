#pragma once
#include "secure_store.hpp"
#include "oauth_loopback.hpp"
#include "youtube_wire.hpp"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <QSet>
#include <QQueue>
#include <QJsonArray>
#include <QHash>
#include <QDateTime>
#include <functional>

namespace sc {
class YouTubeChat final : public QObject {
    Q_OBJECT
public:
    explicit YouTubeChat(QString path,QObject* parent=nullptr);
    ~YouTubeChat()override;
    void setVisible(bool active);void setStreamActive(bool active);
    void signIn();void signOut();void discover();void selectChat(const QString& id);void send(const QString& text);
    bool ready()const{return ready_;}bool signedIn()const{return !tokens_.value("access_token").toString().isEmpty();}QString status()const{return status_;}
    int viewerCount()const{return viewerCount_;}
    bool viewerFresh()const{return viewerUpdated_.isValid()&&viewerUpdated_.secsTo(QDateTime::currentDateTimeUtc())<180;}
Q_SIGNALS:
    void changed();void message(const sc::ChatMessage& message);void removeMessage(const QString& id);void broadcasts(const QJsonArray& choices);
    void sent(bool confirmed,const QString& reason);
private:
    using Reply=std::function<void(int,const QJsonObject&)>;
    void request(const QUrl& url,const QByteArray& method,const QByteArray& body,bool authorized,Reply callback);
    void setStatus(const QString& value);bool saveTokens(const QJsonObject& tokens);void refresh();
    void openStream();void stopStream();void stopNetwork();void receive();void retry();
    void pollViewers();
    SecureStore vault_;QJsonObject tokens_;QNetworkAccessManager http_;OAuthLoopback login_;
    QSet<QNetworkReply*> replies_;QPointer<QNetworkReply> stream_;youtube_wire::Frames frames_;
    QTimer retry_,expiry_,search_,viewerTimer_;QString status_="YouTube: не подключён",verifier_,chatId_,pageToken_,videoId_;
    QHash<QString,QString> chatVideos_;QDateTime viewerUpdated_;int viewerCount_=-1;
    QQueue<QString> seenOrder_;QSet<QString> seen_;quint64 generation_=0;
    bool visible_=false,ready_=false,refreshing_=false,finding_=false,streamActive_=false,viewerRequest_=false;int attempts_=0,authRetries_=0;
};
}

