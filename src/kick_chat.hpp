#pragma once
#include "secure_store.hpp"
#include "oauth_loopback.hpp"
#include "kick_wire.hpp"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QWebSocket>
#include <QTimer>
#include <QSet>
#include <QQueue>
#include <functional>

namespace sc {
class KickChat final : public QObject {
    Q_OBJECT
public:
    explicit KickChat(QString path,QObject* parent=nullptr);
    ~KickChat() override;
    void setEnabled(bool enabled);void setVisible(bool visible);
    bool configure(const QString& clientId,const QString& clientSecret,const QString& channel);
    void signIn();void signOut();void send(const QString& text);
    bool ready()const{return ready_;}
    bool signedIn()const{return !account_.value("access_token").toString().isEmpty();}
    bool canSend()const{return enabled_&&visible_&&ready_&&authorized_&&!refreshing_;}
    bool busy()const{return login_.active()||authorizing_;}
    QString status(bool english=false)const;
    QString clientId()const{return account_.value("client_id").toString();}
    QString clientSecret()const{return account_.value("client_secret").toString();}
    QString channel()const{return account_.value("channel").toString();}
    QString accountLogin()const{return account_.value("account_name").toString();}
    static QString callback(){return "http://localhost:38763/kick/callback";}
Q_SIGNALS:
    void changed();void message(const sc::ChatMessage& value);void removeMessage(const QString& id);
    void sent(bool confirmed,const QString& reason);
private:
    using Reply=std::function<void(int,const QJsonObject&)>;
    void request(const QUrl& url,const QByteArray& method,const QByteArray& body,bool auth,Reply callback);
    void stop();void start();void resolve();void openSocket();void retry();void status(const QString& text);
    bool saveTokens(const QJsonObject& tokens);void refreshToken();void validateAccount(bool allowRefresh=true);
    void receive(const QString& text);void sendFrame(const QJsonObject& frame);
    bool active()const{return enabled_&&visible_;}
    SecureStore vault_;QJsonObject account_;QNetworkAccessManager http_;OAuthLoopback login_;
    QSet<QNetworkReply*> replies_;QWebSocket* socket_=nullptr;
    QTimer retry_,heartbeat_,expiry_;QSet<QString> seen_;QQueue<QString> seenOrder_;
    QString status_="Kick: не подключён",verifier_,roomId_,broadcasterId_;
    quint64 generation_=0;int attempts_=0;bool enabled_=false,visible_=false,ready_=false,authorized_=false;
    bool resolving_=false,refreshing_=false,authorizing_=false,sending_=false,awaitingPong_=false;
};
}
