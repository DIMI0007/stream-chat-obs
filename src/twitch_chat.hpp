#pragma once
#include "secure_store.hpp"
#include "chat_message.hpp"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QWebSocket>
#include <QTimer>
#include <QPointer>
#include <QSet>
#include <QQueue>
#include <QDateTime>
#include <functional>

namespace sc {
class TwitchChat final : public QObject {
    Q_OBJECT
public:
    explicit TwitchChat(QString tokenPath,QObject* parent=nullptr);
    ~TwitchChat() override;
    void setVisible(bool active);
    void setStreamActive(bool active);
    void signIn();
    void signOut();
    bool configure(const QString& clientId);
    QString clientId()const{return tokens_.value("client_id").toString();}
    bool configured()const{return !clientId().isEmpty();}
    void send(const QString& text);
    bool ready()const{return ready_;}
    QString accountLogin()const{return accountLogin_;}
    QString status()const{return status_;}
    int viewerCount()const{return viewerCount_;}
    bool viewerFresh()const{return viewerUpdated_.isValid()&&viewerUpdated_.secsTo(QDateTime::currentDateTimeUtc())<90;}
Q_SIGNALS:
    void changed();
    void message(const sc::ChatMessage& message);
    void removeMessage(const QString& id);
    void clearAuthor(const QString& id);
    void clearChat();
    void signInCode(const QString& code,const QUrl& url);
    void sent(bool confirmed,const QString& reason);
private:
    using Reply=std::function<void(int,const QJsonObject&)>;
    void request(const QUrl& url,const QByteArray& method,const QByteArray& body,const QByteArray& contentType,bool authorized,Reply done);
    void setStatus(const QString& value);
    void validate(bool allowRefresh=true);
    void refresh();
    bool saveTokens(const QJsonObject& tokens);
    bool clearTokens();
    void pollDevice();
    void openSocket(const QUrl& url=QUrl("wss://eventsub.wss.twitch.tv/ws"),bool migration=false);
    void receive(QWebSocket* socket,const QString& text,bool migration);
    void subscribe(const QString& session);
    void retry();
    void pollViewers();
    void stopNetwork();
    SecureStore vault_;QJsonObject tokens_;
    QNetworkAccessManager http_;QSet<QNetworkReply*> replies_;
    QPointer<QWebSocket> socket_,replacement_;
    QTimer deviceTimer_,retryTimer_,validationTimer_,watchdog_,viewerTimer_;
    QQueue<QString> seenOrder_;QSet<QString> seen_;
    QString userId_,accountLogin_,status_="Twitch: не подключён",deviceCode_;
    QDateTime deviceExpires_;quint64 generation_=0;
    QDateTime viewerUpdated_;int viewerCount_=-1;
    int deviceInterval_=5,retries_=0,keepalive_=10;
    bool visible_=false,ready_=false,subscribed_=false,refreshing_=false,validating_=false,streamActive_=false,viewerRequest_=false;
};
}

