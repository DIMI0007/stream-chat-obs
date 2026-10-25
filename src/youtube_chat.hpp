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
#include <functional>

namespace sc {
class YouTubeChat final : public QObject {
    Q_OBJECT
public:
    explicit YouTubeChat(QString path,QObject* parent=nullptr);
    ~YouTubeChat()override;
    void setVisible(bool active);
    void signIn();void signOut();void discover();void selectChat(const QString& id);void send(const QString& text);
    bool ready()const{return ready_;}QString status()const{return status_;}
Q_SIGNALS:
    void changed();void message(const sc::ChatMessage& message);void removeMessage(const QString& id);void broadcasts(const QJsonArray& choices);
    void sent(bool confirmed,const QString& reason);
private:
    using Reply=std::function<void(int,const QJsonObject&)>;
    void request(const QUrl& url,const QByteArray& method,const QByteArray& body,bool authorized,Reply callback);
    void setStatus(const QString& value);bool saveTokens(const QJsonObject& tokens);void refresh();
    void openStream();void stopStream();void stopNetwork();void receive();void retry();
    SecureStore vault_;QJsonObject tokens_;QNetworkAccessManager http_;OAuthLoopback login_;
    QSet<QNetworkReply*> replies_;QPointer<QNetworkReply> stream_;youtube_wire::Frames frames_;
    QTimer retry_,expiry_;QString status_="YouTube: не подключён",verifier_,chatId_,pageToken_;
    QQueue<QString> seenOrder_;QSet<QString> seen_;quint64 generation_=0;
    bool visible_=false,ready_=false,refreshing_=false,finding_=false;int attempts_=0,authRetries_=0;
};
}

