#include "twitch_chat.hpp"
#include "traffic.hpp"
#include "usage.hpp"
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QJsonArray>
#include <QDesktopServices>
#include <algorithm>

namespace sc {
namespace {
const QString clientId="6ivhu6zdlv5ojd0h8w4s037uc7rmgh"; // Public desktop client, not a secret.
const QString scopes="user:read:chat user:write:chat";
QByteArray form(std::initializer_list<std::pair<QString,QString>> fields){QUrlQuery q;for(const auto& field:fields)q.addQueryItem(field.first,field.second);return q.query(QUrl::FullyEncoded).toUtf8().replace("+","%2B");}
}
TwitchChat::TwitchChat(QString path,QObject* parent):QObject(parent),vault_(std::move(path)) {
    bool ok=false;tokens_=vault_.load(&ok);if(!ok)status_="Twitch: не удалось прочитать защищённые настройки";
    deviceTimer_.setSingleShot(true);retryTimer_.setSingleShot(true);watchdog_.setSingleShot(true);
    validationTimer_.setInterval(3600000);
    viewerTimer_.setInterval(30000);
    connect(&deviceTimer_,&QTimer::timeout,this,&TwitchChat::pollDevice);
    connect(&retryTimer_,&QTimer::timeout,this,[this]{validate();});
    connect(&validationTimer_,&QTimer::timeout,this,[this]{validate();});
    connect(&watchdog_,&QTimer::timeout,this,&TwitchChat::retry);
    connect(&viewerTimer_,&QTimer::timeout,this,&TwitchChat::pollViewers);
}
TwitchChat::~TwitchChat(){stopNetwork();}
void TwitchChat::setStatus(const QString& value){if(status_!=value){status_=value;Q_EMIT changed();}}
void TwitchChat::request(const QUrl& url,const QByteArray& method,const QByteArray& body,const QByteArray& type,bool auth,Reply done){
    QNetworkRequest req(url);req.setTransferTimeout(15000);req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader,"StreamChatOBS/0.3");
    if(!type.isEmpty())req.setHeader(QNetworkRequest::ContentTypeHeader,type);
    if(auth){const QString scheme=url.host()=="id.twitch.tv"&&url.path()=="/oauth2/validate"?"OAuth ":"Bearer ";req.setRawHeader("Authorization",(scheme+tokens_.value("access_token").toString()).toUtf8());req.setRawHeader("Client-Id",clientId.toUtf8());}
    traffic::addSent(body.size());auto* r=method=="GET"?http_.get(req):http_.sendCustomRequest(req,method,body);replies_.insert(r);
    const auto generation=generation_;
    connect(r,&QNetworkReply::readyRead,r,[r]{if(r->bytesAvailable()>1048576)r->abort();});
    connect(r,&QNetworkReply::finished,this,[this,r,generation,done=std::move(done)]{
        CpuScope cpu;
        replies_.remove(r);const int status=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto payload=r->readAll();traffic::addReceived(payload.size());const auto data=QJsonDocument::fromJson(payload).object();r->deleteLater();
        if(generation==generation_)done(status,data);
    });
}
void TwitchChat::stopNetwork(){
    ++generation_;ready_=false;subscribed_=false;refreshing_=false;validating_=false;
    deviceTimer_.stop();retryTimer_.stop();validationTimer_.stop();watchdog_.stop();viewerTimer_.stop();viewerRequest_=false;deviceCode_.clear();
    if(replacement_){replacement_->disconnect(this);replacement_->abort();replacement_->deleteLater();replacement_=nullptr;}
    if(socket_){socket_->disconnect(this);socket_->abort();socket_->deleteLater();socket_=nullptr;}
    const auto pending=replies_.values();replies_.clear();for(auto* reply:pending)reply->abort();
}
void TwitchChat::setVisible(bool active){
    if(visible_==active)return;visible_=active;
    if(!active){stopNetwork();setStatus("Twitch: приостановлен");}
    else if(!tokens_.value("access_token").toString().isEmpty())validate();
    else setStatus("Twitch: не подключён");
    Q_EMIT changed();
}
void TwitchChat::setStreamActive(bool active){
    if(streamActive_==active)return;streamActive_=active;
    if(!active){viewerTimer_.stop();viewerCount_=-1;viewerUpdated_={};Q_EMIT changed();return;}
    if(visible_&&!userId_.isEmpty()&&!tokens_.value("access_token").toString().isEmpty()){
        viewerTimer_.setInterval(30000);viewerTimer_.start();pollViewers();
    }
}
void TwitchChat::pollViewers(){
    if(!visible_||!streamActive_||viewerRequest_||userId_.isEmpty()||tokens_.value("access_token").toString().isEmpty())return;
    viewerRequest_=true;QUrl url("https://api.twitch.tv/helix/streams");QUrlQuery query;query.addQueryItem("user_id",userId_);url.setQuery(query);
    request(url,"GET",{}, {},true,[this](int http,const QJsonObject& data){
        viewerRequest_=false;
        if(!streamActive_)return;
        if(http==401){refresh();return;}
        if(http==429){viewerTimer_.setInterval(120000);Q_EMIT changed();return;}
        if(http!=200){Q_EMIT changed();return;}
        viewerTimer_.setInterval(30000);
        const auto streams=data.value("data").toArray();
        viewerCount_=streams.isEmpty()?-1:streams.first().toObject().value("viewer_count").toInt(-1);
        viewerUpdated_=QDateTime::currentDateTimeUtc();Q_EMIT changed();
    });
}
void TwitchChat::signIn(){
    stopNetwork();setStatus("Twitch: получаем код входа…");
    request(QUrl("https://id.twitch.tv/oauth2/device"),"POST",form({{"client_id",clientId},{"scopes",scopes}}),"application/x-www-form-urlencoded",false,[this](int http,const QJsonObject& data){
        const QUrl url(data.value("verification_uri").toString());
        if(http!=200||data.value("device_code").toString().isEmpty()||url.scheme()!="https"||url.host()!="www.twitch.tv"){
            setStatus(QString("Twitch: ошибка получения кода (HTTP %1)").arg(http));return;
        }
        deviceCode_=data.value("device_code").toString();deviceInterval_=std::clamp(data.value("interval").toInt(5),5,60);
        deviceExpires_=QDateTime::currentDateTimeUtc().addSecs(std::clamp(data.value("expires_in").toInt(1800),30,1800));
        setStatus("Twitch: подтвердите вход в браузере");Q_EMIT signInCode(data.value("user_code").toString(),url);
        if(!QDesktopServices::openUrl(url)){setStatus("Twitch: откройте ссылку входа вручную");}
        deviceTimer_.start(deviceInterval_*1000);
    });
}
void TwitchChat::pollDevice(){
    if(deviceCode_.isEmpty())return;
    if(QDateTime::currentDateTimeUtc()>=deviceExpires_){deviceCode_.clear();setStatus("Twitch: время входа истекло");return;}
    request(QUrl("https://id.twitch.tv/oauth2/token"),"POST",form({{"client_id",clientId},{"scopes",scopes},{"device_code",deviceCode_},{"grant_type","urn:ietf:params:oauth:grant-type:device_code"}}),"application/x-www-form-urlencoded",false,[this](int http,const QJsonObject& data){
        if(http==200&&saveTokens(data)){deviceCode_.clear();validate();return;}
        const auto reason=data.value("message").toString(data.value("error").toString());
        if(reason=="authorization_pending"||reason=="slow_down"||http==429){if(reason=="slow_down"||http==429)deviceInterval_=std::min(60,deviceInterval_+5);deviceTimer_.start(deviceInterval_*1000);return;}
        deviceCode_.clear();setStatus(QString("Twitch: вход не завершён (HTTP %1), попробуйте снова").arg(http));
    });
}
bool TwitchChat::saveTokens(const QJsonObject& data){
    if(data.value("access_token").toString().isEmpty())return false;
    auto updated=data;if(updated.value("refresh_token").toString().isEmpty())updated["refresh_token"]=tokens_.value("refresh_token");
    if(!vault_.save(updated)){setStatus("Twitch: не удалось защитить и сохранить токен");return false;}
    tokens_=updated;return true;
}
void TwitchChat::validate(bool allowRefresh){
    if(validating_||refreshing_||tokens_.value("access_token").toString().isEmpty())return;
    validating_=true;
    request(QUrl("https://id.twitch.tv/oauth2/validate"),"GET",{}, {},true,[this,allowRefresh](int http,const QJsonObject& data){
        validating_=false;
        if(http==401&&allowRefresh){refresh();return;}
        const auto granted=data.value("scopes").toArray();
        if(http!=200||data.value("client_id").toString()!=clientId||data.value("user_id").toString().isEmpty()||!granted.contains("user:read:chat")||!granted.contains("user:write:chat")){
            ready_=false;Q_EMIT changed();
            if(http==401||http==200){stopNetwork();tokens_={};vault_.save({});setStatus("Twitch: требуется повторный вход с разрешениями чата");}
            else {setStatus(QString("Twitch: проверка соединения (HTTP %1)").arg(http));retry();}
            return;
        }
        userId_=data.value("user_id").toString();validationTimer_.start();
        if(visible_&&streamActive_&&!viewerTimer_.isActive()){viewerTimer_.start();pollViewers();}
        if(visible_&&!socket_){setStatus("Twitch: подключаем чат "+data.value("login").toString());openSocket();}
        else if(visible_&&socket_&&subscribed_){ready_=true;setStatus("Twitch: чат подключён");Q_EMIT changed();}
        else if(!visible_)setStatus("Twitch: вход выполнен, чат приостановлен");
    });
}
void TwitchChat::refresh(){
    if(refreshing_)return;const auto refreshToken=tokens_.value("refresh_token").toString();
    if(refreshToken.isEmpty()){stopNetwork();setStatus("Twitch: требуется повторный вход");return;}
    refreshing_=true;ready_=false;Q_EMIT changed();
    request(QUrl("https://id.twitch.tv/oauth2/token"),"POST",form({{"client_id",clientId},{"grant_type","refresh_token"},{"refresh_token",refreshToken}}),"application/x-www-form-urlencoded",false,[this](int http,const QJsonObject& data){
        refreshing_=false;
        if(http==200&&saveTokens(data)){validate(false);return;}
        if(http==400||http==401){stopNetwork();tokens_={};vault_.save({});setStatus("Twitch: разрешение истекло, войдите снова");}
        else {setStatus("Twitch: не удалось обновить подключение");retry();}
    });
}
void TwitchChat::signOut(){stopNetwork();tokens_={};userId_.clear();viewerCount_=-1;viewerUpdated_={};if(!vault_.save({}))setStatus("Twitch: ошибка удаления локального токена");else setStatus("Twitch: отключён");Q_EMIT changed();}
void TwitchChat::openSocket(const QUrl& url,bool migration){
    if(!visible_||url.scheme()!="wss"||url.host()!="eventsub.wss.twitch.tv"||!url.userInfo().isEmpty()||(url.port()!=-1&&url.port()!=443))return;
    auto* ws=new QWebSocket(QString(),QWebSocketProtocol::VersionLatest,this);
    ws->setMaxAllowedIncomingMessageSize(262144);ws->setMaxAllowedIncomingFrameSize(262144);
    if(migration)replacement_=ws;else socket_=ws;
    const auto generation=generation_;
    connect(ws,&QWebSocket::textMessageReceived,this,[this,ws,migration,generation](const QString& text){if(generation==generation_)receive(ws,text,migration);});
    connect(ws,&QWebSocket::disconnected,this,[this,ws,generation]{if(generation==generation_&&(socket_==ws||replacement_==ws))retry();});
    connect(ws,&QWebSocket::errorOccurred,this,[this,ws,generation](QAbstractSocket::SocketError){if(generation==generation_&&(socket_==ws||replacement_==ws))retry();});
    watchdog_.start(15000);ws->open(url);
}
void TwitchChat::receive(QWebSocket* ws,const QString& raw,bool migration){
    CpuScope cpu;
    const auto bytes=raw.toUtf8();traffic::addReceived(bytes.size());const auto packet=QJsonDocument::fromJson(bytes).object();const auto metadata=packet.value("metadata").toObject();
    const auto type=metadata.value("message_type").toString();const auto payload=packet.value("payload").toObject();
    if(type=="session_welcome"){
        const auto session=payload.value("session").toObject();if(session.value("id").toString().isEmpty()){retry();return;}
        if(migration&&replacement_==ws){auto old=socket_;socket_=ws;replacement_=nullptr;if(old){old->disconnect(this);old->close();old->deleteLater();}}
        keepalive_=std::clamp(session.value("keepalive_timeout_seconds").toInt(10),10,600);watchdog_.start((keepalive_+5)*1000);
        if(!migration)subscribe(session.value("id").toString());return;
    }
    if(ws!=socket_)return;
    if(!type.isEmpty())watchdog_.start((keepalive_+5)*1000);
    if(type=="session_reconnect"){
        const QUrl url(payload.value("session").toObject().value("reconnect_url").toString());
        if(url.scheme()!="wss"||url.host()!="eventsub.wss.twitch.tv"){retry();return;}
        if(!replacement_)openSocket(url,true);return;
    }
    if(type=="revocation"){stopNetwork();setStatus("Twitch: подписка отозвана, войдите снова");Q_EMIT changed();return;}
    if(type!="notification")return;
    const auto id=metadata.value("message_id").toString();if(id.isEmpty()||seen_.contains(id))return;
    seen_.insert(id);seenOrder_.enqueue(id);while(seenOrder_.size()>1000)seen_.remove(seenOrder_.dequeue());
    const auto event=payload.value("event").toObject();const auto subscription=payload.value("subscription").toObject().value("type").toString();
    if(subscription=="channel.chat.message"){
        const auto body=event.value("message").toObject();
        ChatMessage message{"Twitch",event.value("message_id").toString(),event.value("chatter_user_id").toString(),event.value("chatter_user_name").toString().left(100),body.value("text").toString().left(5000)};
        message.sentAt=parsePlatformTime(metadata.value("message_timestamp").toString());
        int remaining=5000;
        for(const auto& item:body.value("fragments").toArray()){
            if(message.fragments.size()>=100||remaining<=0)break;
            const auto fragment=item.toObject();const auto part=fragment.value("text").toString().left(remaining);remaining-=part.size();
            QString emoteId;
            if(fragment.value("type").toString()=="emote"){
                const auto candidate=fragment.value("emote").toObject().value("id").toString();
                if(!candidate.isEmpty()&&candidate.size()<=64){
                    bool valid=true;for(const auto ch:candidate)if(!ch.isLetterOrNumber()&&ch!='-'&&ch!='_'){valid=false;break;}
                    if(valid)emoteId=candidate;
                }
            }
            message.fragments.append({part,emoteId});
        }
        if(!message.id.isEmpty())Q_EMIT this->message(message);
    } else if(subscription=="channel.chat.message_delete")Q_EMIT removeMessage(event.value("message_id").toString());
    else if(subscription=="channel.chat.clear_user_messages")Q_EMIT clearAuthor(event.value("target_user_id").toString());
    else if(subscription=="channel.chat.clear")Q_EMIT clearChat();
}
void TwitchChat::subscribe(const QString& session){
    const QStringList types={"channel.chat.message","channel.chat.message_delete","channel.chat.clear_user_messages","channel.chat.clear"};
    for(const auto& type:types){
        const QPointer<QWebSocket> expected=socket_;
        const QJsonObject body{{"type",type},{"version","1"},{"condition",QJsonObject{{"broadcaster_user_id",userId_},{"user_id",userId_}}},{"transport",QJsonObject{{"method","websocket"},{"session_id",session}}}};
        request(QUrl("https://api.twitch.tv/helix/eventsub/subscriptions"),"POST",QJsonDocument(body).toJson(QJsonDocument::Compact),"application/json",true,[this,type,expected](int http,const QJsonObject&){
            if(!expected||socket_!=expected)return;
            if(http==401){ready_=false;Q_EMIT changed();refresh();return;}
            if(http!=202){if(type=="channel.chat.message"){ready_=false;setStatus(QString("Twitch: подписка не создана (HTTP %1)").arg(http));retry();}return;}
            if(type=="channel.chat.message"){ready_=true;subscribed_=true;retries_=0;setStatus("Twitch: чат подключён");Q_EMIT changed();}
        });
    }
}
void TwitchChat::retry(){
    if(!visible_||retryTimer_.isActive())return;
    if(socket_){socket_->disconnect(this);socket_->abort();socket_->deleteLater();socket_=nullptr;}
    if(replacement_){replacement_->disconnect(this);replacement_->abort();replacement_->deleteLater();replacement_=nullptr;}
    ready_=false;subscribed_=false;watchdog_.stop();Q_EMIT changed();
    retries_=std::min(retries_+1,6);setStatus("Twitch: подключение");retryTimer_.start(std::min(120000,1000*(1<<retries_)));
}
void TwitchChat::send(const QString& text){
    if(!ready_||text.trimmed().isEmpty()||text.size()>500){Q_EMIT sent(false,"Twitch: чат не готов или текст длиннее 500 символов");return;}
    const QJsonObject body{{"broadcaster_id",userId_},{"sender_id",userId_},{"message",text}};
    request(QUrl("https://api.twitch.tv/helix/chat/messages"),"POST",QJsonDocument(body).toJson(QJsonDocument::Compact),"application/json",true,[this](int http,const QJsonObject& data){
        const auto rows=data.value("data").toArray();const bool ok=http==200&&!rows.isEmpty()&&rows.first().toObject().value("is_sent").toBool();
        Q_EMIT sent(ok,ok?"Twitch: сообщение отправлено":QString("Twitch: отправка не подтверждена (HTTP %1), автоматического повтора нет").arg(http));
        if(http==401)refresh();
    });
}
}
