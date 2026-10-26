#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>
#include "twitch_chat.hpp"
#include "youtube_chat.hpp"
#include <QComboBox>
#include <QSignalBlocker>
#include <QStringList>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextImageFormat>
#include <QTextCharFormat>
#include <QFont>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHideEvent>
#include <QShowEvent>
#include <QCoreApplication>
#include <QSslSocket>
#include <QPointer>
#include <QPainter>
#include <QPainterPath>
#include <QFrame>
#include <algorithm>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("stream-chat-obs", "en-US")
namespace {
constexpr const char* DockId="stream-chat-obs.chat";
const char* ChatStyle=R"(
QWidget#StreamChatRoot { background:#080f18; color:#dfe8ff; font-size:12px; }
QWidget#Card { background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #101925,stop:1 #0b131e); border:1px solid #263244; border-radius:8px; }
QLabel { color:#dfe8ff; background:transparent; }
QLabel#Title { color:#e7fff4; font-size:17px; font-weight:700; }
QLabel#Section { color:#edf3ff; font-size:13px; font-weight:700; }
QLabel#Hint { color:#8ea1c0; font-size:11px; }
QLabel#Callout { color:#ffdd6b; background:#211e16; border:1px solid #51452b; border-radius:6px; padding:5px; }
QPushButton { color:#e1ebff; background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #202a3b,stop:1 #141d2b); border:1px solid #39465d; border-radius:7px; min-height:22px; padding:3px 8px; }
QPushButton:hover { background:#26344d; border-color:#6476a0; }
QPushButton:focus { border-color:#8a79ff; }
QPushButton:pressed { background:#111b30; }
QPushButton:disabled { color:#6c7890; border-color:#273246; }
QPushButton#Primary { color:#061c20; font-weight:700; background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #51efaa,stop:1 #34cce9); border-color:#7fffd0; }
QPushButton#Primary:hover { background:#8af9d0; border-color:#c7ffea; }
QPushButton#Primary:disabled { color:#71879b; background:#1a2b39; border-color:#314251; }
QPushButton#Disconnect { color:#8ea1c0; background:#111b28; border:1px solid #304057; border-radius:7px; min-width:24px; max-width:24px; min-height:24px; max-height:24px; padding:0; font-size:16px; font-weight:700; }
QPushButton#Disconnect:hover { color:#ff8d96; background:#2a1820; border-color:#8d4350; }
QLineEdit,QTextEdit,QComboBox { color:#e7eeff; background:#0c1420; border:1px solid #344865; border-radius:6px; padding:5px; selection-background-color:#316ac4; }
QLineEdit:focus,QTextEdit:focus,QComboBox:focus { border-color:#776af5; background:#111c2e; }
QComboBox QAbstractItemView { background:#101a2b; color:#e7eeff; selection-background-color:#26344d; }
QCheckBox { color:#dce7fb; spacing:6px; min-height:20px; }
QCheckBox::indicator { width:15px; height:15px; }
QCheckBox::indicator:unchecked { border-radius:4px; background:#344357; border:1px solid #65748d; }
QCheckBox::indicator:checked { border-radius:4px; background:#00d37d; border:1px solid #2ff9a0; }
QScrollBar:vertical { border:0; background:#101929; width:9px; }
QScrollBar::handle:vertical { background:#384d6c; border-radius:4px; min-height:24px; }
QScrollBar::handle:vertical:hover { background:#52698d; }
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; background:transparent; border:0; }
)";

class PlatformBadge final : public QWidget {
public:
    explicit PlatformBadge(QString platform,QWidget* parent=nullptr):QWidget(parent),platform_(std::move(platform)){setFixedSize(28,28);setAccessibleName(platform_);}
protected:
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.scale(width()/32.0,height()/32.0);
        const QColor color=platform_=="YouTube"?QColor("#ff1538"):platform_=="Twitch"?QColor("#9146ff"):QColor("#06382c");
        p.setPen(Qt::NoPen);p.setBrush(color);p.drawRoundedRect(QRectF(0,0,32,32),9,9);p.setBrush(Qt::white);
        if(platform_=="YouTube"){
            p.drawRoundedRect(QRectF(5,8,22,16),5,5);p.setBrush(color);QPainterPath play;play.moveTo(13,11);play.lineTo(22,16);play.lineTo(13,21);play.closeSubpath();p.drawPath(play);
        }else if(platform_=="Twitch"){
            QPainterPath bubble;bubble.moveTo(7,6);bubble.lineTo(26,6);bubble.lineTo(26,21);bubble.lineTo(20,27);bubble.lineTo(14,27);bubble.lineTo(10,30);bubble.lineTo(10,25);bubble.lineTo(7,25);bubble.closeSubpath();p.drawPath(bubble);p.setPen(QPen(color,2));p.drawLine(15,11,15,19);p.drawLine(21,11,21,19);
        }else{
            const QColor green("#00df8c");p.setBrush(green);p.drawEllipse(QPointF(16,16),3,3);p.setBrush(Qt::NoBrush);p.setPen(QPen(green,2.5,Qt::SolidLine,Qt::RoundCap));p.drawArc(QRectF(7,7,18,18),-50*16,100*16);p.drawArc(QRectF(7,7,18,18),130*16,100*16);p.drawArc(QRectF(3,3,26,26),-45*16,90*16);p.drawArc(QRectF(3,3,26,26),135*16,90*16);
        }
    }
private: QString platform_;
};

class PlatformToggle final : public QCheckBox {
public:
    explicit PlatformToggle(QString platform,QWidget* parent=nullptr):QCheckBox(parent),platform_(std::move(platform)){
        setFixedSize(46,26);setCursor(Qt::PointingHandCursor);setAccessibleName(platform_);setToolTip("Отправлять в "+platform_);
    }
protected:
    bool hitButton(const QPoint& point)const override{return rect().contains(point);}
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setOpacity(isEnabled()?1.0:0.45);
        p.setPen(QPen(isChecked()?QColor("#1f8062"):QColor("#34465d"),1));p.setBrush(isChecked()?QColor("#102c29"):QColor("#121b27"));p.drawRoundedRect(rect().adjusted(1,1,-1,-1),7,7);
        const QColor brand=platform_=="YouTube"?QColor("#ff1538"):QColor("#9146ff");p.setPen(Qt::NoPen);p.setBrush(brand);p.drawRoundedRect(QRectF(5,4,22,18),5,5);p.setBrush(Qt::white);
        if(platform_=="YouTube"){
            QPainterPath play;play.moveTo(13,8);play.lineTo(21,13);play.lineTo(13,18);play.closeSubpath();p.drawPath(play);
        }else{
            QPainterPath bubble;bubble.moveTo(10,7);bubble.lineTo(23,7);bubble.lineTo(23,17);bubble.lineTo(19,21);bubble.lineTo(15,21);bubble.lineTo(12,23);bubble.lineTo(12,20);bubble.lineTo(10,20);bubble.closeSubpath();p.drawPath(bubble);p.setPen(QPen(brand,1.5));p.drawLine(16,10,16,16);p.drawLine(20,10,20,16);
        }
        p.setPen(Qt::NoPen);p.setBrush(isChecked()?QColor("#00df8c"):QColor("#65748d"));p.drawEllipse(QPointF(36,13),4,4);
        if(hasFocus()){p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor("#8a79ff"),1,Qt::DotLine));p.drawRoundedRect(rect().adjusted(0,0,-1,-1),7,7);}
    }
private: QString platform_;
};

QPixmap messagePlatformIcon(const QString& platform){
    QPixmap image(36,36);image.fill(Qt::transparent);QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.scale(1.5,1.5);
    const QColor brand=platform=="YouTube"?QColor("#ff1538"):QColor("#9146ff");p.setPen(Qt::NoPen);p.setBrush(brand);p.drawRoundedRect(QRectF(1,1,22,22),6,6);p.setBrush(Qt::white);
    if(platform=="YouTube"){
        p.drawRoundedRect(QRectF(4,6,16,12),4,4);p.setBrush(brand);QPainterPath play;play.moveTo(10,8);play.lineTo(16,12);play.lineTo(10,16);play.closeSubpath();p.drawPath(play);
    }else{
        QPainterPath bubble;bubble.moveTo(5,4);bubble.lineTo(20,4);bubble.lineTo(20,16);bubble.lineTo(16,20);bubble.lineTo(11,20);bubble.lineTo(8,23);bubble.lineTo(8,19);bubble.lineTo(5,19);bubble.closeSubpath();p.drawPath(bubble);p.setPen(QPen(brand,1.6));p.drawLine(11,8,11,14);p.drawLine(16,8,16,14);
    }
    return image;
}

QWidget* card(){auto* result=new QWidget;result->setObjectName("Card");return result;}
QLabel* sectionTitle(const QString& text){auto* result=new QLabel(text);result->setObjectName("Section");return result;}
QString compactStatus(QString value,const QString& platform){value.remove(platform+": ");value.replace("чат подключён","Подключён",Qt::CaseInsensitive);value.replace("не подключён","Не подключён",Qt::CaseInsensitive);value.replace("вход выполнен, чат приостановлен","Вход выполнен",Qt::CaseInsensitive);value.replace("подтвердите разрешение в браузере","Подтвердите вход в браузере",Qt::CaseInsensitive);return value;}
void paintStatus(QLabel* label,const QString& raw,const QString& platform,bool ready){
    const QString value=compactStatus(raw,platform);QString color="#8ea1c0";
    if(ready)color="#00df8c";
    else if(value.contains("HTTP",Qt::CaseInsensitive)||value.contains("ошиб",Qt::CaseInsensitive)||value.contains("не удалось",Qt::CaseInsensitive)||value.contains("отклон",Qt::CaseInsensitive)||value.contains("недоступ",Qt::CaseInsensitive))color="#ff7f88";
    else if(value.contains("подтверд",Qt::CaseInsensitive)||value.contains("заверша",Qt::CaseInsensitive)||value.contains("ищем",Qt::CaseInsensitive)||value.contains("подключ",Qt::CaseInsensitive)||value.contains("восстанавли",Qt::CaseInsensitive))color="#ffcd57";
    label->setText("● "+value);label->setStyleSheet(QString("color:%1;font-size:11px;").arg(color));
}
QString configPath(const char* name){char* raw=obs_module_config_path(name);if(!raw)return {};const auto path=QString::fromUtf8(raw);bfree(raw);return path;}
class ChatDock final : public QWidget {
public:
    ChatDock():twitch_(configPath("twitch-account.dat"),this),youtube_(configPath("youtube-account.dat"),this){
        setObjectName("StreamChatRoot");setMinimumWidth(320);setStyleSheet(ChatStyle);
        auto* layout=new QVBoxLayout(this);layout->setContentsMargins(8,8,8,8);layout->setSpacing(7);
        auto* header=new QHBoxLayout;header->setSpacing(7);header->addWidget(new PlatformBadge("Stream Chat"));auto* title=new QLabel("Stream Chat");title->setObjectName("Title");header->addWidget(title,1);layout->addLayout(header);

        auto* twitchCard=card();auto* twitchLayout=new QVBoxLayout(twitchCard);twitchLayout->setContentsMargins(8,7,8,8);twitchLayout->setSpacing(5);
        auto* twitchHead=new QHBoxLayout;twitchHead->setSpacing(7);twitchHead->addWidget(new PlatformBadge("Twitch"));twitchHead->addWidget(sectionTitle("Twitch"),1);logout_=new QPushButton("×");logout_->setObjectName("Disconnect");logout_->setToolTip("Отключить Twitch");twitchHead->addWidget(logout_);twitchLayout->addLayout(twitchHead);twitchStatus_=new QLabel;twitchStatus_->setWordWrap(true);twitchLayout->addWidget(twitchStatus_);
        auto* connections=new QHBoxLayout;login_=new QPushButton("Войти");connections->addWidget(login_);twitchLayout->addLayout(connections);
        connect(login_,&QPushButton::clicked,&twitch_,&sc::TwitchChat::signIn);connect(logout_,&QPushButton::clicked,&twitch_,&sc::TwitchChat::signOut);
        code_=new QLabel;code_->setObjectName("Callout");code_->setTextFormat(Qt::PlainText);code_->setWordWrap(true);code_->setTextInteractionFlags(Qt::TextSelectableByMouse);code_->hide();twitchLayout->addWidget(code_);layout->addWidget(twitchCard);

        auto* youtubeCard=card();auto* youtubeLayout=new QVBoxLayout(youtubeCard);youtubeLayout->setContentsMargins(8,7,8,8);youtubeLayout->setSpacing(5);
        auto* youtubeHead=new QHBoxLayout;youtubeHead->setSpacing(7);youtubeHead->addWidget(new PlatformBadge("YouTube"));youtubeHead->addWidget(sectionTitle("YouTube"),1);googleLogout_=new QPushButton("×");googleLogout_->setObjectName("Disconnect");googleLogout_->setToolTip("Отключить YouTube");youtubeHead->addWidget(googleLogout_);youtubeLayout->addLayout(youtubeHead);youtubeStatus_=new QLabel;youtubeStatus_->setWordWrap(true);youtubeLayout->addWidget(youtubeStatus_);
        auto* googleButtons=new QHBoxLayout;googleLogin_=new QPushButton("Войти");googleButtons->addWidget(googleLogin_);youtubeLayout->addLayout(googleButtons);
        connect(googleLogin_,&QPushButton::clicked,&youtube_,&sc::YouTubeChat::signIn);connect(googleLogout_,&QPushButton::clicked,&youtube_,&sc::YouTubeChat::signOut);
        auto* broadcasts=new QHBoxLayout;broadcast_=new QComboBox;broadcast_->addItem("Эфир не выбран",QString());find_=new QPushButton("Найти эфир");broadcasts->addWidget(broadcast_,1);broadcasts->addWidget(find_);youtubeLayout->addLayout(broadcasts);layout->addWidget(youtubeCard);
        connect(find_,&QPushButton::clicked,&youtube_,&sc::YouTubeChat::discover);
        connect(broadcast_,&QComboBox::currentIndexChanged,this,[this](int index){const auto id=broadcast_->itemData(index).toString();if(!id.isEmpty())youtube_.selectChat(id);});
        connect(&youtube_,&sc::YouTubeChat::broadcasts,this,[this](const QJsonArray& choices){
            const QSignalBlocker block(broadcast_);broadcast_->clear();broadcast_->addItem("Выберите эфир",QString());
            for(const auto& value:choices){const auto choice=value.toObject();broadcast_->addItem(choice.value("title").toString(),choice.value("id").toString());}
            if(choices.size()==1)broadcast_->setCurrentIndex(1);
        });
        auto* chatCard=card();auto* chatLayout=new QVBoxLayout(chatCard);chatLayout->setContentsMargins(8,7,8,8);chatLayout->setSpacing(5);chatLayout->addWidget(sectionTitle("Чат"));
        messages_=new QTextEdit;messages_->setReadOnly(true);messages_->setAcceptRichText(false);messages_->document()->setMaximumBlockCount(500);messages_->setPlaceholderText("Здесь появятся сообщения. История хранится только до закрытия OBS.");
        messages_->document()->addResource(QTextDocument::ImageResource,QUrl("stream-chat://icon/twitch"),messagePlatformIcon("Twitch"));messages_->document()->addResource(QTextDocument::ImageResource,QUrl("stream-chat://icon/youtube"),messagePlatformIcon("YouTube"));chatLayout->addWidget(messages_,1);
        auto* targets=new QHBoxLayout;auto* recipientHint=new QLabel("Куда отправлять");recipientHint->setObjectName("Hint");targets->addWidget(recipientHint);targets->addStretch();target_=new PlatformToggle("Twitch");target_->setChecked(true);targets->addWidget(target_);youtubeTarget_=new PlatformToggle("YouTube");targets->addWidget(youtubeTarget_);chatLayout->addLayout(targets);layout->addWidget(chatCard,1);
        auto* composerCard=card();auto* composerLayout=new QVBoxLayout(composerCard);composerLayout->setContentsMargins(8,7,8,8);composerLayout->setSpacing(5);auto* composer=new QHBoxLayout;input_=new QLineEdit;input_->setMaxLength(500);input_->setPlaceholderText("Сообщение в выбранные каналы");send_=new QPushButton("Отправить");send_->setObjectName("Primary");composer->addWidget(input_,1);composer->addWidget(send_);composerLayout->addLayout(composer);delivery_=new QLabel;delivery_->setObjectName("Hint");delivery_->setWordWrap(true);delivery_->setTextFormat(Qt::PlainText);delivery_->hide();composerLayout->addWidget(delivery_);layout->addWidget(composerCard);
        auto* version=new QLabel("developed by OTMOROZOK · v" STREAM_CHAT_VERSION);version->setObjectName("Hint");version->setAlignment(Qt::AlignCenter);layout->addWidget(version);
        batch_.setInterval(20);batch_.setSingleShot(true);
        connect(&batch_,&QTimer::timeout,this,[this]{flush();});
        connect(send_,&QPushButton::clicked,this,[this]{send();});connect(input_,&QLineEdit::returnPressed,this,[this]{send();});
        connect(target_,&QCheckBox::toggled,this,[this]{refresh();});
        connect(youtubeTarget_,&QCheckBox::toggled,this,[this]{refresh();});
        connect(input_,&QLineEdit::textChanged,this,[this]{refresh();});
        connect(&twitch_,&sc::TwitchChat::changed,this,[this]{if(twitch_.ready())code_->hide();refresh();});
        connect(&youtube_,&sc::YouTubeChat::changed,this,[this]{refresh();});
        connect(&twitch_,&sc::TwitchChat::signInCode,this,[this](const QString& code,const QUrl& url){code_->setText("Код Twitch: "+code.left(64)+"\n"+url.toString());code_->show();});
        connect(&twitch_,&sc::TwitchChat::message,this,[this](const sc::ChatMessage& message){pending_.enqueue(message);while(pending_.size()>500)pending_.dequeue();if(!batch_.isActive())batch_.start();});
        connect(&youtube_,&sc::YouTubeChat::message,this,[this](const sc::ChatMessage& message){pending_.enqueue(message);while(pending_.size()>500)pending_.dequeue();if(!batch_.isActive())batch_.start();});
        connect(&twitch_,&sc::TwitchChat::removeMessage,this,[this](const QString& id){remove([&](const sc::ChatMessage& m){return m.platform=="Twitch"&&m.id==id;});});
        connect(&youtube_,&sc::YouTubeChat::removeMessage,this,[this](const QString& id){remove([&](const sc::ChatMessage& m){return m.platform=="YouTube"&&m.id==id;});});
        connect(&twitch_,&sc::TwitchChat::clearAuthor,this,[this](const QString& id){remove([&](const sc::ChatMessage& m){return m.platform=="Twitch"&&m.authorId==id;});});
        connect(&twitch_,&sc::TwitchChat::clearChat,this,[this]{remove([](const sc::ChatMessage& m){return m.platform=="Twitch";});});
        connect(&twitch_,&sc::TwitchChat::sent,this,[this](bool confirmed,const QString& reason){completed("Twitch",confirmed,reason);});
        connect(&youtube_,&sc::YouTubeChat::sent,this,[this](bool confirmed,const QString& reason){completed("YouTube",confirmed,reason);});
        refresh();
    }
protected:
    void showEvent(QShowEvent* event)override{QWidget::showEvent(event);twitch_.setVisible(true);youtube_.setVisible(true);if(!pending_.isEmpty())batch_.start();}
    void hideEvent(QHideEvent* event)override{twitch_.setVisible(false);youtube_.setVisible(false);batch_.stop();if(activeSends_)delivery_->setText("Панель скрыта: результат отправки не подтверждён. Автоматического повтора нет.");activeSends_=0;refresh();QWidget::hideEvent(event);}
private:
    void appendMessage(const sc::ChatMessage& message){
        auto text=message.text,author=message.author;text.replace('\n',' ');text.replace('\r',' ');author.replace('\n',' ');author.replace('\r',' ');
        QTextCursor cursor(messages_->document());cursor.movePosition(QTextCursor::End);if(!messages_->document()->isEmpty())cursor.insertBlock();
        QTextImageFormat icon;icon.setName(message.platform=="YouTube"?"stream-chat://icon/youtube":"stream-chat://icon/twitch");icon.setWidth(18);icon.setHeight(18);cursor.insertImage(icon);cursor.insertText(" ");
        const bool youtube=message.platform=="YouTube";const QColor accent=youtube?QColor("#ff8491"):QColor("#b995ff");const QColor tint=youtube?QColor("#32181d"):QColor("#241b35");
        QTextCharFormat authorStyle;authorStyle.setForeground(accent);authorStyle.setBackground(tint);authorStyle.setFontWeight(QFont::DemiBold);cursor.insertText(" "+author+" ",authorStyle);
        QTextCharFormat messageStyle;messageStyle.setForeground(QColor("#dfe8ff"));cursor.insertText("  "+text,messageStyle);messages_->ensureCursorVisible();
    }
    void refresh(){
        paintStatus(twitchStatus_,twitch_.status(),"Twitch",twitch_.ready());paintStatus(youtubeStatus_,youtube_.status(),"YouTube",youtube_.ready());
        const bool twitchBusy=code_->isVisible()&&twitch_.status().contains("подтвердите",Qt::CaseInsensitive);if(code_->isVisible()&&!twitchBusy&&!twitch_.ready())code_->hide();
        login_->setVisible(!twitch_.ready());logout_->setVisible(twitch_.ready());login_->setEnabled(!twitchBusy);
        googleLogin_->setVisible(!youtube_.signedIn());googleLogout_->setVisible(youtube_.signedIn());
        const bool youtubeBusy=youtube_.status().contains("подтвердите",Qt::CaseInsensitive)||youtube_.status().contains("завершаем",Qt::CaseInsensitive);googleLogin_->setEnabled(!youtubeBusy);broadcast_->setEnabled(youtube_.signedIn());find_->setEnabled(youtube_.signedIn());
        delivery_->setVisible(!delivery_->text().isEmpty());
        const bool selected=target_->isChecked()||youtubeTarget_->isChecked();
        const bool destinations=(!target_->isChecked()||twitch_.ready())&&(!youtubeTarget_->isChecked()||youtube_.ready());
        input_->setEnabled((twitch_.ready()||youtube_.ready())&&!activeSends_);
        send_->setEnabled(selected&&destinations&&!activeSends_&&!input_->text().trimmed().isEmpty());
        target_->setEnabled(!activeSends_);youtubeTarget_->setEnabled(!activeSends_);
    }
    void send(){
        if(activeSends_||input_->text().trimmed().isEmpty())return;
        const bool tw=target_->isChecked(),yt=youtubeTarget_->isChecked();
        if((!tw&&!yt)||(tw&&!twitch_.ready())||(yt&&!youtube_.ready()))return;
        if(yt&&input_->text().size()>200){delivery_->setText("Для YouTube сократите сообщение до 200 символов.");return;}
        activeSends_=int(tw)+int(yt);allSent_=true;twitchSent_=false;youtubeSent_=false;results_.clear();
        delivery_->setText("Отправляем в выбранные каналы…");refresh();const auto text=input_->text();
        if(tw)twitch_.send(text);if(yt)youtube_.send(text);
    }
    void completed(const QString& platform,bool confirmed,const QString& reason){
        if(!activeSends_)return;allSent_=allSent_&&confirmed;results_.append(reason);
        if(platform=="Twitch")twitchSent_=confirmed;else youtubeSent_=confirmed;
        --activeSends_;if(!activeSends_){if(allSent_)input_->clear();else {if(twitchSent_)target_->setChecked(false);if(youtubeSent_)youtubeTarget_->setChecked(false);}}
        delivery_->setText(results_.join('\n'));refresh();
    }
    void flush(){int count=0;while(!pending_.isEmpty()&&count++<50){const auto message=pending_.dequeue();history_.enqueue(message);while(history_.size()>500)history_.dequeue();appendMessage(message);}if(!pending_.isEmpty())batch_.start();}
    template<class Predicate>void remove(Predicate predicate){
        history_.erase(std::remove_if(history_.begin(),history_.end(),predicate),history_.end());
        pending_.erase(std::remove_if(pending_.begin(),pending_.end(),predicate),pending_.end());
        messages_->clear();for(const auto& message:history_)appendMessage(message);
    }
    sc::TwitchChat twitch_;sc::YouTubeChat youtube_;QTextEdit* messages_=nullptr;QLabel* twitchStatus_=nullptr;QLabel* youtubeStatus_=nullptr;QLabel* code_=nullptr;QLabel* delivery_=nullptr;
    QLineEdit* input_=nullptr;QCheckBox* target_=nullptr;QPushButton* send_=nullptr;QPushButton* login_=nullptr;QPushButton* logout_=nullptr;
    QCheckBox* youtubeTarget_=nullptr;QComboBox* broadcast_=nullptr;QPushButton* googleLogin_=nullptr;QPushButton* googleLogout_=nullptr;QPushButton* find_=nullptr;
    QQueue<sc::ChatMessage> history_,pending_;QTimer batch_;int activeSends_=0;bool allSent_=false,twitchSent_=false,youtubeSent_=false;QStringList results_;
};
QPointer<ChatDock> dock;QString tlsPath;
void createDock(){if(dock)return;auto* widget=new ChatDock;if(obs_frontend_add_dock_by_id(DockId,"Stream Chat",widget))dock=widget;else delete widget;}
void onFrontendEvent(enum obs_frontend_event event,void*){if(event==OBS_FRONTEND_EVENT_FINISHED_LOADING)createDock();}
}
bool obs_module_load(void){
    char* path=obs_module_file("qt-plugins");if(path){tlsPath=QString::fromUtf8(path);bfree(path);QCoreApplication::addLibraryPath(tlsPath);}
    if(!QSslSocket::supportsSsl()&&QSslSocket::availableBackends().contains("schannel"))QSslSocket::setActiveBackend("schannel");
    obs_frontend_add_event_callback(onFrontendEvent,nullptr);blog(LOG_INFO,"Stream Chat OBS %s loaded; HTTPS: %s",STREAM_CHAT_VERSION,QSslSocket::supportsSsl()?"available":"unavailable");return true;
}
void obs_module_unload(void){obs_frontend_remove_event_callback(onFrontendEvent,nullptr);if(dock)obs_frontend_remove_dock(DockId);dock=nullptr;if(!tlsPath.isEmpty())QCoreApplication::removeLibraryPath(tlsPath);tlsPath.clear();}

