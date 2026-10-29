#include "kick_chat.hpp"
#include "traffic.hpp"
#include "usage.hpp"
#include <QDesktopServices>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrlQuery>
#include <QRandomGenerator>
#include <algorithm>

namespace sc {
namespace {
QByteArray form(std::initializer_list<std::pair<QString,QString>> fields){QUrlQuery q;for(const auto& v:fields)q.addQueryItem(v.first,v.second);return q.query(QUrl::FullyEncoded).toUtf8().replace("+","%2B");}
const QString scopes="user:read channel:read chat:write";
}
KickChat::KickChat(QString path,QObject* parent):QObject(parent),vault_(std::move(path)),login_(this){
    bool ok=false;account_=vault_.load(&ok);if(!ok)status_="Kick: не удалось прочитать защищённые настройки";
    retry_.setSingleShot(true);expiry_.setSingleShot(true);heartbeat_.setInterval(20000);
    connect(&retry_,&QTimer::timeout,this,&KickChat::start);
    connect(&expiry_,&QTimer::timeout,this,&KickChat::refreshToken);
    connect(&heartbeat_,&QTimer::timeout,this,[this]{
        if(!socket_)return;if(awaitingPong_){retry();return;}
        awaitingPong_=true;sendFrame({{"event","pusher:ping"},{"data",QJsonObject{}}});
    });
    connect(&login_,&OAuthLoopback::failed,this,[this](const QString&){authorizing_=false;status("Kick: вход отменён или время истекло");});
    connect(&login_,&OAuthLoopback::received,this,[this](const QUrlQuery& query){
        if(!active())return;authorizing_=true;status("Kick: завершаем вход…");
        const auto body=form({{"grant_type","authorization_code"},{"client_id",clientId()},{"client_secret",clientSecret()},
            {"redirect_uri",callback()},{"code",query.queryItemValue("code",QUrl::FullyDecoded)},{"code_verifier",verifier_}});
        verifier_.clear();
        request(QUrl("https://id.kick.com/oauth/token"),"POST",body,false,[this](int http,const QJsonObject& data){
            authorizing_=false;
            if(http!=200||!saveTokens(data)){status(QString("Kick: ошибка входа (HTTP %1)").arg(http));return;}
            validateAccount();
        });
    });
}
KickChat::~KickChat(){stop();}
QString KickChat::status(bool english)const{
    if(!english)return status_;
    const std::pair<const char*,const char*> translations[]={
        {"не удалось прочитать защищённые настройки","Could not read encrypted settings"},
        {"вход отменён или время истекло","Sign-in cancelled or timed out"},
        {"завершаем вход…","Completing sign-in…"},{"ошибка входа","Sign-in error"},
        {"отключён","Disabled"},{"приостановлен","Paused"},
        {"ошибка — укажите имя канала без ссылки","Enter a channel name without a link"},
        {"не удалось сохранить защищённые настройки","Could not save encrypted settings"},
        {"укажите канал в настройках","Set a channel in Settings"},{"подключаем чат…","Connecting chat…"},
        {"ошибка получения канала","Could not load channel"},{"ошибка формата канала","Invalid channel response"},
        {"восстанавливаем соединение…","Reconnecting…"},{"чат подключён","Chat connected"},
        {"укажите Client ID и Client Secret в настройках","Enter Client ID and Client Secret in Settings"},
        {"не удалось открыть localhost:38763 — порт занят","localhost:38763 is unavailable — port busy"},
        {"подтвердите вход в браузере","Confirm sign-in in your browser"},{"не удалось открыть браузер","Could not open browser"},
        {"требуется повторный вход","Sign in again"},
        {"ошибка обновления входа; чтение чата доступно","Could not refresh sign-in; chat reading remains available"},
        {"ошибка проверки аккаунта","Account verification error"},{"не удалось сохранить аккаунт","Could not save account"},
        {"не удалось удалить сохранённый вход","Could not remove saved sign-in"},{"не подключён","Not connected"}};
    auto result=status_;for(const auto& entry:translations)result.replace(QString::fromUtf8(entry.first),QString::fromUtf8(entry.second));return result;
}
void KickChat::status(const QString& value){if(status_!=value){status_=value;Q_EMIT changed();}}
void KickChat::request(const QUrl& url,const QByteArray& method,const QByteArray& body,bool auth,Reply callback){
    if(!active())return;
    if(url.scheme()!="https"||!QStringList{"kick.com","id.kick.com","api.kick.com"}.contains(url.host()))return;
    QNetworkRequest req(url);req.setTransferTimeout(15000);req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader,"StreamChatOBS/Kick");req.setRawHeader("Accept","application/json");
    if(auth)req.setRawHeader("Authorization",("Bearer "+account_.value("access_token").toString()).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader,url.host()=="id.kick.com"?"application/x-www-form-urlencoded":"application/json");
    traffic::addSent(body.size());auto* reply=method=="GET"?http_.get(req):http_.sendCustomRequest(req,method,body);replies_.insert(reply);
    const auto generation=generation_;connect(reply,&QNetworkReply::readyRead,reply,[reply]{if(reply->bytesAvailable()>1048576)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,callback=std::move(callback)]{
        CpuScope cpu;replies_.remove(reply);const auto bytes=reply->readAll();traffic::addReceived(bytes.size());
        const int http=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool transportOk=reply->error()==QNetworkReply::NoError;
        const auto data=QJsonDocument::fromJson(bytes).object();reply->deleteLater();
        if(generation==generation_&&active())callback(transportOk||http>=400?http:0,data);
    });
}
void KickChat::stop(){
    ++generation_;login_.stop();verifier_.clear();retry_.stop();heartbeat_.stop();expiry_.stop();
    ready_=false;authorized_=false;resolving_=false;refreshing_=false;authorizing_=false;awaitingPong_=false;
    if(socket_){socket_->disconnect(this);socket_->abort();socket_->deleteLater();socket_=nullptr;}
    const auto pending=replies_.values();replies_.clear();for(auto* reply:pending)reply->abort();
    if(sending_){sending_=false;Q_EMIT sent(false,"Kick: результат отправки не подтверждён. Автоматического повтора нет.");}
}
void KickChat::setEnabled(bool value){if(enabled_==value)return;enabled_=value;stop();if(active())start();else status("Kick: отключён");Q_EMIT changed();}
void KickChat::setVisible(bool value){if(visible_==value)return;visible_=value;stop();if(active())start();else status(enabled_?"Kick: приостановлен":"Kick: отключён");Q_EMIT changed();}
bool KickChat::configure(const QString& id,const QString& secret,const QString& slug){
    const auto channelName=slug.trimmed().toLower();
    if(!channelName.isEmpty()&&!kick_wire::validChannel(channelName)){status("Kick: ошибка — укажите имя канала без ссылки");return false;}
    const bool credentials=id.trimmed()!=clientId()||secret!=clientSecret();const bool changedChannel=channelName!=channel();
    if(!credentials&&!changedChannel)return true;
    auto next=account_;next["client_id"]=id.trimmed();next["client_secret"]=secret;next["channel"]=channelName;
    if(credentials){for(const auto* key:{"access_token","refresh_token","expires_at","account_id","account_name","scope"})next.remove(key);}
    if(!vault_.save(next)){status("Kick: не удалось сохранить защищённые настройки");return false;}
    stop();account_=next;roomId_.clear();broadcasterId_.clear();seen_.clear();seenOrder_.clear();attempts_=0;if(active())start();Q_EMIT changed();return true;
}
void KickChat::start(){
    if(!active())return;
#ifdef STREAM_CHAT_PREVIEW
    status("Kick: укажите канал в настройках");return;
#endif
    if(!signedIn()&&channel().isEmpty()){status("Kick: укажите канал в настройках");return;}
    if(signedIn()&&!authorized_&&!refreshing_&&!authorizing_){
        if(account_.value("expires_at").toVariant().toLongLong()<=QDateTime::currentSecsSinceEpoch()+60)refreshToken();else validateAccount();
    }
    if(!channel().isEmpty()&&!ready_&&!resolving_&&!socket_)resolve();
}
void KickChat::resolve(){
    if(!active()||resolving_||channel().isEmpty())return;resolving_=true;status("Kick: подключаем чат…");
    request(QUrl("https://kick.com/api/v2/channels/"+channel()),"GET",{},false,[this](int http,const QJsonObject& data){
        resolving_=false;
        if(http!=200){status(QString("Kick: ошибка получения канала (HTTP %1)").arg(http));if(http!=404)retry();return;}
        roomId_=data.value("chatroom").toObject().value("id").toVariant().toString();broadcasterId_=data.value("user_id").toVariant().toString();
        static const QRegularExpression numeric("^[0-9]{1,20}$");
        if(!numeric.match(roomId_).hasMatch()||!numeric.match(broadcasterId_).hasMatch()){status("Kick: ошибка формата канала");return;}
        openSocket();
    });
}
void KickChat::openSocket(){
    if(!active()||socket_)return;
    auto* ws=new QWebSocket(QString(),QWebSocketProtocol::VersionLatest,this);socket_=ws;
    ws->setMaxAllowedIncomingFrameSize(1048576);ws->setMaxAllowedIncomingMessageSize(1048576);
    connect(ws,&QWebSocket::textMessageReceived,this,&KickChat::receive);
    connect(ws,&QWebSocket::disconnected,this,[this,ws]{if(socket_==ws)retry();});
    connect(ws,&QWebSocket::errorOccurred,this,[this,ws](QAbstractSocket::SocketError){if(socket_==ws)retry();});
    connect(ws,&QWebSocket::connected,this,[this]{sendFrame({{"event","pusher:subscribe"},{"data",QJsonObject{{"auth",""},{"channel","chatrooms."+roomId_+".v2"}}}});});
    QNetworkRequest req(QUrl("wss://ws-us2.pusher.com/app/34bf7a0ff419a2a775b9?protocol=7&client=js&version=8.4.0&flash=false"));req.setRawHeader("Origin","https://kick.com");ws->open(req);
    QTimer::singleShot(15000,ws,[this,ws]{if(socket_==ws&&!ready_)retry();});
}
void KickChat::sendFrame(const QJsonObject& frame){if(socket_&&socket_->state()==QAbstractSocket::ConnectedState){const auto text=QString::fromUtf8(QJsonDocument(frame).toJson(QJsonDocument::Compact));traffic::addSent(text.toUtf8().size());socket_->sendTextMessage(text);}}
void KickChat::retry(){
    ready_=false;heartbeat_.stop();awaitingPong_=false;
    if(socket_){auto* old=socket_;socket_=nullptr;old->disconnect(this);old->abort();old->deleteLater();}
    if(active()&&!retry_.isActive()){
        const int delay=std::min(120000,2000*(1<<std::min(attempts_++,6)))+int(QRandomGenerator::global()->bounded(800));
        retry_.start(delay);status("Kick: восстанавливаем соединение…");
    }
    Q_EMIT changed();
}
void KickChat::receive(const QString& text){
    CpuScope cpu;traffic::addReceived(text.toUtf8().size());const auto root=QJsonDocument::fromJson(text.toUtf8()).object();const auto event=root.value("event").toString();
    if(event=="pusher:pong"){awaitingPong_=false;return;}
    if(event=="pusher:ping"){sendFrame({{"event","pusher:pong"},{"data",QJsonObject{}}});return;}
    const auto room="chatrooms."+roomId_+".v2";
    if(event=="pusher_internal:subscription_succeeded"&&root.value("channel").toString()==room){ready_=true;attempts_=0;awaitingPong_=false;heartbeat_.start();status("Kick: чат подключён");Q_EMIT changed();return;}
    if(event=="pusher:error"||event=="pusher:subscription_error"){retry();return;}
    if(root.value("channel").toString()!=room)return;
    const auto raw=root.value("data");const auto data=raw.isString()?QJsonDocument::fromJson(raw.toString().toUtf8()).object():raw.toObject();
    if(event=="App\\Events\\ChatMessageEvent"){
        const auto m=kick_wire::message(data);if(m.id.isEmpty()||seen_.contains(m.id))return;
        seen_.insert(m.id);seenOrder_.enqueue(m.id);while(seenOrder_.size()>1000)seen_.remove(seenOrder_.dequeue());Q_EMIT message(m);
    }else if(event=="App\\Events\\MessageDeletedEvent"){
        const auto id=data.value("message").toObject().value("id").toString();if(!id.isEmpty())Q_EMIT removeMessage(id);
    }
}
void KickChat::signIn(){
    if(!active())return;if(clientId().isEmpty()||clientSecret().isEmpty()){status("Kick: укажите Client ID и Client Secret в настройках");return;}
    if(busy())return;
    if(!login_.begin(38763,"/kick/callback",false,true)){status("Kick: не удалось открыть localhost:38763 — порт занят");return;}
    QByteArray entropy;for(int i=0;i<8;++i)entropy+=QByteArray::number(QRandomGenerator::system()->generate64(),16);
    verifier_=QString::fromLatin1(entropy.toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals).left(96));
    const auto challenge=QCryptographicHash::hash(verifier_.toUtf8(),QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals);
    QUrl url("https://id.kick.com/oauth/authorize");QUrlQuery q;q.addQueryItem("response_type","code");q.addQueryItem("client_id",clientId());q.addQueryItem("redirect_uri",callback());q.addQueryItem("scope",scopes);q.addQueryItem("state",login_.state());q.addQueryItem("code_challenge",QString::fromLatin1(challenge));q.addQueryItem("code_challenge_method","S256");url.setQuery(q);
    status("Kick: подтвердите вход в браузере");if(!QDesktopServices::openUrl(url)){login_.stop();status("Kick: не удалось открыть браузер");}Q_EMIT changed();
}
bool KickChat::saveTokens(const QJsonObject& data){
    if(data.value("access_token").toString().isEmpty())return false;
    auto next=account_;next["access_token"]=data.value("access_token");if(!data.value("refresh_token").toString().isEmpty())next["refresh_token"]=data.value("refresh_token");
    if(data.contains("scope"))next["scope"]=data.value("scope");
    next["expires_at"]=QDateTime::currentSecsSinceEpoch()+std::clamp(data.value("expires_in").toInt(7200),60,86400);
    if(!vault_.save(next))return false;account_=next;return true;
}
void KickChat::refreshToken(){
    if(!active()||refreshing_)return;
    const auto refresh=account_.value("refresh_token").toString();
    if(refresh.isEmpty()){authorized_=false;Q_EMIT changed();return;}
    refreshing_=true;authorized_=false;
    request(QUrl("https://id.kick.com/oauth/token"),"POST",form({{"grant_type","refresh_token"},{"client_id",clientId()},{"client_secret",clientSecret()},{"refresh_token",refresh}}),false,[this](int http,const QJsonObject& data){
        refreshing_=false;
        if(http==200&&saveTokens(data)){validateAccount(false);return;}
        if(http==400||http==401){
            auto next=account_;for(const auto* key:{"access_token","refresh_token","expires_at","account_id","account_name","scope"})next.remove(key);
            if(vault_.save(next))account_=next;
            status("Kick: требуется повторный вход");
        }
        else {status("Kick: ошибка обновления входа; чтение чата доступно");expiry_.start(60000+int(QRandomGenerator::global()->bounded(5000)));}
        Q_EMIT changed();
    });
}
void KickChat::validateAccount(bool allowRefresh){
    if(!active()||authorizing_)return;authorizing_=true;
    request(QUrl("https://api.kick.com/public/v1/users"),"GET",{},true,[this,allowRefresh](int http,const QJsonObject& data){
        authorizing_=false;
        if(http==401&&allowRefresh){refreshToken();return;}
        if(http!=200||data.value("data").toArray().isEmpty()){authorized_=false;status(QString("Kick: ошибка проверки аккаунта (HTTP %1)").arg(http));Q_EMIT changed();return;}
        const auto user=data.value("data").toArray().first().toObject();auto next=account_;next["account_id"]=user.value("user_id").toVariant().toString();next["account_name"]=user.value("name").toString();
        if(channel().isEmpty())next["channel"]=user.value("name").toString().toLower();
        if(next.value("account_id").toString().isEmpty()||!kick_wire::validChannel(next.value("channel").toString())||!vault_.save(next)){status("Kick: не удалось сохранить аккаунт");return;}
        account_=next;authorized_=true;
        const auto remaining=account_.value("expires_at").toVariant().toLongLong()-QDateTime::currentSecsSinceEpoch()-60;
        expiry_.start(int(std::clamp<qint64>(remaining*1000,1000,86400000)));
        if(!ready_&&!socket_&&!resolving_)resolve();else if(ready_)status("Kick: чат подключён");Q_EMIT changed();
    });
}
void KickChat::signOut(){
    stop();auto next=account_;for(const auto* key:{"access_token","refresh_token","expires_at","account_id","account_name","scope"})next.remove(key);
    if(!vault_.save(next)){status("Kick: не удалось удалить сохранённый вход");return;}account_=next;if(active())start();Q_EMIT changed();
}
void KickChat::send(const QString& text){
    if(!canSend()||sending_){Q_EMIT sent(false,"Kick: отправка недоступна — проверьте вход");return;}
    if(!kick_wire::validMessage(text)){Q_EMIT sent(false,"Kick: предел сообщения — 500 символов и 2048 байт UTF-8");return;}
    sending_=true;const QJsonObject body{{"type","user"},{"broadcaster_user_id",broadcasterId_.toLongLong()},{"content",text}};
    request(QUrl("https://api.kick.com/public/v1/chat"),"POST",QJsonDocument(body).toJson(QJsonDocument::Compact),true,[this](int http,const QJsonObject& data){
        sending_=false;const auto result=data.value("data").toObject();const bool ok=http==200&&result.value("is_sent").toBool()&&!result.value("message_id").toString().isEmpty();
        Q_EMIT sent(ok,ok?"Kick: сообщение отправлено":QString("Kick: отправка не подтверждена (HTTP %1). Автоматического повтора нет.").arg(http));
        if(http==401){authorized_=false;refreshToken();}Q_EMIT changed();
    });
}
}
