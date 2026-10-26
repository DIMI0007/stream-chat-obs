#include "youtube_chat.hpp"
#include "traffic.hpp"
#include "usage.hpp"
#include <obs-module.h>
#include <util/bmem.h>
#include <QFile>
#include <QNetworkRequest>
#include <QDesktopServices>
#include <QCryptographicHash>
#include <QDateTime>
#include <QRandomGenerator>
#include <QUrlQuery>
#include <algorithm>

namespace sc {
namespace {
const QString googleClient="398266429510-m38h7aklj8s3r123fqmc64h33cdbmh3j.apps.googleusercontent.com";
QString googleSecret(){
    char* raw=obs_module_file("google-oauth.json");
    if(!raw)return {};
    QFile file(QString::fromUtf8(raw));bfree(raw);
    if(!file.open(QIODevice::ReadOnly))return {};
    const auto config=QJsonDocument::fromJson(file.readAll()).object();
    if(config.value("client_id").toString()!=googleClient)return {};
    return config.value("client_secret").toString();
}
QByteArray googleForm(std::initializer_list<std::pair<QString,QString>> fields){QUrlQuery q;for(const auto& field:fields)q.addQueryItem(field.first,field.second);return q.query(QUrl::FullyEncoded).toUtf8().replace("+","%2B");}
QString randomVerifier(){QString value;for(int i=0;i<4;++i)value+=QString::number(QRandomGenerator::system()->generate64(),16).rightJustified(16,'0');return value;}
QString oauthFailure(int http,const QJsonObject& data){
    const QString code=data.value("error").toString().left(64);
    if(code=="invalid_grant")return QString("YouTube: Google отклонил код входа — invalid_grant (HTTP %1)").arg(http);
    if(code=="invalid_client")return QString("YouTube: Google не принял приложение — invalid_client (HTTP %1)").arg(http);
    if(code=="redirect_uri_mismatch")return QString("YouTube: адрес возврата не совпал — redirect_uri_mismatch (HTTP %1)").arg(http);
    if(code=="invalid_request")return QString("YouTube: Google отклонил запрос — invalid_request (HTTP %1)").arg(http);
    if(!code.isEmpty())return QString("YouTube: вход отклонён — %1 (HTTP %2)").arg(code).arg(http);
    return QString("YouTube: вход не завершён (HTTP %1)").arg(http);
}
}
YouTubeChat::YouTubeChat(QString path,QObject* parent):QObject(parent),vault_(std::move(path)),login_(this){
    bool ok=false;tokens_=vault_.load(&ok);if(!ok)status_="YouTube: не удалось прочитать защищённые настройки";
    retry_.setSingleShot(true);expiry_.setSingleShot(true);search_.setInterval(10000);
    connect(&search_,&QTimer::timeout,this,[this]{if(streamActive_&&visible_&&!ready_&&chatId_.isEmpty())discover();});
    connect(&retry_,&QTimer::timeout,this,[this]{if(chatId_.isEmpty())discover();else openStream();});
    connect(&expiry_,&QTimer::timeout,this,&YouTubeChat::refresh);
    connect(&login_,&OAuthLoopback::failed,this,[this](const QString&){verifier_.clear();setStatus("YouTube: вход отменён или время истекло");});
    connect(&login_,&OAuthLoopback::received,this,[this](const QUrlQuery& query){
        const auto code=query.queryItemValue("code",QUrl::FullyDecoded);const auto redirect=login_.redirect().toString();
        const auto body=googleForm({{"client_id",googleClient},{"client_secret",googleSecret()},{"grant_type","authorization_code"},{"code",code},{"redirect_uri",redirect},{"code_verifier",verifier_}});
        verifier_.clear();setStatus("YouTube: завершаем вход…");
        request(QUrl("https://oauth2.googleapis.com/token"),"POST",body,false,[this](int http,const QJsonObject& data){
            if(http!=200){setStatus(oauthFailure(http,data));return;}
            if(!saveTokens(data)){setStatus("YouTube: Google не вернул токен доступа");return;}
            if(visible_&&streamActive_){search_.start();setStatus("YouTube: вход выполнен, ждём эфир");}
            else setStatus("YouTube: вход выполнен, ожидаем эфир");
        });
    });
}
YouTubeChat::~YouTubeChat(){stopNetwork();}
void YouTubeChat::setStatus(const QString& value){if(status_!=value){status_=value;Q_EMIT changed();}}
void YouTubeChat::request(const QUrl& url,const QByteArray& method,const QByteArray& body,bool auth,Reply callback){
    QNetworkRequest req(url);req.setTransferTimeout(15000);req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    if(auth)req.setRawHeader("Authorization",("Bearer "+tokens_.value("access_token").toString()).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader,url.host()=="oauth2.googleapis.com"?"application/x-www-form-urlencoded":"application/json");
    traffic::addSent(body.size());auto* reply=method=="GET"?http_.get(req):http_.post(req,body);replies_.insert(reply);const auto generation=generation_;
    connect(reply,&QNetworkReply::readyRead,reply,[reply]{if(reply->bytesAvailable()>1048576)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,callback=std::move(callback)]{
        CpuScope cpu;
        replies_.remove(reply);const int http=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto payload=reply->readAll();traffic::addReceived(payload.size());const auto data=QJsonDocument::fromJson(payload).object();reply->deleteLater();if(generation==generation_)callback(http,data);
    });
}
void YouTubeChat::stopStream(){if(stream_){auto reply=stream_;stream_=nullptr;reply->disconnect(this);reply->abort();reply->deleteLater();}frames_.clear();ready_=false;}
void YouTubeChat::stopNetwork(){++generation_;stopStream();login_.stop();retry_.stop();expiry_.stop();search_.stop();refreshing_=false;finding_=false;verifier_.clear();const auto pending=replies_.values();replies_.clear();for(auto* reply:pending)reply->abort();}
void YouTubeChat::setVisible(bool active){
    if(visible_==active)return;visible_=active;
    if(!active){stopNetwork();setStatus("YouTube: приостановлен");}
    else if(tokens_.value("access_token").toString().isEmpty())setStatus("YouTube: не подключён");
    else if(tokens_.value("expires_at").toDouble()<=double(QDateTime::currentSecsSinceEpoch()+60))refresh();
    else if(!chatId_.isEmpty())openStream();
    else if(streamActive_){search_.start();setStatus("YouTube: ждём поиск эфира");}
    else setStatus("YouTube: ожидаем начала эфира");Q_EMIT changed();
}
void YouTubeChat::setStreamActive(bool active){
    if(streamActive_==active)return;streamActive_=active;
    if(!active){search_.stop();if(chatId_.isEmpty()&&signedIn())setStatus("YouTube: ожидаем начала эфира");return;}
    if(visible_&&signedIn()&&chatId_.isEmpty()){search_.start();setStatus("YouTube: ждём поиск эфира");}
}
void YouTubeChat::signIn(){
    stopNetwork();chatId_.clear();pageToken_.clear();authRetries_=0;
    if(googleSecret().isEmpty()){setStatus("YouTube: нет настроек входа Google. Установите полный архив плагина.");return;}
    if(!login_.begin(0,"/google/callback",false)){setStatus("YouTube: не удалось открыть локальный адрес входа");return;}
    verifier_=randomVerifier();const auto challenge=QCryptographicHash::hash(verifier_.toUtf8(),QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals);
    QUrl url("https://accounts.google.com/o/oauth2/v2/auth");QUrlQuery q;
    q.addQueryItem("client_id",googleClient);q.addQueryItem("redirect_uri",login_.redirect().toString());q.addQueryItem("response_type","code");
    q.addQueryItem("scope","https://www.googleapis.com/auth/youtube.force-ssl");q.addQueryItem("state",login_.state());
    q.addQueryItem("code_challenge",QString::fromLatin1(challenge));q.addQueryItem("code_challenge_method","S256");q.addQueryItem("access_type","offline");q.addQueryItem("prompt","consent");url.setQuery(q);
    setStatus("YouTube: подтвердите разрешение в браузере");if(!QDesktopServices::openUrl(url)){login_.stop();setStatus("YouTube: не удалось открыть браузер");}
}
bool YouTubeChat::saveTokens(const QJsonObject& data){
    if(data.value("access_token").toString().isEmpty())return false;
    auto updated=data;if(updated.value("refresh_token").toString().isEmpty())updated["refresh_token"]=tokens_.value("refresh_token");
    const int seconds=std::clamp(data.value("expires_in").toInt(3600),60,86400);updated["expires_at"]=double(QDateTime::currentSecsSinceEpoch()+seconds);
    if(!vault_.save(updated)){setStatus("YouTube: не удалось защитить и сохранить токен");return false;}
    tokens_=updated;if(visible_)expiry_.start(std::max(1000,(seconds-60)*1000));return true;
}
void YouTubeChat::refresh(){
    if(refreshing_)return;
    if(tokens_.value("refresh_token").toString().isEmpty()){stopNetwork();setStatus("YouTube: требуется повторный вход");Q_EMIT changed();return;}
    refreshing_=true;stopStream();Q_EMIT changed();
    const auto secret=googleSecret();
    if(secret.isEmpty()){refreshing_=false;setStatus("YouTube: нет настроек входа Google. Установите полный архив плагина.");return;}
    request(QUrl("https://oauth2.googleapis.com/token"),"POST",googleForm({{"client_id",googleClient},{"client_secret",secret},{"grant_type","refresh_token"},{"refresh_token",tokens_.value("refresh_token").toString()}}),false,[this](int http,const QJsonObject& data){
        refreshing_=false;
        if(http==200&&saveTokens(data)){if(visible_){if(chatId_.isEmpty()){if(streamActive_)search_.start();}else openStream();}return;}
        if(http==400||http==401){stopNetwork();tokens_={};vault_.save({});setStatus("YouTube: разрешение истекло, войдите снова");}
        else {setStatus("YouTube: не удалось обновить вход, повторите подключение");}Q_EMIT changed();
    });
}
void YouTubeChat::signOut(){stopNetwork();tokens_={};chatId_.clear();pageToken_.clear();Q_EMIT broadcasts({});setStatus(vault_.save({})?"YouTube: отключён":"YouTube: не удалось удалить сохранённый токен");Q_EMIT changed();}
void YouTubeChat::discover(){
    if(!visible_||finding_||refreshing_||tokens_.value("access_token").toString().isEmpty())return;
    if(tokens_.value("expires_at").toDouble()<=double(QDateTime::currentSecsSinceEpoch()+60)){refresh();return;}
    expiry_.start(int(std::clamp(tokens_.value("expires_at").toDouble()-double(QDateTime::currentSecsSinceEpoch())-60.0,1.0,86400.0))*1000);
    stopStream();chatId_.clear();pageToken_.clear();finding_=true;setStatus("YouTube: ищем активный эфир…");
    QUrl url("https://www.googleapis.com/youtube/v3/liveBroadcasts");QUrlQuery q;q.addQueryItem("part","snippet");q.addQueryItem("broadcastStatus","active");q.addQueryItem("broadcastType","all");q.addQueryItem("maxResults","50");url.setQuery(q);
    request(url,"GET",{},true,[this](int http,const QJsonObject& data){
        finding_=false;if(http==401){refresh();return;}
        if(http!=200){if(http==403||http==429)search_.stop();setStatus(QString("YouTube: эфир недоступен (HTTP %1). Проверьте доступ и квоту API.").arg(http));return;}
        QJsonArray choices;for(const auto& item:data.value("items").toArray()){
            const auto snippet=item.toObject().value("snippet").toObject();const auto id=snippet.value("liveChatId").toString();
            if(!id.isEmpty())choices.append(QJsonObject{{"id",id},{"title",snippet.value("title").toString().left(200)}});
        }
        Q_EMIT broadcasts(choices);
        if(choices.size()==1)selectChat(choices.first().toObject().value("id").toString());
        else if(choices.isEmpty())setStatus("YouTube: активный эфир с чатом не найден. Повторим поиск через 10 с.");
        else {search_.stop();setStatus("YouTube: выберите эфир для подключения");}
    });
}
void YouTubeChat::selectChat(const QString& id){if(id.isEmpty()||id.size()>512||!visible_)return;search_.stop();stopStream();chatId_=id;pageToken_.clear();attempts_=0;authRetries_=0;seen_.clear();seenOrder_.clear();openStream();}
void YouTubeChat::openStream(){
    if(!visible_||chatId_.isEmpty()||refreshing_)return;
    stopStream();retry_.stop();setStatus("YouTube: подключаем поток чата…");
    QNetworkRequest req(QUrl("https://youtube.googleapis.com/youtube.api.v3.V3DataLiveChatMessageService/StreamList"));
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute,true);req.setAttribute(QNetworkRequest::Http2DirectAttribute,true);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("Authorization",("Bearer "+tokens_.value("access_token").toString()).toUtf8());req.setRawHeader("Content-Type","application/grpc");req.setRawHeader("TE","trailers");req.setRawHeader("grpc-accept-encoding","identity");
    const auto body=youtube_wire::request(chatId_,pageToken_);traffic::addSent(body.size());auto* reply=http_.post(req,body);stream_=reply;reply->setReadBufferSize(65536);
    connect(reply,&QNetworkReply::readyRead,this,&YouTubeChat::receive);
    connect(reply,&QNetworkReply::finished,this,[this,reply]{
        if(stream_!=reply){reply->deleteLater();return;}
        receive();if(stream_!=reply)return;
        const int http=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray grpc=reply->rawHeader("grpc-status");stream_=nullptr;reply->deleteLater();ready_=false;Q_EMIT changed();
        if(http==401||grpc=="16"){if(authRetries_++<1){refresh();return;}setStatus("YouTube: требуется повторный вход");return;}
        if(http==403||grpc=="7"||grpc=="8"||http==429){setStatus("YouTube: доступ или квота API ограничены; автоматические повторы остановлены");return;}
        retry();
    });
    const QPointer<QNetworkReply> expected=reply;
    QTimer::singleShot(20000,this,[this,expected]{if(expected&&stream_==expected&&!ready_){stopStream();retry();}});
}
void YouTubeChat::receive(){
    CpuScope cpu;
    if(!stream_)return;
    if(stream_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200){traffic::addReceived(stream_->readAll().size());return;}
    while(stream_&&stream_->bytesAvailable()){
        QVector<QByteArray> complete;
        const auto bytes=stream_->read(65536);traffic::addReceived(bytes.size());
        if(!frames_.append(bytes,complete)){stopStream();setStatus("YouTube: неподдерживаемый или слишком большой ответ потока");Q_EMIT changed();return;}
        for(const auto& payload:complete){
            youtube_wire::Batch batch;if(!youtube_wire::decode(payload,batch)){stopStream();setStatus("YouTube: некорректный ответ потока");Q_EMIT changed();return;}
            if(!batch.nextPage.isEmpty())pageToken_=batch.nextPage;
            if(!ready_){ready_=true;attempts_=0;authRetries_=0;setStatus("YouTube: чат подключён");Q_EMIT changed();}
            for(const auto& id:batch.removedIds)Q_EMIT removeMessage(id);
            for(const auto& message:batch.messages){if(seen_.contains(message.id))continue;seen_.insert(message.id);seenOrder_.enqueue(message.id);while(seenOrder_.size()>1000)seen_.remove(seenOrder_.dequeue());Q_EMIT this->message(message);}
            if(batch.ended){stopStream();chatId_.clear();setStatus("YouTube: эфир завершён");Q_EMIT changed();return;}
        }
    }
}
void YouTubeChat::retry(){
    if(!visible_||retry_.isActive())return;
    if(++attempts_>8){setStatus("YouTube: соединение не восстановлено. Повторим при следующем эфире.");return;}
    setStatus("YouTube: восстанавливаем соединение…");retry_.start(std::min(120000,1000*(1<<std::min(attempts_,6))));
}
void YouTubeChat::send(const QString& text){
    if(!ready_||text.trimmed().isEmpty()||text.size()>200){Q_EMIT sent(false,"YouTube: чат не готов или текст длиннее 200 символов");return;}
    const QJsonObject body{{"snippet",QJsonObject{{"liveChatId",chatId_},{"type","textMessageEvent"},{"textMessageDetails",QJsonObject{{"messageText",text}}}}}};
    request(QUrl("https://www.googleapis.com/youtube/v3/liveChat/messages?part=snippet"),"POST",QJsonDocument(body).toJson(QJsonDocument::Compact),true,[this](int http,const QJsonObject& data){
        const bool ok=http==200&&!data.value("id").toString().isEmpty();Q_EMIT sent(ok,ok?"YouTube: сообщение отправлено":QString("YouTube: отправка не подтверждена (HTTP %1), автоматического повтора нет").arg(http));if(http==401)refresh();
    });
}
}
