#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>
#include "twitch_chat.hpp"
#include "youtube_chat.hpp"
#include "kick_chat.hpp"
#include "traffic.hpp"
#include "usage.hpp"
#include <QComboBox>
#include <QSignalBlocker>
#include <QStringList>
#include <QAbstractButton>
#include <QSizePolicy>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextOption>
#include <QTextCursor>
#include <QTextImageFormat>
#include <QTextCharFormat>
#include <QFont>
#include <QPushButton>
#include <QDialog>
#include <QSettings>
#include <QStyle>
#include <QScrollBar>
#include <QDockWidget>
#include <QMainWindow>
#include <QAction>
#include <QDesktopServices>
#include <QIcon>
#include <QSlider>
#include <QStackedLayout>
#include <memory>
#include <QRegularExpression>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QVBoxLayout>
#include <QHideEvent>
#include <QShowEvent>
#include <QCoreApplication>
#include <QMetaObject>
#include <QLocale>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHash>
#include <QSet>
#include <QPixmap>
#include <QSslSocket>
#include <QPointer>
#include <QPainter>
#include <QPainterPath>
#include <QFrame>
#include <algorithm>
#include <thread>
#include <QCheckBox>
#include <QScrollArea>
#include <QTextBlockFormat>
#include <QClipboard>
#include <QGuiApplication>

#ifndef STREAM_CHAT_PREVIEW
extern "C" __declspec(dllexport) bool stream_chat_telemetry_v1(std::uint64_t* sent,std::uint64_t* received){
    if(!sent||!received)return false;
    *sent=sc::traffic::sent.load(std::memory_order_relaxed);
    *received=sc::traffic::received.load(std::memory_order_relaxed);
    return true;
}
extern "C" __declspec(dllexport) bool stream_chat_telemetry_v2(std::uint64_t* sent,std::uint64_t* received,std::uint64_t* cpuNs,std::uint64_t* dataBytes){
    if(!sent||!received||!cpuNs||!dataBytes)return false;
    *sent=sc::traffic::sent.load(std::memory_order_relaxed);
    *received=sc::traffic::received.load(std::memory_order_relaxed);
    *cpuNs=sc::measuredCpuNs.load(std::memory_order_relaxed);
    *dataBytes=sc::measuredDataBytes.load(std::memory_order_relaxed);
    return true;
}
#endif

#ifndef STREAM_CHAT_PREVIEW
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("stream-chat-obs", "en-US")
#endif
namespace {
constexpr const char* DockId="stream-chat-obs.chat";
bool obsLanguageIsEnglish(){
#ifdef STREAM_CHAT_PREVIEW
    return !QLocale::system().name().startsWith("ru",Qt::CaseInsensitive);
#else
    const char* locale=obs_get_locale();
    return !locale||!QString::fromUtf8(locale).startsWith("ru",Qt::CaseInsensitive);
#endif
}
const char* ChatStyle=R"(
QWidget#StreamChatRoot { background:#080f18; color:#dfe8ff; font-size:12px; }
QWidget#Card,QWidget#KickCard { background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #101925,stop:1 #0b131e); border:1px solid #263244; border-radius:8px; }
QWidget#Card:hover,QWidget#KickCard:hover { border-color:#38516d; }
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
QPushButton#Settings { color:#e1ebff; background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #202a3b,stop:1 #141d2b); border:1px solid #39465d; border-radius:8px; padding:0; }
QPushButton#Settings:hover { background:#26344d; border-color:#6476a0; }
QPushButton#ScrollBottom { color:#b7efff; background:#17283a; border:1px solid #34627b; border-radius:7px; min-height:18px; max-height:18px; padding:0 6px; font-size:10px; }
QPushButton#ScrollBottom:hover { color:#ffffff; background:#24405a; }
QLabel#Credit { color:#9299a5; background:#080f18; min-height:20px; max-height:20px; font-size:11px; }
QLineEdit,QTextEdit,QComboBox { color:#e7eeff; background:#0c1420; border:1px solid #344865; border-radius:6px; padding:5px; selection-background-color:#316ac4; }
QLineEdit:focus,QTextEdit:focus,QComboBox:focus { border-color:#776af5; background:#111c2e; }
QComboBox QAbstractItemView { background:#101a2b; color:#e7eeff; selection-background-color:#26344d; }
QScrollBar:vertical { border:0; background:#101929; width:9px; }
QScrollBar::handle:vertical { background:#384d6c; border-radius:4px; min-height:24px; }
QScrollBar::handle:vertical:hover { background:#52698d; }
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; background:transparent; border:0; }
)";
QIcon settingsIcon(){
    QPixmap image(48,48);image.fill(Qt::transparent);
    QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);painter.scale(2,2);
    painter.setPen(QPen(QColor("#aebde3"),1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter.setBrush(Qt::NoBrush);
    painter.translate(12,12);
    for(int i=0;i<8;++i){painter.drawLine(QPointF(0,-8),QPointF(0,-10));painter.rotate(45);}
    painter.drawEllipse(QPointF(0,0),6.8,6.8);painter.drawEllipse(QPointF(0,0),2.5,2.5);
    painter.end();image.setDevicePixelRatio(2);return QIcon(image);
}

class PlatformBadge final : public QWidget {
public:
    explicit PlatformBadge(QString platform,QWidget* parent=nullptr):QWidget(parent),platform_(std::move(platform)){setFixedSize(28,28);setAccessibleName(platform_);setToolTip(platform_);}
protected:
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.scale(width()/32.0,height()/32.0);
        const QColor color=platform_=="Kick"?QColor("#53fc18"):platform_=="YouTube"?QColor("#ff1538"):platform_=="Twitch"?QColor("#9146ff"):QColor("#06382c");
        p.setPen(Qt::NoPen);p.setBrush(color);p.drawRoundedRect(QRectF(0,0,32,32),9,9);p.setBrush(Qt::white);
        if(platform_=="YouTube"){
            p.drawRoundedRect(QRectF(5,8,22,16),5,5);p.setBrush(color);QPainterPath play;play.moveTo(13,11);play.lineTo(22,16);play.lineTo(13,21);play.closeSubpath();p.drawPath(play);
        }else if(platform_=="Kick"){
            p.setBrush(QColor("#10200c"));
            const int pixels[][2]={{0,0},{0,1},{0,2},{0,3},{0,4},{1,2},{2,1},{2,2},{2,3},{3,0},{3,1},{3,3},{3,4}};
            for(const auto& cell:pixels)p.drawRect(QRectF(8+cell[0]*4,6+cell[1]*4,4,4));
        }else if(platform_=="Twitch"){
            QPainterPath bubble;bubble.moveTo(7,6);bubble.lineTo(26,6);bubble.lineTo(26,21);bubble.lineTo(20,27);bubble.lineTo(14,27);bubble.lineTo(10,30);bubble.lineTo(10,25);bubble.lineTo(7,25);bubble.closeSubpath();p.drawPath(bubble);p.setPen(QPen(color,2));p.drawLine(15,11,15,19);p.drawLine(21,11,21,19);
        }else{
            const QColor green("#00df8c");p.setBrush(green);p.drawEllipse(QPointF(16,16),3,3);p.setBrush(Qt::NoBrush);p.setPen(QPen(green,2.5,Qt::SolidLine,Qt::RoundCap));p.drawArc(QRectF(7,7,18,18),-50*16,100*16);p.drawArc(QRectF(7,7,18,18),130*16,100*16);p.drawArc(QRectF(3,3,26,26),-45*16,90*16);p.drawArc(QRectF(3,3,26,26),135*16,90*16);
        }
    }
private: QString platform_;
};

class PlatformToggle final : public QAbstractButton {
public:
    explicit PlatformToggle(QString platform,QWidget* parent=nullptr):QAbstractButton(parent),platform_(std::move(platform)){
        setCheckable(true);setFixedSize(30,18);setCursor(Qt::PointingHandCursor);
        setAccessibleName("Отправлять в "+platform_);setToolTip("Отправлять в "+platform_);
    }
protected:
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setOpacity(isEnabled()?1.0:0.45);
        p.setPen(QPen(isChecked()?QColor("#00cf84"):QColor("#34465d"),isChecked()?2:1));
        p.setBrush(isChecked()?QColor("#102c29"):QColor("#121b27"));
        p.drawRoundedRect(QRectF(1,1,28,16),5,5);
        p.translate(8,2);p.scale(14.0/32.0,14.0/32.0);
        const QColor brand=platform_=="Kick"?QColor("#53fc18"):platform_=="YouTube"?QColor("#ff1538"):QColor("#9146ff");
        p.setPen(Qt::NoPen);p.setBrush(brand);p.drawRoundedRect(QRectF(0,0,32,32),9,9);p.setBrush(Qt::white);
        if(platform_=="Kick"){
            p.setBrush(QColor("#10200c"));const int pixels[][2]={{0,0},{0,1},{0,2},{0,3},{0,4},{1,2},{2,1},{2,2},{2,3},{3,0},{3,1},{3,3},{3,4}};
            for(const auto& cell:pixels)p.drawRect(QRectF(8+cell[0]*4,6+cell[1]*4,4,4));
        }else if(platform_=="YouTube"){
            p.drawRoundedRect(QRectF(5,8,22,16),5,5);p.setBrush(brand);QPainterPath play;play.moveTo(13,11);play.lineTo(22,16);play.lineTo(13,21);play.closeSubpath();p.drawPath(play);
        }else{
            QPainterPath bubble;bubble.moveTo(7,6);bubble.lineTo(26,6);bubble.lineTo(26,21);bubble.lineTo(20,27);bubble.lineTo(14,27);bubble.lineTo(10,30);bubble.lineTo(10,25);bubble.lineTo(7,25);bubble.closeSubpath();p.drawPath(bubble);p.setPen(QPen(brand,2));p.drawLine(15,11,15,19);p.drawLine(21,11,21,19);
        }
        if(hasFocus()){p.resetTransform();p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor("#8a79ff"),1,Qt::DotLine));p.drawRoundedRect(QRectF(0.5,0.5,29,17),5,5);}
    }
private: QString platform_;
};

QPixmap messagePlatformIcon(const QString& platform){
    if(platform=="Kick"){
        QPixmap image(36,36);image.fill(Qt::transparent);QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);p.setBrush(QColor("#53fc18"));p.drawRoundedRect(QRectF(1,1,34,34),9,9);p.setBrush(QColor("#10200c"));
        const int pixels[][2]={{0,0},{0,1},{0,2},{0,3},{0,4},{1,2},{2,1},{2,2},{2,3},{3,0},{3,1},{3,3},{3,4}};
        for(const auto& cell:pixels)p.drawRect(QRectF(9+cell[0]*4.5,6.75+cell[1]*4.5,4.5,4.5));return image;
    }
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
QString compactStatus(QString value,const QString& platform){
    value.remove(platform+": ");
    if(platform=="Kick"&&value.contains("укажите канал"))return "Укажите канал";
    if(value.contains("укажите Client ID"))return "Настройте вход";
    if(platform=="Kick"&&value.contains("повторный вход"))return "Войдите снова";
    if(platform=="Kick"&&value.contains("ошибка получения канала (HTTP 404)"))return "Канал не найден";
    if(value.contains("активный эфир с чатом не найден",Qt::CaseInsensitive))return "Эфир не найден";
    if(value.contains("чат подключён",Qt::CaseInsensitive))return "Подключён";
    if(value.contains("не подключён",Qt::CaseInsensitive))return "Не подключён";
    if(value.contains("вход выполнен",Qt::CaseInsensitive))return "Подключён";
    if(value.contains("подтвердите",Qt::CaseInsensitive))return "Подтвердите вход";
    if(value.contains("завершаем вход",Qt::CaseInsensitive))return "Завершаем вход";
    if(value.contains("ищем активный эфир",Qt::CaseInsensitive))return "Ищем эфир";
    if(value.contains("подключаем поток",Qt::CaseInsensitive))return "Подключаем чат";
    if(value.contains("восстанавливаем",Qt::CaseInsensitive))return "Подключение";
    if(value.contains("HTTP",Qt::CaseInsensitive))return "Ошибка входа";
    if(value.contains("не удалось",Qt::CaseInsensitive)||value.contains("ошиб",Qt::CaseInsensitive))return "Ошибка подключения";
    if(value.contains("эфир завершён",Qt::CaseInsensitive))return "Эфир завершён";
    return value.size()>22?value.left(21)+"…":value;
}
QString englishStatus(const QString& value){
    if(value=="Укажите канал")return "Set a channel";
    if(value=="Настройте вход")return "Set up sign-in";
    if(value=="Войдите снова")return "Sign in again";
    if(value=="Канал не найден")return "Channel not found";
    if(value=="Подключён")return "Connected";
    if(value=="Не подключён")return "Not connected";
    if(value=="Подтвердите вход")return "Confirm sign-in";
    if(value=="Завершаем вход")return "Signing in";
    if(value=="Ищем эфир")return "Finding stream";
    if(value=="Эфир не найден")return "Stream not found";
    if(value=="Подключаем чат"||value=="Подключение")return "Connecting";
    if(value=="Ошибка входа")return "Sign-in error";
    if(value=="Ошибка подключения")return "Connection error";
    if(value=="Эфир завершён")return "Stream ended";
    if(value.contains("приостановлен",Qt::CaseInsensitive))return "Paused";
    if(value.contains("ожидаем",Qt::CaseInsensitive)||value.contains("ждём",Qt::CaseInsensitive))return "Waiting for stream";
    if(value.contains("отключён",Qt::CaseInsensitive))return "Disconnected";
    return "Connecting";
}
void paintStatus(QLabel* label,const QString& raw,const QString& platform,bool ready,bool english){
    const QString value=compactStatus(raw,platform);QString color="#8ea1c0";
    if(ready)color="#07df91";
    else if(value.contains("ошиб",Qt::CaseInsensitive)||value.contains("отклон",Qt::CaseInsensitive))color="#ff8484";
    else if(value=="Подключение"||value.contains("подтверд",Qt::CaseInsensitive)||value.contains("заверша",Qt::CaseInsensitive)||value.contains("ищем",Qt::CaseInsensitive)||value.contains("эфир не найден",Qt::CaseInsensitive))color="#ffdc45";
    const QString shown=english?englishStatus(value):value;
    const bool changed=label->property("statusShown").isValid()&&label->property("statusShown").toString()!=shown;
    if(english){const auto http=raw.indexOf("(HTTP ");label->setToolTip(http<0?shown:shown+" "+raw.mid(http));}
    else label->setToolTip(raw);
    label->setText("● "+shown);label->setStyleSheet(QString("color:%1;font-size:11px;").arg(color));
    label->setProperty("statusShown",shown);
    if(changed&&label->isVisible()){
        auto* effect=qobject_cast<QGraphicsOpacityEffect*>(label->graphicsEffect());
        if(!effect){effect=new QGraphicsOpacityEffect(label);label->setGraphicsEffect(effect);}
        if(auto* prior=effect->findChild<QPropertyAnimation*>()){prior->stop();prior->deleteLater();}
        auto* fade=new QPropertyAnimation(effect,"opacity",effect);fade->setDuration(170);
        fade->setStartValue(0.65);fade->setEndValue(1.0);fade->start(QAbstractAnimation::DeleteWhenStopped);
    }
}
QString configPath(const char* name){
#ifdef STREAM_CHAT_PREVIEW
    return QString::fromUtf8(qgetenv("STREAM_CHAT_PREVIEW_CONFIG"))+"/"+QString::fromUtf8(name);
#else
    char* raw=obs_module_config_path(name);if(!raw)return {};const auto path=QString::fromUtf8(raw);bfree(raw);return path;
#endif
}
QString volume(quint64 bytes,bool english=false){
    const QLocale locale(english?QLocale::English:QLocale::Russian);
    if(bytes>=1024ULL*1024*1024)return locale.toString(double(bytes)/(1024.0*1024*1024),'f',2)+(english?" GB":" ГБ");
    if(bytes>=1024ULL*1024)return locale.toString(double(bytes)/(1024.0*1024),'f',1)+(english?" MB":" МБ");
    if(bytes>=1024)return locale.toString(double(bytes)/1024.0,'f',1)+(english?" KB":" КБ");
    return locale.toString(qlonglong(bytes))+(english?" B":" Б");
}
bool anyStreamingOutput(){
#ifdef STREAM_CHAT_PREVIEW
    return false;
#else
    bool active=obs_frontend_streaming_active();
    obs_enum_outputs([](void* context,obs_output_t* output){
        if((obs_output_get_flags(output)&OBS_OUTPUT_SERVICE)&&obs_output_active(output)){
            *static_cast<bool*>(context)=true;return false;
        }
        return true;
    },&active);
    return active;
#endif
}
class WrappedChatEdit final : public QTextEdit {
public:
    void setPlaceholderText(const QString& text){placeholder_=text;QTextEdit::setPlaceholderText({});viewport()->update();}
protected:
    void paintEvent(QPaintEvent* event)override{
        QTextEdit::paintEvent(event);
        if(!document()->isEmpty()||placeholder_.isEmpty())return;
        QPainter painter(viewport());painter.setPen(QColor("#8ea1c0"));painter.setFont(font());
        painter.drawText(viewport()->rect().adjusted(8,8,-8,-8),Qt::AlignLeft|Qt::AlignTop|Qt::TextWordWrap,placeholder_);
    }
private:
    QString placeholder_;
};
class ChatDock final : public QWidget {
public:
#ifdef STREAM_CHAT_PREVIEW
    void previewAppend(const sc::ChatMessage& message){history_.enqueue(message);appendMessage(message);}
    void previewRedraw(){redrawMessages();}
    void previewCounters(quint64 tw,quint64 yt,quint64 ki){streaming_=true;seenThisStream_=true;twitchMessages_=tw;youtubeMessages_=yt;kickMessages_=ki;refresh();}
#endif
    ChatDock():twitch_(configPath("twitch-account.dat"),this),youtube_(configPath("youtube-account.dat"),this),kick_(configPath("kick-account.dat"),this){
        {QSettings saved(configPath("ui.ini"),QSettings::IniFormat);textScale_=std::clamp(saved.value("textScale",100).toInt(),80,140);
            mentionNames_=saved.value("mentionNames").toString();highlightMentions_=saved.value("highlightMentions",true).toBool();
            hideBots_=saved.value("hideBots",false).toBool();botNames_=saved.value("botNames","Nightbot, WizeBot, BotRix, BotRixOfficial, StreamElements, Streamlabs, Moobot").toString();kickEnabled_=saved.value("kickEnabled",false).toBool();}
        setObjectName("StreamChatRoot");setMinimumWidth(275);setStyleSheet(ChatStyle);
        pages_=new QStackedLayout(this);auto* chatPage=new QWidget(this);pages_->addWidget(chatPage);
        auto* layout=new QVBoxLayout(chatPage);layout->setContentsMargins(8,8,8,8);layout->setSpacing(5);
        auto* header=new QHBoxLayout;header->setContentsMargins(2,2,2,2);header->setSpacing(7);auto* brand=new PlatformBadge("Stream Chat");brand->setObjectName("HeaderBrand");header->addWidget(brand);auto* title=new QLabel("Stream Chat");title->setObjectName("Title");header->addWidget(title,1);settings_=new QPushButton;settings_->setObjectName("Settings");settings_->setIcon(settingsIcon());settings_->setIconSize(QSize(19,19));settings_->setFixedSize(30,28);settings_->setToolTip("Настройки");settings_->setAccessibleName("Настройки");header->addWidget(settings_);layout->addLayout(header);

        auto* twitchCard=card();auto* twitchLayout=new QVBoxLayout(twitchCard);twitchLayout->setContentsMargins(8,6,8,6);twitchLayout->setSpacing(5);
        auto* twitchHead=new QHBoxLayout;twitchHead->setSpacing(6);twitchHead->addWidget(new PlatformBadge("Twitch"));twitchHead->addWidget(sectionTitle("Twitch"));twitchViewers_=new QLabel;twitchViewers_->setObjectName("Hint");twitchViewers_->setToolTip("Смотрят Twitch сейчас");twitchHead->addWidget(twitchViewers_);twitchStatus_=new QLabel;twitchStatus_->setAlignment(Qt::AlignRight|Qt::AlignVCenter);twitchStatus_->setWordWrap(true);twitchStatus_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);twitchHead->addWidget(twitchStatus_,1);login_=new QPushButton("Войти");twitchHead->addWidget(login_);logout_=new QPushButton("×");logout_->setObjectName("Disconnect");logout_->setToolTip("Отключить Twitch");twitchHead->addWidget(logout_);twitchLayout->addLayout(twitchHead);
        connect(login_,&QPushButton::clicked,this,[this]{if(twitch_.configured())twitch_.signIn();else openSettings();});connect(logout_,&QPushButton::clicked,&twitch_,&sc::TwitchChat::signOut);
        code_=new QLabel;code_->setObjectName("Callout");code_->setTextFormat(Qt::PlainText);code_->setWordWrap(true);code_->setTextInteractionFlags(Qt::TextSelectableByMouse);code_->hide();twitchLayout->addWidget(code_);layout->addWidget(twitchCard);

        auto* youtubeCard=card();auto* youtubeLayout=new QVBoxLayout(youtubeCard);youtubeLayout->setContentsMargins(8,6,8,6);youtubeLayout->setSpacing(5);
        auto* youtubeHead=new QHBoxLayout;youtubeHead->setSpacing(6);youtubeHead->addWidget(new PlatformBadge("YouTube"));youtubeHead->addWidget(sectionTitle("YouTube"));youtubeViewers_=new QLabel;youtubeViewers_->setObjectName("Hint");youtubeViewers_->setToolTip("Смотрят YouTube сейчас");youtubeHead->addWidget(youtubeViewers_);youtubeStatus_=new QLabel;youtubeStatus_->setAlignment(Qt::AlignRight|Qt::AlignVCenter);youtubeStatus_->setWordWrap(true);youtubeStatus_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);youtubeHead->addWidget(youtubeStatus_,1);googleLogin_=new QPushButton("Войти");youtubeHead->addWidget(googleLogin_);googleLogout_=new QPushButton("×");googleLogout_->setObjectName("Disconnect");googleLogout_->setToolTip("Отключить YouTube");youtubeHead->addWidget(googleLogout_);youtubeLayout->addLayout(youtubeHead);
        connect(googleLogin_,&QPushButton::clicked,this,[this]{if(youtube_.configured())youtube_.signIn();else openSettings();});connect(googleLogout_,&QPushButton::clicked,&youtube_,&sc::YouTubeChat::signOut);
        broadcast_=new QComboBox;broadcast_->addItem("Выберите эфир",QString());youtubeLayout->addWidget(broadcast_);layout->addWidget(youtubeCard);
        connect(broadcast_,&QComboBox::currentIndexChanged,this,[this](int index){const auto id=broadcast_->itemData(index).toString();if(!id.isEmpty())youtube_.selectChat(id);});
        connect(&youtube_,&sc::YouTubeChat::broadcasts,this,[this](const QJsonArray& choices){
            const QSignalBlocker block(broadcast_);broadcast_->clear();broadcast_->addItem(english_?"Choose stream":"Выберите эфир",QString());
            for(const auto& value:choices){const auto choice=value.toObject();broadcast_->addItem(choice.value("title").toString(),choice.value("id").toString());}
            if(choices.size()==1)broadcast_->setCurrentIndex(1);
            refresh();
        });
        kickCard_=card();kickCard_->setObjectName("KickCard");auto* kickLayout=new QHBoxLayout(kickCard_);kickLayout->setContentsMargins(8,6,8,6);kickLayout->setSpacing(6);
        kickLayout->addWidget(new PlatformBadge("Kick"));kickLayout->addWidget(sectionTitle("Kick"));kickViewers_=new QLabel;kickLayout->addWidget(kickViewers_);kickViewers_->hide();
        kickStatus_=new QLabel;kickStatus_->setWordWrap(true);kickStatus_->setAlignment(Qt::AlignRight|Qt::AlignVCenter);kickStatus_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);kickLayout->addWidget(kickStatus_,1);
        kickLogin_=new QPushButton("Настроить");kickLayout->addWidget(kickLogin_);kickLogout_=new QPushButton("×");kickLogout_->setObjectName("Disconnect");kickLayout->addWidget(kickLogout_);
        connect(kickLogin_,&QPushButton::clicked,this,[this]{openSettings();});connect(kickLogout_,&QPushButton::clicked,&kick_,&sc::KickChat::signOut);
        kickCard_->setVisible(kickEnabled_);layout->addWidget(kickCard_);
        auto* chatCard=card();auto* chatLayout=new QVBoxLayout(chatCard);chatLayout->setContentsMargins(8,7,8,8);chatLayout->setSpacing(5);auto* chatHead=new QHBoxLayout;chatTitle_=sectionTitle("Чат");chatHead->addWidget(chatTitle_);chatHead->addStretch();scrollBottom_=new QPushButton("↓");scrollBottom_->setObjectName("ScrollBottom");scrollBottom_->setToolTip("К новым сообщениям");scrollBottom_->hide();chatHead->addWidget(scrollBottom_);totalCount_=new QLabel;totalCount_->setObjectName("Hint");totalCount_->setToolTip("Всего сообщений за этот запуск OBS");chatHead->addWidget(totalCount_);chatLayout->addLayout(chatHead);
        messages_=new WrappedChatEdit;messages_->setObjectName("ChatMessages");messages_->setReadOnly(true);messages_->setAcceptRichText(false);messages_->setFocusPolicy(Qt::NoFocus);messages_->setTextInteractionFlags(Qt::NoTextInteraction);messages_->setContextMenuPolicy(Qt::NoContextMenu);messages_->setCursor(Qt::ArrowCursor);messages_->setLineWrapMode(QTextEdit::WidgetWidth);messages_->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);messages_->document()->setMaximumBlockCount(500);messages_->setPlaceholderText("Здесь появятся сообщения. История хранится только до закрытия OBS.");
        applyChatTextSize();
        messages_->document()->addResource(QTextDocument::ImageResource,QUrl("stream-chat://icon/twitch"),messagePlatformIcon("Twitch"));messages_->document()->addResource(QTextDocument::ImageResource,QUrl("stream-chat://icon/youtube"),messagePlatformIcon("YouTube"));messages_->document()->addResource(QTextDocument::ImageResource,QUrl("stream-chat://icon/kick"),messagePlatformIcon("Kick"));chatLayout->addWidget(messages_,1);
        recipientHint_=new QLabel("Куда отправлять");recipientHint_->setObjectName("Hint");chatLayout->addWidget(recipientHint_);
        recipientGroups_=new QWidget;recipientLayout_=new QVBoxLayout(recipientGroups_);recipientLayout_->setContentsMargins(0,0,0,0);recipientLayout_->setSpacing(3);chatLayout->addWidget(recipientGroups_);
        auto recipientPair=[this](const QString& platform,QAbstractButton*& toggle,QLabel*& count){
            auto* pair=new QWidget(recipientGroups_);pair->setVisible(true);pair->setObjectName(platform+"Recipient");auto* row=new QHBoxLayout(pair);row->setContentsMargins(0,0,0,0);row->setSpacing(2);
            toggle=new PlatformToggle(platform);toggle->setChecked(true);row->addWidget(toggle);
            count=new QLabel;count->setObjectName(platform+"MessageCount");count->setStyleSheet("color:#dfe8ff;font-size:14px;font-weight:600;");row->addWidget(count);
            pair->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Preferred);recipientPairs_.append(pair);
        };
        recipientPair("Twitch",target_,twitchCount_);recipientPair("YouTube",youtubeTarget_,youtubeCount_);recipientPair("Kick",kickTarget_,kickCount_);
        recipientPairs_.last()->setVisible(kickEnabled_);layout->addWidget(chatCard,1);
        totalCount_->setStyleSheet("color:#8ea1c0;font-size:13px;font-weight:600;");

        lastMessage_=new QLabel;lastMessage_->setObjectName("Hint");lastMessage_->setWordWrap(true);chatLayout->addWidget(lastMessage_);
        auto* composerCard=card();auto* composerLayout=new QVBoxLayout(composerCard);composerLayout->setContentsMargins(8,7,8,8);composerLayout->setSpacing(5);auto* composer=new QHBoxLayout;input_=new QLineEdit;input_->setMaxLength(500);input_->setPlaceholderText("Написать в чат…");send_=new QPushButton("Отправить");send_->setObjectName("Primary");composer->addWidget(input_,1);composer->addWidget(send_);composerLayout->addLayout(composer);delivery_=new QLabel;delivery_->setObjectName("Hint");delivery_->setWordWrap(true);delivery_->setTextFormat(Qt::PlainText);delivery_->hide();composerLayout->addWidget(delivery_);layout->addWidget(composerCard);
        auto* footer=new QVBoxLayout;footer->setContentsMargins(0,0,0,0);footer->setSpacing(0);auto* version=new QLabel("developed by <a href=\"https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/\" style=\"color:#59dfff;text-decoration:none;\">OTMOPOZOK</a> · v" STREAM_CHAT_VERSION);version->setObjectName("Credit");version->setTextFormat(Qt::RichText);version->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::LinksAccessibleByKeyboard);version->setOpenExternalLinks(true);version->setAlignment(Qt::AlignCenter);version->setFixedHeight(20);footer->addWidget(version);layout->addLayout(footer);
        batch_.setInterval(20);batch_.setSingleShot(true);trafficTimer_.setInterval(2000);
        emoteRedraw_.setSingleShot(true);emoteRedraw_.setInterval(120);
        connect(&emoteRedraw_,&QTimer::timeout,this,[this]{redrawMessages();});
        connect(&batch_,&QTimer::timeout,this,[this]{flush();});
        connect(scrollBottom_,&QPushButton::clicked,this,[this]{messages_->verticalScrollBar()->setValue(messages_->verticalScrollBar()->maximum());unread_=0;updateScrollButton();});
        connect(messages_->verticalScrollBar(),&QScrollBar::valueChanged,this,[this]{if(redrawing_)return;if(atBottom())unread_=0;updateScrollButton();});
        connect(&trafficTimer_,&QTimer::timeout,this,[this]{setStreaming(anyStreamingOutput());updateMetrics();updateLastMessage();});
        connect(send_,&QPushButton::clicked,this,[this]{send();});connect(input_,&QLineEdit::returnPressed,this,[this]{send();});
        connect(target_,&QAbstractButton::toggled,this,[this]{refresh();});
        connect(youtubeTarget_,&QAbstractButton::toggled,this,[this]{refresh();});connect(kickTarget_,&QAbstractButton::toggled,this,[this]{refresh();});
        connect(&kick_,&sc::KickChat::changed,this,[this]{refresh();});
        connect(&kick_,&sc::KickChat::message,this,[this](const sc::ChatMessage& m){pending_.enqueue(m);while(pending_.size()>500)pending_.dequeue();if(!batch_.isActive())batch_.start();});
        connect(&kick_,&sc::KickChat::removeMessage,this,[this](const QString& id){remove([&](const sc::ChatMessage& m){return m.platform=="Kick"&&m.id==id;});});
        connect(&kick_,&sc::KickChat::sent,this,[this](bool ok,const QString& reason){completed("Kick",ok,reason);});
        connect(input_,&QLineEdit::textChanged,this,[this]{refresh();});
        connect(&twitch_,&sc::TwitchChat::changed,this,[this]{if(twitch_.ready())code_->hide();refresh();});
        connect(&youtube_,&sc::YouTubeChat::changed,this,[this]{refresh();});
        connect(&twitch_,&sc::TwitchChat::signInCode,this,[this](const QString& code,const QUrl& url){code_->setText((english_?"Twitch code: ":"Код Twitch: ")+code.left(64)+"\n"+url.toString());code_->show();});
        connect(&twitch_,&sc::TwitchChat::message,this,[this](const sc::ChatMessage& message){pending_.enqueue(message);while(pending_.size()>500)pending_.dequeue();if(!batch_.isActive())batch_.start();});
        connect(&youtube_,&sc::YouTubeChat::message,this,[this](const sc::ChatMessage& message){pending_.enqueue(message);while(pending_.size()>500)pending_.dequeue();if(!batch_.isActive())batch_.start();});
        connect(&twitch_,&sc::TwitchChat::removeMessage,this,[this](const QString& id){remove([&](const sc::ChatMessage& m){return m.platform=="Twitch"&&m.id==id;});});
        connect(&youtube_,&sc::YouTubeChat::removeMessage,this,[this](const QString& id){remove([&](const sc::ChatMessage& m){return m.platform=="YouTube"&&m.id==id;});});
        connect(&twitch_,&sc::TwitchChat::clearAuthor,this,[this](const QString& id){remove([&](const sc::ChatMessage& m){return m.platform=="Twitch"&&m.authorId==id;});});
        connect(&twitch_,&sc::TwitchChat::clearChat,this,[this]{remove([](const sc::ChatMessage& m){return m.platform=="Twitch";});});
        connect(&twitch_,&sc::TwitchChat::sent,this,[this](bool confirmed,const QString& reason){completed("Twitch",confirmed,reason);});
        connect(&youtube_,&sc::YouTubeChat::sent,this,[this](bool confirmed,const QString& reason){completed("YouTube",confirmed,reason);});
        connect(settings_,&QPushButton::clicked,this,[this]{openSettings();});
        {QSettings saved(configPath("ui.ini"),QSettings::IniFormat);language_=saved.value("language","auto").toString();if(language_!="auto"&&language_!="ru"&&language_!="en")language_="auto";english_=language_=="en"||(language_=="auto"&&obsLanguageIsEnglish());}
        applyLanguage();
        streaming_=anyStreamingOutput();twitch_.setStreamActive(streaming_);youtube_.setStreamActive(streaming_);kick_.setStreamActive(streaming_);
        kick_.setEnabled(kickEnabled_);refresh();updateLastMessage();updateMetrics();
    }
    void setStreaming(bool active){if(streaming_==active)return;streaming_=active;seenThisStream_=false;twitch_.setStreamActive(active);youtube_.setStreamActive(active);kick_.setStreamActive(active);refresh();}
protected:
    void resizeEvent(QResizeEvent* event)override{QWidget::resizeEvent(event);layoutRecipients();}
    void showEvent(QShowEvent* event)override{QWidget::showEvent(event);setStreaming(anyStreamingOutput());twitch_.setVisible(true);youtube_.setVisible(true);kick_.setVisible(true);QTimer::singleShot(0,this,[this]{if(isVisible()){twitch_.setVisible(true);youtube_.setVisible(true);kick_.setVisible(true);refresh();}});trafficTimer_.start();updateLastMessage();updateMetrics();if(!pending_.isEmpty())batch_.start();if(emotesDirty_)emoteRedraw_.start();}
    void hideEvent(QHideEvent* event)override{twitch_.setVisible(false);youtube_.setVisible(false);kick_.setVisible(false);batch_.stop();trafficTimer_.stop();emoteRedraw_.stop();if(activeSends_)delivery_->setText(english_?"Dock hidden: delivery unconfirmed. No automatic retry.":"Панель скрыта: результат отправки не подтверждён. Автоматического повтора нет.");activeSends_=0;refresh();QWidget::hideEvent(event);}
private:
    void layoutRecipients(){
        if(!recipientLayout_)return;const int available=std::max(1,width()-32);QString key=QString::number(available);
        for(auto* pair:recipientPairs_)key+="/"+QString::number(pair->isHidden()?-1:pair->sizeHint().width());
        if(key==recipientLayoutKey_)return;recipientLayoutKey_=key;
        while(auto* item=recipientLayout_->takeAt(0))delete item;
        QHBoxLayout* row=nullptr;int used=0;
        for(auto* pair:recipientPairs_){if(pair->isHidden())continue;const int width=pair->sizeHint().width();
            if(!row||used+8+width>available){if(row)row->addStretch();row=new QHBoxLayout;row->setContentsMargins(0,0,0,0);row->setSpacing(8);recipientLayout_->addLayout(row);used=0;}
            row->addWidget(pair);pair->setVisible(true);used+=width+(used?8:0);
        }
        if(row)row->addStretch();
    }
    void applyChatTextSize(){
        QFont font=messages_->font();font.setPixelSize(std::max(9,12*textScale_/100));
        messages_->setFont(font);messages_->document()->setDefaultFont(font);
    }
    void openSettings(){
        if(pages_->count()>1){pages_->setCurrentIndex(1);return;}
        auto* page=new QWidget(this);page->setObjectName("SettingsPage");pages_->addWidget(page);pages_->setCurrentWidget(page);
        page->setStyleSheet("QWidget#SettingsPage{background:#080f18;color:#dfe8ff;} QWidget#LanguageCard{background:#0b131e;border:1px solid #263244;border-radius:8px;} QLabel{color:#dfe8ff;font-weight:700;} QPushButton{color:#dfe8ff;background:#172236;border:1px solid #354863;border-radius:7px;padding:5px 12px;} QPushButton#ActiveNav{background:#18234e;border-color:#7663ff;color:#ffffff;font-weight:700;}");
        auto* outer=new QVBoxLayout(page);outer->setContentsMargins(8,8,8,8);outer->setSpacing(8);
        auto* head=new QHBoxLayout;auto* back=new QPushButton(english_?"← Back":"← Назад");head->addWidget(back);auto* title=new QLabel(english_?"Settings":"Настройки");title->setObjectName("Title");head->addWidget(title,1);outer->addLayout(head);
        auto* settingsScroll=new QScrollArea;settingsScroll->setWidgetResizable(true);settingsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        settingsScroll->setStyleSheet("QScrollArea{border:0;background:#080f18;} QCheckBox{color:#dfe8ff;background:transparent;} QLineEdit{color:#dfe8ff;background:#0c1420;border:1px solid #344865;border-radius:5px;padding:4px;}");
        settingsScroll->viewport()->setStyleSheet("background:#080f18;");
        auto* settingsContent=new QWidget;settingsContent->setObjectName("SettingsContent");settingsContent->setStyleSheet("QWidget#SettingsContent{background:#080f18;}");settingsScroll->setWidget(settingsContent);outer->addWidget(settingsScroll,1);
        outer=new QVBoxLayout(settingsContent);outer->setContentsMargins(0,0,0,0);outer->setSpacing(8);
        connect(back,&QPushButton::clicked,page,[this,page]{pages_->setCurrentIndex(0);pages_->removeWidget(page);page->deleteLater();});
        auto* card=new QWidget(page);card->setObjectName("LanguageCard");outer->addWidget(card);
        auto* layout=new QVBoxLayout(card);layout->setContentsMargins(8,6,8,8);layout->setSpacing(4);
        auto* label=new QLabel(english_?"Interface language":"Язык интерфейса");layout->addWidget(label);
        auto* line=new QHBoxLayout;line->setSpacing(4);
        const struct {const char* code;QString title;} languages[]={
            {"auto",english_?"Auto":"Авто"},{"ru",QString::fromUtf8("Русский")},{"en",QString("English")}
        };
        auto choices=std::make_shared<QVector<QPushButton*>>();
        for(const auto& item:languages){
            auto* choice=new QPushButton(item.title);choice->setCheckable(true);choice->setChecked(language_==item.code);
            if(choice->isChecked())choice->setObjectName("ActiveNav");
            const QString code=QString::fromLatin1(item.code);
            connect(choice,&QPushButton::clicked,page,[this,label,title,back,choices,code]{
                language_=code;english_=language_=="en"||(language_=="auto"&&obsLanguageIsEnglish());
                QSettings saved(configPath("ui.ini"),QSettings::IniFormat);saved.setValue("language",language_);saved.sync();applyLanguage();
                title->setText(english_?"Settings":"Настройки");back->setText(english_?"← Back":"← Назад");label->setText(english_?"Interface language":"Язык интерфейса");
                (*choices)[0]->setText(english_?"Auto":"Авто");
                for(auto* button:*choices){button->setChecked(button->property("localeCode").toString()==language_);button->setObjectName(button->isChecked()?"ActiveNav":"");button->style()->unpolish(button);button->style()->polish(button);}
            });
            choice->setProperty("localeCode",code);choices->push_back(choice);line->addWidget(choice,1);
        }
        layout->addLayout(line);
        auto* scaleCard=new QWidget(page);scaleCard->setObjectName("LanguageCard");scaleCard->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);outer->addWidget(scaleCard);
        auto* scaleLayout=new QVBoxLayout(scaleCard);scaleLayout->setContentsMargins(8,6,8,8);scaleLayout->setSpacing(4);
        auto* scaleTitle=new QLabel(english_?"Chat text size":"Размер текста чата");scaleLayout->addWidget(scaleTitle);
        auto* scaleRow=new QHBoxLayout;auto* slider=new QSlider(Qt::Horizontal);slider->setRange(80,140);slider->setSingleStep(5);slider->setPageStep(10);slider->setValue(textScale_);scaleRow->addWidget(slider,1);
        auto* percent=new QLabel(QString::number(textScale_)+" %");scaleRow->addWidget(percent);scaleLayout->addLayout(scaleRow);
        auto* preview=new QTextEdit;preview->setReadOnly(true);preview->setFocusPolicy(Qt::NoFocus);preview->setTextInteractionFlags(Qt::NoTextInteraction);preview->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);preview->setFixedHeight(54);scaleLayout->addWidget(preview);
        auto updatePreview=[this,preview,percent]{
            percent->setText(QString::number(textScale_)+" %");
            preview->setStyleSheet(QString("QTextEdit{color:#dfe8ff;background:#0c1420;border:1px solid #344865;border-radius:6px;padding:5px;font-size:%1px;}").arg(12*textScale_/100));
            preview->clear();preview->document()->addResource(QTextDocument::ImageResource,QUrl("stream-chat://icon/twitch"),messagePlatformIcon("Twitch"));
            QTextCursor cursor(preview->document());QTextImageFormat icon;icon.setName("stream-chat://icon/twitch");icon.setWidth(14);icon.setHeight(14);cursor.insertImage(icon);cursor.insertText(" ");
            QTextCharFormat author;author.setForeground(QColor("#b995ff"));author.setBackground(QColor("#241b35"));author.setFontWeight(QFont::DemiBold);cursor.insertText(" Viewer ",author);
            QTextCharFormat time;time.setForeground(QColor("#8ea1c0"));cursor.insertText(" 14:32",time);
            QTextCharFormat body;body.setForeground(QColor("#dfe8ff"));cursor.insertText(english_?"  Hello from Twitch!":"  Привет из Twitch!",body);
        };
        updatePreview();
        for(auto* choice:*choices)connect(choice,&QPushButton::clicked,page,[this,scaleTitle,updatePreview]{scaleTitle->setText(english_?"Chat text size":"Размер текста чата");updatePreview();});
        connect(slider,&QSlider::valueChanged,page,[this,updatePreview](int value){
            textScale_=value;applyChatTextSize();redrawMessages();updatePreview();
            QSettings saved(configPath("ui.ini"),QSettings::IniFormat);saved.setValue("textScale",textScale_);saved.sync();
        });
        auto* filters=new QWidget;filters->setObjectName("LanguageCard");auto* filterLayout=new QVBoxLayout(filters);
        auto* highlight=new QCheckBox(english_?"Highlight mentions":"Выделять упоминания");highlight->setChecked(highlightMentions_);filterLayout->addWidget(highlight);
        auto* namesLabel=new QLabel(english_?"Your names, separated by commas":"Ваши имена через запятую");namesLabel->setWordWrap(true);filterLayout->addWidget(namesLabel);
        auto* names=new QLineEdit(mentionNames_);names->setMaxLength(500);names->setPlaceholderText(english_?"Twitch login is detected automatically":"Имя Twitch определяется автоматически");filterLayout->addWidget(names);
        auto* hideBots=new QCheckBox(english_?"Hide listed bots":"Скрывать ботов из списка");hideBots->setChecked(hideBots_);filterLayout->addWidget(hideBots);
        auto* bots=new QLineEdit(botNames_);bots->setMaxLength(1000);bots->setToolTip(english_?"Exact account names, separated by commas":"Точные имена аккаунтов через запятую");filterLayout->addWidget(bots);outer->addWidget(filters);
        names->setCursorPosition(0);bots->setCursorPosition(0);
        for(auto* choice:*choices)connect(choice,&QPushButton::clicked,page,[this,highlight,namesLabel,names,hideBots,bots]{
            highlight->setText(english_?"Highlight mentions":"Выделять упоминания");namesLabel->setText(english_?"Your names, separated by commas":"Ваши имена через запятую");
            names->setPlaceholderText(english_?"Twitch login is detected automatically":"Имя Twitch определяется автоматически");hideBots->setText(english_?"Hide listed bots":"Скрывать ботов из списка");bots->setToolTip(english_?"Exact account names, separated by commas":"Точные имена аккаунтов через запятую");
        });
        auto saveFilters=[this,highlight,names,hideBots,bots]{
            highlightMentions_=highlight->isChecked();mentionNames_=names->text();hideBots_=hideBots->isChecked();botNames_=bots->text();
            QSettings saved(configPath("ui.ini"),QSettings::IniFormat);saved.setValue("highlightMentions",highlightMentions_);saved.setValue("mentionNames",mentionNames_);saved.setValue("hideBots",hideBots_);saved.setValue("botNames",botNames_);redrawMessages();
        };
        connect(highlight,&QCheckBox::toggled,page,[saveFilters](bool){saveFilters();});
        connect(hideBots,&QCheckBox::toggled,page,[saveFilters](bool){saveFilters();});
        connect(names,&QLineEdit::editingFinished,page,saveFilters);connect(bots,&QLineEdit::editingFinished,page,saveFilters);
        auto addPersonalAccount=[this,page,outer,choices](bool youtube){
            auto* group=new QWidget;group->setObjectName(youtube?"YouTubePersonalSettings":"TwitchPersonalSettings");
            group->setStyleSheet("QWidget#YouTubePersonalSettings,QWidget#TwitchPersonalSettings{background:#0b131e;border:1px solid #263244;border-radius:8px;}");outer->addWidget(group);
            auto* box=new QVBoxLayout(group);box->setContentsMargins(8,8,8,8);box->setSpacing(6);
            box->addWidget(sectionTitle(youtube?"YouTube":"Twitch"));
            auto* hint=new QLabel;hint->setWordWrap(true);hint->setStyleSheet("font-weight:400;color:#8ea1c0;");box->addWidget(hint);
            box->addWidget(new QLabel("Client ID"));auto* id=new QLineEdit(youtube?youtube_.clientId():twitch_.clientId());id->setMaxLength(240);id->setObjectName(youtube?"YouTubeClientId":"TwitchClientId");box->addWidget(id);
            QLineEdit* secret=nullptr;
            if(youtube){box->addWidget(new QLabel("Client Secret"));secret=new QLineEdit(youtube_.clientSecret());secret->setObjectName("YouTubeClientSecret");secret->setEchoMode(QLineEdit::Password);secret->setMaxLength(500);box->addWidget(secret);}
            auto* save=new QPushButton;box->addWidget(save);auto* login=new QPushButton;box->addWidget(login);auto* instructions=new QPushButton;instructions->setObjectName(youtube?"YouTubeSetupGuide":"TwitchSetupGuide");box->addWidget(instructions);
            auto* status=new QLabel;status->setWordWrap(true);status->setStyleSheet("font-weight:400;color:#8ea1c0;");box->addWidget(status);
            auto update=[this,youtube,hint,save,login,instructions,status]{
                hint->setText(youtube?(english_?"Create a personal Google desktop application, then paste its keys here. They are encrypted on this PC.":"Создайте личное приложение Google для компьютера и вставьте ключи сюда. Они сохраняются зашифрованными на этом ПК."):(english_?"Create a personal Twitch application with client type Public. Only Client ID is needed; no secret.":"Создайте личное приложение Twitch с типом Public. Нужен только Client ID, без секрета."));
                save->setText(english_?"Save application":"Сохранить приложение");login->setText(english_?"Sign in":"Войти в аккаунт");instructions->setText(english_?"Connection guide on GitHub":"Инструкция подключения в GitHub");
                const auto raw=youtube?youtube_.status():twitch_.status();status->setText(english_?englishStatus(compactStatus(raw,youtube?"YouTube":"Twitch")):raw);
            };
            update();for(auto* choice:*choices)connect(choice,&QPushButton::clicked,page,update);
            if(youtube)connect(&youtube_,&sc::YouTubeChat::changed,page,update);else connect(&twitch_,&sc::TwitchChat::changed,page,update);
            auto persist=[this,youtube,id,secret]{if(activeSends_)return false;return youtube?youtube_.configure(id->text(),secret->text()):twitch_.configure(id->text());};
            connect(save,&QPushButton::clicked,page,[persist]{persist();});connect(login,&QPushButton::clicked,page,[this,youtube,persist]{if(persist()){if(youtube)youtube_.signIn();else twitch_.signIn();}});
            connect(instructions,&QPushButton::clicked,page,[youtube]{QDesktopServices::openUrl(QUrl(QString("https://github.com/DIMI0007/stream-chat-obs/blob/main/docs/REGISTER_APPS_RU.md#")+(youtube?"youtube":"twitch")));});
        };
        addPersonalAccount(false);addPersonalAccount(true);
        auto* kickCard=new QWidget;kickCard->setObjectName("LanguageCard");outer->addWidget(kickCard);
        auto* kickLayout=new QVBoxLayout(kickCard);kickLayout->setContentsMargins(8,8,8,8);kickLayout->setSpacing(6);
        auto* enableKick=new QCheckBox;enableKick->setObjectName("EnableKick");enableKick->setChecked(kickEnabled_);kickLayout->addWidget(enableKick);
        auto* account=new QWidget;account->setObjectName("KickAccountSettings");auto* accountLayout=new QVBoxLayout(account);accountLayout->setContentsMargins(0,0,0,0);accountLayout->setSpacing(6);kickLayout->addWidget(account);account->setVisible(kickEnabled_);
        auto addField=[accountLayout](const QString& name,QLineEdit* field){auto* label=new QLabel(name);label->setWordWrap(true);accountLayout->addWidget(label);accountLayout->addWidget(field);return label;};
        auto* channel=new QLineEdit(kick_.channel());channel->setMaxLength(100);channel->setObjectName("KickChannel");auto* channelLabel=addField({},channel);
        auto* clientId=new QLineEdit(kick_.clientId());clientId->setMaxLength(200);clientId->setObjectName("KickClientId");addField("Client ID",clientId);
        auto* secret=new QLineEdit(kick_.clientSecret());secret->setMaxLength(500);secret->setEchoMode(QLineEdit::Password);secret->setObjectName("KickClientSecret");addField("Client Secret",secret);
        auto* storageHint=new QLabel;storageHint->setObjectName("Hint");storageHint->setWordWrap(true);storageHint->setStyleSheet("font-weight:400;color:#8ea1c0;");accountLayout->addWidget(storageHint);
        auto* callbackLabel=new QLabel;callbackLabel->setWordWrap(true);accountLayout->addWidget(callbackLabel);
        auto* callback=new QLineEdit(sc::KickChat::callback());callback->setReadOnly(true);callback->setObjectName("KickCallback");accountLayout->addWidget(callback);
        auto* copy=new QPushButton;accountLayout->addWidget(copy);connect(copy,&QPushButton::clicked,page,[]{QGuiApplication::clipboard()->setText(sc::KickChat::callback());});
        auto* save=new QPushButton;accountLayout->addWidget(save);
        auto* signIn=new QPushButton;accountLayout->addWidget(signIn);
        auto* signOut=new QPushButton;accountLayout->addWidget(signOut);
        auto* accountStatus=new QLabel;accountStatus->setObjectName("Hint");accountStatus->setWordWrap(true);accountStatus->setStyleSheet("font-weight:400;color:#8ea1c0;");accountLayout->addWidget(accountStatus);
        auto* guideToggle=new QPushButton;guideToggle->setObjectName("KickGuideToggle");guideToggle->setCheckable(true);accountLayout->addWidget(guideToggle);
        auto* guide=new QLabel;guide->setObjectName("KickSetupGuide");guide->setWordWrap(true);guide->setTextFormat(Qt::PlainText);guide->setTextInteractionFlags(Qt::TextSelectableByMouse);guide->setStyleSheet("font-weight:400;color:#b8c7df;");guide->hide();accountLayout->addWidget(guide);
        auto* developerPage=new QPushButton;accountLayout->addWidget(developerPage);
        auto* githubGuide=new QPushButton;accountLayout->addWidget(githubGuide);githubGuide->setText(english_?"Connection guide on GitHub":"Инструкция подключения в GitHub");
        connect(githubGuide,&QPushButton::clicked,page,[]{QDesktopServices::openUrl(QUrl("https://github.com/DIMI0007/stream-chat-obs/blob/main/docs/REGISTER_APPS_RU.md#kick"));});
        for(auto* choice:*choices)connect(choice,&QPushButton::clicked,page,[this,githubGuide]{githubGuide->setText(english_?"Connection guide on GitHub":"Инструкция подключения в GitHub");});
        connect(developerPage,&QPushButton::clicked,page,[]{QDesktopServices::openUrl(QUrl("https://kick.com/settings/developer"));});
        connect(guideToggle,&QPushButton::toggled,guide,&QWidget::setVisible);
        auto updateKickSettings=[this,enableKick,channel,channelLabel,storageHint,callbackLabel,copy,save,signIn,signOut,accountStatus,guideToggle,guide,developerPage]{
            enableKick->setText(english_?"Enable Kick":"Включить Kick");
            channelLabel->setText(english_?"Channel name (without a link)":"Имя канала (без ссылки)");channel->setPlaceholderText("otmopozok1101");
            storageHint->setText(english_?"Channel name is enough to read chat. Sign in to send. Keys and tokens are encrypted for your Windows account.":"Для чтения достаточно имени канала. Для отправки нужен вход. Ключи и вход сохраняются в зашифрованном виде для вашей учётной записи Windows.");
            callbackLabel->setText(english_?"Redirect URL — copy exactly":"Redirect URL — скопируйте без изменений");copy->setText(english_?"Copy Redirect URL":"Скопировать Redirect URL");
            save->setText(english_?"Save and connect chat":"Сохранить и подключить чат");signIn->setText(english_?"Sign in to Kick":"Войти в Kick");signOut->setText(english_?"Sign out on this PC":"Выйти на этом ПК");
            signIn->setEnabled(!kick_.busy());signOut->setVisible(kick_.signedIn());accountStatus->setText(kick_.status(english_));
            guideToggle->setText(english_?"How to connect Kick":"Как подключить Kick");developerPage->setText(english_?"Open Kick developer settings":"Открыть приложения Kick");
            guide->setText(english_?
                "1. Open Kick developer settings and choose Create application. Enable two-factor authentication if Kick asks.\n\n2. Use a name without spaces, such as StreamChatOBSPersonal. Add a short description.\n\n3. Paste the Redirect URL shown above. Leave webhooks disabled.\n\n4. Select Read channel information (channel:read) and Write to chat (chat:write). Kick also requires Read user information (user:read), including email. Leave other permissions off.\n\n5. Create the application after reviewing Kick's developer terms. Copy Client ID and Client Secret into these fields. The secret is not your stream key.\n\n6. Enter your channel name, save, and choose Sign in to Kick. Confirm access on Kick in your browser.\n\nReading uses Kick's website chat connection, which may change. Sending uses the official Kick API. Disabling Kick stops its connections and keeps your saved account.":
                "1. Откройте приложения Kick и нажмите «Создать приложение». Включите двухфакторную защиту, если Kick попросит.\n\n2. Название — без пробелов, например StreamChatOBSPersonal. Добавьте короткое описание.\n\n3. Вставьте Redirect URL из поля выше. Webhooks оставьте выключенными.\n\n4. Выберите чтение канала (channel:read) и отправку в чат (chat:write). Kick также требует чтение профиля (user:read), включая email. Остальные права не нужны.\n\n5. Ознакомьтесь с условиями разработчика Kick и создайте приложение. Скопируйте Client ID и Client Secret в эти поля. Secret — это не ключ трансляции.\n\n6. Введите имя своего канала, сохраните и нажмите «Войти в Kick». Подтвердите доступ в браузере на сайте Kick.\n\nЧтение использует соединение чата сайта Kick, которое может измениться. Отправка работает через официальный API Kick. Выключение Kick останавливает подключения и сохраняет аккаунт.");
        };
        updateKickSettings();for(auto* choice:*choices)connect(choice,&QPushButton::clicked,page,updateKickSettings);
        connect(&kick_,&sc::KickChat::changed,page,updateKickSettings);
        connect(enableKick,&QCheckBox::toggled,page,[this,account](bool enabled){kickEnabled_=enabled;QSettings saved(configPath("ui.ini"),QSettings::IniFormat);saved.setValue("kickEnabled",enabled);saved.sync();account->setVisible(enabled);kick_.setEnabled(enabled);refresh();});
        auto saveKick=[this,channel,clientId,secret]{return kick_.configure(clientId->text(),secret->text(),channel->text());};
        connect(save,&QPushButton::clicked,page,[saveKick]{saveKick();});
        connect(signIn,&QPushButton::clicked,page,[this,saveKick]{if(saveKick())kick_.signIn();});
        connect(signOut,&QPushButton::clicked,&kick_,&sc::KickChat::signOut);
        outer->addStretch();
    }
    void applyLanguage(){
        settings_->setToolTip(english_?"Settings":"Настройки");settings_->setAccessibleName(settings_->toolTip());
        login_->setText(english_?"Sign in":"Войти");googleLogin_->setText(login_->text());
        logout_->setToolTip(english_?"Disconnect Twitch":"Отключить Twitch");googleLogout_->setToolTip(english_?"Disconnect YouTube":"Отключить YouTube");
        chatTitle_->setText(english_?"Chat":"Чат");recipientHint_->setText(english_?"Send to":"Куда отправлять");
        scrollBottom_->setToolTip(english_?"Jump to new messages":"К новым сообщениям");
        messages_->setPlaceholderText(english_?"Messages appear here. History is kept until OBS closes.":"Здесь появятся сообщения. История хранится только до закрытия OBS.");
        input_->setPlaceholderText(english_?"Write to chat…":"Написать в чат…");send_->setText(english_?"Send":"Отправить");
        totalCount_->setToolTip(english_?"Messages during this OBS session":"Всего сообщений за этот запуск OBS");twitchCount_->setToolTip(english_?"Twitch messages during this OBS session":"Сообщения Twitch за этот запуск OBS");youtubeCount_->setToolTip(english_?"YouTube messages during this OBS session":"Сообщения YouTube за этот запуск OBS");
        target_->setToolTip(english_?"Send to Twitch":"Отправлять в Twitch");target_->setAccessibleName(target_->toolTip());youtubeTarget_->setToolTip(english_?"Send to YouTube":"Отправлять в YouTube");youtubeTarget_->setAccessibleName(youtubeTarget_->toolTip());
        kickTarget_->setAccessibleName(english_?"Send to Kick":"Отправлять в Kick");kickCount_->setToolTip(english_?"Kick messages during this OBS session":"Сообщения Kick за этот запуск OBS");
        if(broadcast_->count())broadcast_->setItemText(0,english_?"Choose stream":"Выберите эфир");
        refresh();updateLastMessage();updateMetrics();
    }
    static QStringList namesFrom(const QString& raw){
        auto names=raw.split(QRegularExpression("[,;\\n]"),Qt::SkipEmptyParts);for(auto& name:names){name=name.trimmed();if(name.startsWith('@'))name.remove(0,1);}names.removeAll(QString());return names;
    }
    bool filteredBot(const sc::ChatMessage& message)const{
        if(!hideBots_)return false;for(const auto& name:namesFrom(botNames_))if(name.compare(message.author,Qt::CaseInsensitive)==0)return true;return false;
    }
    bool mentioned(const QString& text)const{
        if(!highlightMentions_)return false;auto names=namesFrom(mentionNames_);if(!twitch_.accountLogin().isEmpty())names.append(twitch_.accountLogin());if(kickEnabled_&&!kick_.accountLogin().isEmpty())names.append(kick_.accountLogin());
        for(const auto& name:names){QRegularExpression match("(?<![\\p{L}\\p{N}_])@?"+QRegularExpression::escape(name)+"(?![\\p{L}\\p{N}_])",QRegularExpression::CaseInsensitiveOption|QRegularExpression::UseUnicodePropertiesOption);if(match.match(text).hasMatch())return true;}return false;
    }
    void updateLastMessage(){
        if(!lastMessage_)return;
        const auto age=lastReceived_.isValid()?std::max<qint64>(0,lastReceived_.secsTo(QDateTime::currentDateTimeUtc())):-1;
        lastMessage_->setText(age<0?(english_?"No messages received this session":"За этот запуск сообщений ещё нет"):
            (english_?"Last message: ":"Последнее сообщение: ")+(age<60?QString::number(age)+(english_?" sec ago":" сек. назад"):QString::number(age/60)+(english_?" min ago":" мин. назад")));
        lastMessage_->setToolTip(english_?"Time since a message arrived; silence alone does not mean a connection failure":"Время с получения сообщения. Тишина в чате сама по себе не означает обрыв связи");
    }
    void requestEmote(const QString& id){
        if(id.isEmpty()||emotes_.contains(id)||pendingEmotes_.contains(id)||failedEmotes_.contains(id)||emotes_.size()+pendingEmotes_.size()+failedEmotes_.size()>=64)return;
        const bool ki=id.startsWith("kick:");if(ki&&!kickEnabled_)return;const auto n=ki?id.mid(5):id;if(!QRegularExpression("^[A-Za-z0-9_-]{1,100}$").match(n).hasMatch())return;
        QNetworkRequest request(QUrl(ki?"https://files.kick.com/emotes/"+n+"/fullsize":"https://static-cdn.jtvnw.net/emoticons/v2/"+id+"/static/dark/1.0"));
        request.setTransferTimeout(10000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
        request.setRawHeader("Accept","image/png");pendingEmotes_.insert(id);
        auto* reply=emoteHttp_.get(request);
        connect(reply,&QNetworkReply::readyRead,reply,[reply]{if(reply->bytesAvailable()>65536)reply->abort();});
        connect(reply,&QNetworkReply::finished,this,[this,reply,id]{
            sc::CpuScope cpu;pendingEmotes_.remove(id);
            const auto bytes=reply->readAll();sc::traffic::addReceived(bytes.size());
            const bool ok=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200&&bytes.size()<=65536;
            QPixmap image;if(ok)image.loadFromData(bytes);reply->deleteLater();
            if(image.isNull()||image.width()>128||image.height()>128){failedEmotes_.insert(id);return;}
            emotes_.insert(id,image.scaled(20,20,Qt::KeepAspectRatio,Qt::SmoothTransformation));
            emotesDirty_=true;if(isVisible()&&!emoteRedraw_.isActive())emoteRedraw_.start();
        });
    }
    void updateMetrics(){
        quint64 bytes=quint64(messages_->document()->characterCount())*sizeof(QChar);
        auto add=[&bytes](const sc::ChatMessage& m){for(const auto* s:{&m.platform,&m.id,&m.authorId,&m.author,&m.text})bytes+=quint64(s->capacity())*sizeof(QChar);for(const auto& part:m.fragments){bytes+=quint64(part.text.capacity()+part.emoteId.capacity())*sizeof(QChar);}};
        for(const auto& m:history_)add(m);for(const auto& m:pending_)add(m);
        for(auto it=emotes_.cbegin();it!=emotes_.cend();++it)bytes+=quint64(it.value().width())*quint64(it.value().height())*4;
        sc::measuredDataBytes.store(bytes,std::memory_order_relaxed);
    }
    bool atBottom()const{const auto* bar=messages_->verticalScrollBar();return bar->value()>=bar->maximum()-3;}
    void updateScrollButton(){scrollBottom_->setVisible(!atBottom()&&unread_>0);scrollBottom_->setText(unread_>0?"↓ "+QString::number(unread_):"↓");}
    void redrawMessages(){
        auto* bar=messages_->verticalScrollBar();const bool bottom=atBottom();const int oldPosition=bar->value();
        redrawing_=true;messages_->clear();
        messages_->document()->addResource(QTextDocument::ImageResource,QUrl("stream-chat://icon/twitch"),messagePlatformIcon("Twitch"));
        messages_->document()->addResource(QTextDocument::ImageResource,QUrl("stream-chat://icon/youtube"),messagePlatformIcon("YouTube"));messages_->document()->addResource(QTextDocument::ImageResource,QUrl("stream-chat://icon/kick"),messagePlatformIcon("Kick"));
        for(const auto& message:history_)appendMessage(message,false);
        bar->setValue(bottom?bar->maximum():oldPosition);redrawing_=false;emotesDirty_=false;updateScrollButton();
    }
    void appendMessage(const sc::ChatMessage& message,bool fresh=true){
        if(filteredBot(message))return;
        auto* bar=messages_->verticalScrollBar();const bool bottom=atBottom();const int oldPosition=bar->value();
        const bool wasRedrawing=redrawing_;redrawing_=true;
        auto text=message.text,author=message.author;text.replace('\n',' ');text.replace('\r',' ');author.replace('\n',' ');author.replace('\r',' ');
        QTextCursor cursor(messages_->document());cursor.movePosition(QTextCursor::End);if(!messages_->document()->isEmpty())cursor.insertBlock();
        QTextBlockFormat block;block.setBackground(mentioned(message.text)?QColor("#193b34"):QColor(Qt::transparent));cursor.setBlockFormat(block);
        QTextImageFormat icon;icon.setName(message.platform=="Kick"?"stream-chat://icon/kick":message.platform=="YouTube"?"stream-chat://icon/youtube":"stream-chat://icon/twitch");icon.setWidth(14);icon.setHeight(14);cursor.insertImage(icon);cursor.insertText(" ");
        const bool youtube=message.platform=="YouTube";const QColor accent=message.platform=="Kick"?QColor("#8be965"):youtube?QColor("#ff8491"):QColor("#b995ff");const QColor tint=message.platform=="Kick"?QColor("#16311c"):youtube?QColor("#32181d"):QColor("#241b35");
        QTextCharFormat authorStyle;authorStyle.setForeground(accent);authorStyle.setBackground(tint);authorStyle.setFontWeight(QFont::DemiBold);cursor.insertText(" "+author+" ",authorStyle);
        if(message.sentAt.isValid()){QTextCharFormat timeStyle;timeStyle.setForeground(QColor("#8ea1c0"));cursor.insertText(" "+message.sentAt.toLocalTime().toString("HH:mm"),timeStyle);}
        QTextCharFormat messageStyle;messageStyle.setForeground(QColor("#dfe8ff"));cursor.insertText("  ",messageStyle);
        if((message.platform=="Twitch"||message.platform=="Kick")&&!message.fragments.isEmpty()){
            for(const auto& fragment:message.fragments){
                if(!fragment.emoteId.isEmpty()&&emotes_.contains(fragment.emoteId)){
                    const auto resource=QUrl("stream-chat://emote/"+fragment.emoteId);const auto& picture=emotes_[fragment.emoteId];
                    messages_->document()->addResource(QTextDocument::ImageResource,resource,picture);
                    QTextImageFormat emote;emote.setName(resource.toString());emote.setWidth(picture.width());emote.setHeight(picture.height());cursor.insertImage(emote);
                }else{
                    auto part=fragment.text;part.replace('\n',' ');part.replace('\r',' ');cursor.insertText(part,messageStyle);
                    if(!fragment.emoteId.isEmpty())requestEmote(fragment.emoteId);
                }
            }
        }else cursor.insertText(text,messageStyle);
        if(bottom)bar->setValue(bar->maximum());else{bar->setValue(oldPosition);if(fresh)++unread_;}
        redrawing_=wasRedrawing;
        if(!redrawing_)updateScrollButton();
    }
    void refresh(){
        paintStatus(twitchStatus_,twitch_.status(),"Twitch",twitch_.ready(),english_);paintStatus(youtubeStatus_,youtube_.status(),"YouTube",youtube_.ready(),english_);
        if(youtube_.signedIn()&&!youtube_.ready()&&(youtube_.status().contains("начала эфира")||youtube_.status().contains("не найден"))){
            youtubeStatus_->setText(english_?"● Account linked · waiting for live chat":"● Вход выполнен · ждём чат эфира");
            youtubeStatus_->setToolTip(english_?"Account linked. Messages will appear after an active live chat is found.":"Аккаунт подключён. Сообщения появятся после обнаружения активного чата трансляции.");
        }
        const QLocale ru(english_?QLocale::English:QLocale::Russian);
        twitchViewers_->setVisible(streaming_&&twitch_.viewerCount()>=0);
        youtubeViewers_->setVisible(streaming_&&youtube_.viewerCount()>=0);
        if(twitch_.viewerCount()>=0){twitchViewers_->setText("👥 "+ru.toString(twitch_.viewerCount()));twitchViewers_->setStyleSheet(QString("color:%1;font-size:15px;font-weight:700;").arg(twitch_.viewerFresh()?"#dfe8ff":"#8ea1c0"));twitchViewers_->setToolTip(twitch_.viewerFresh()?(english_?"Twitch viewers now":"Смотрят Twitch сейчас"):(english_?"Last known Twitch viewer count":"Последнее известное число зрителей Twitch"));}
        if(youtube_.viewerCount()>=0){youtubeViewers_->setText("👥 "+ru.toString(youtube_.viewerCount()));youtubeViewers_->setStyleSheet(QString("color:%1;font-size:15px;font-weight:700;").arg(youtube_.viewerFresh()?"#dfe8ff":"#8ea1c0"));youtubeViewers_->setToolTip(youtube_.viewerFresh()?(english_?"YouTube viewers now":"Смотрят YouTube сейчас"):(english_?"Last known YouTube viewer count":"Последнее известное число зрителей YouTube"));}
        kickViewers_->setVisible(kickEnabled_&&streaming_&&kick_.viewerCount()>=0);
        if(kick_.viewerCount()>=0){kickViewers_->setText("👥 "+ru.toString(kick_.viewerCount()));kickViewers_->setStyleSheet(QString("color:%1;font-size:15px;font-weight:700;").arg(kick_.viewerFresh()?"#dfe8ff":"#8ea1c0"));kickViewers_->setToolTip(kick_.viewerFresh()?(english_?"Kick viewers now":"Смотрят Kick сейчас"):(english_?"Last known Kick viewer count":"Последнее известное число зрителей Kick"));}
        recipientPairs_.last()->setVisible(kickEnabled_);kickCard_->setVisible(kickEnabled_);kickTarget_->setVisible(kickEnabled_);paintStatus(kickStatus_,kick_.status(),"Kick",kick_.ready(),english_);
        kickStatus_->setToolTip(kick_.status(english_));if(kick_.ready())kickStatus_->setText(kick_.canSend()?(english_?"● Connected":"● Подключён"):(english_?"● Read only":"● Только чтение"));
        kickTarget_->setToolTip(kick_.canSend()?(english_?"Send to Kick":"Отправлять в Kick"):(english_?"Sign in to Kick in Settings to send":"Для отправки войдите в Kick в настройках"));
        kickLogin_->setText(english_?"Settings":"Настроить");kickLogin_->setVisible(!kick_.signedIn());kickLogout_->setVisible(kick_.signedIn());kickLogout_->setToolTip(english_?"Sign out of Kick":"Выйти из Kick");
        const bool showCounts=streaming_&&seenThisStream_;
        totalCount_->setVisible(showCounts&&(twitchMessages_+youtubeMessages_+kickMessages_>0));
        twitchCount_->setVisible(showCounts&&twitchMessages_>0);youtubeCount_->setVisible(showCounts&&youtubeMessages_>0);kickCount_->setVisible(kickEnabled_&&showCounts&&kickMessages_>0);kickCount_->setText(QString::number(kickMessages_));
        if(showCounts){totalCount_->setText("✉ "+QString::number(twitchMessages_+youtubeMessages_+kickMessages_));twitchCount_->setText(QString::number(twitchMessages_));youtubeCount_->setText(QString::number(youtubeMessages_));}
        const bool twitchBusy=code_->isVisible()&&twitch_.status().contains("подтвердите",Qt::CaseInsensitive);if(code_->isVisible()&&!twitchBusy&&!twitch_.ready())code_->hide();
        login_->setVisible(!twitch_.ready());logout_->setVisible(twitch_.ready());login_->setEnabled(!twitchBusy);
        login_->setText(twitch_.configured()?(english_?"Sign in":"Войти"):(english_?"Set up":"Настроить"));googleLogin_->setText(youtube_.configured()?(english_?"Sign in":"Войти"):(english_?"Set up":"Настроить"));
        googleLogin_->setVisible(!youtube_.signedIn());googleLogout_->setVisible(youtube_.signedIn());
        const bool youtubeBusy=youtube_.status().contains("подтвердите",Qt::CaseInsensitive)||youtube_.status().contains("завершаем",Qt::CaseInsensitive);googleLogin_->setEnabled(!youtubeBusy);broadcast_->setVisible(youtube_.signedIn()&&broadcast_->count()>2);
        delivery_->setVisible(!delivery_->text().isEmpty());
        const bool destinations=(target_->isChecked()&&twitch_.ready())||(youtubeTarget_->isChecked()&&youtube_.ready())||(kickEnabled_&&kickTarget_->isChecked()&&kick_.canSend());
        input_->setEnabled((twitch_.ready()||youtube_.ready()||kick_.canSend())&&!activeSends_);
        send_->setEnabled(destinations&&!activeSends_&&!input_->text().trimmed().isEmpty());
        target_->setEnabled(!activeSends_);youtubeTarget_->setEnabled(!activeSends_);kickTarget_->setEnabled(!activeSends_&&kick_.canSend());layoutRecipients();
    }
    void send(){
        if(activeSends_||input_->text().trimmed().isEmpty())return;
        const bool tw=target_->isChecked()&&twitch_.ready(),yt=youtubeTarget_->isChecked()&&youtube_.ready(),ki=kickEnabled_&&kickTarget_->isChecked()&&kick_.canSend();
        if(!tw&&!yt&&!ki)return;
        if(ki&&!sc::kick_wire::validMessage(input_->text())){delivery_->setText(english_?"Kick: maximum 500 characters and 2048 UTF-8 bytes":"Kick: предел — 500 символов и 2048 байт UTF-8");delivery_->show();return;}
        if(yt&&input_->text().size()>200){delivery_->setText(english_?"YouTube messages are limited to 200 characters.":"Для YouTube сократите сообщение до 200 символов.");return;}
        activeSends_=int(tw)+int(yt)+int(ki);allSent_=true;twitchSent_=false;youtubeSent_=false;kickSent_=false;results_.clear();
        delivery_->setText(english_?"Sending to selected channels…":"Отправляем в выбранные каналы…");refresh();const auto text=input_->text();
        if(tw)twitch_.send(text);if(yt)youtube_.send(text);if(ki)kick_.send(text);
    }
    void completed(const QString& platform,bool confirmed,const QString& reason){
        if(!activeSends_)return;allSent_=allSent_&&confirmed;results_.append(english_?(platform+(confirmed?": Message sent":": Delivery unconfirmed; no automatic retry")):reason);
        if(platform=="Twitch")twitchSent_=confirmed;else if(platform=="YouTube")youtubeSent_=confirmed;else if(platform=="Kick")kickSent_=confirmed;
        --activeSends_;if(!activeSends_){if(allSent_)input_->clear();else {if(twitchSent_)target_->setChecked(false);if(youtubeSent_)youtubeTarget_->setChecked(false);if(kickSent_)kickTarget_->setChecked(false);}}
        delivery_->setText(results_.join('\n'));refresh();
    }
    void flush(){sc::CpuScope cpu;int count=0;while(!pending_.isEmpty()&&count++<50){const auto message=pending_.dequeue();lastReceived_=QDateTime::currentDateTimeUtc();history_.enqueue(message);while(history_.size()>500)history_.dequeue();appendMessage(message);if(streaming_){if(message.platform=="Twitch")++twitchMessages_;else if(message.platform=="YouTube")++youtubeMessages_;else if(message.platform=="Kick")++kickMessages_;seenThisStream_=true;}}refresh();if(!pending_.isEmpty())batch_.start();}
    template<class Predicate>void remove(Predicate predicate){
        history_.erase(std::remove_if(history_.begin(),history_.end(),predicate),history_.end());
        pending_.erase(std::remove_if(pending_.begin(),pending_.end(),predicate),pending_.end());
        redrawMessages();
    }
    sc::TwitchChat twitch_;sc::YouTubeChat youtube_;sc::KickChat kick_;
    QWidget* recipientGroups_=nullptr;QVBoxLayout* recipientLayout_=nullptr;QVector<QWidget*> recipientPairs_;QString recipientLayoutKey_;
    QWidget* kickCard_=nullptr;QLabel* kickViewers_=nullptr;QLabel* kickStatus_=nullptr;QLabel* kickCount_=nullptr;QAbstractButton* kickTarget_=nullptr;QPushButton* kickLogin_=nullptr;QPushButton* kickLogout_=nullptr;bool kickEnabled_=false,kickSent_=false;quint64 kickMessages_=0;QStackedLayout* pages_=nullptr;WrappedChatEdit* messages_=nullptr;QLabel* twitchStatus_=nullptr;QLabel* youtubeStatus_=nullptr;QLabel* twitchViewers_=nullptr;QLabel* youtubeViewers_=nullptr;QLabel* code_=nullptr;QLabel* delivery_=nullptr;QLabel* totalCount_=nullptr;QLabel* twitchCount_=nullptr;QLabel* youtubeCount_=nullptr;QLabel* traffic_=nullptr;QLabel* cpuValue_=nullptr;QLabel* memoryValue_=nullptr;QLabel* chatTitle_=nullptr;QLabel* recipientHint_=nullptr;QLabel* usageTitle_=nullptr;QLabel* memoryName_=nullptr;QWidget* usageCard_=nullptr;QPushButton* settings_=nullptr;QPushButton* scrollBottom_=nullptr;
    QLineEdit* input_=nullptr;QAbstractButton* target_=nullptr;QPushButton* send_=nullptr;QPushButton* login_=nullptr;QPushButton* logout_=nullptr;
    QAbstractButton* youtubeTarget_=nullptr;QComboBox* broadcast_=nullptr;QPushButton* googleLogin_=nullptr;QPushButton* googleLogout_=nullptr;
    QLabel* lastMessage_=nullptr;QDateTime lastReceived_;QString mentionNames_,botNames_;bool highlightMentions_=true,hideBots_=false;QQueue<sc::ChatMessage> history_,pending_;QTimer batch_,trafficTimer_,emoteRedraw_;QElapsedTimer usageClock_;QNetworkAccessManager emoteHttp_;QHash<QString,QPixmap> emotes_;QSet<QString> pendingEmotes_,failedEmotes_;std::uint64_t lastCpuNs_=0;int activeSends_=0,unread_=0,textScale_=100;bool allSent_=false,twitchSent_=false,youtubeSent_=false;bool streaming_=false,seenThisStream_=false,english_=false,redrawing_=false,emotesDirty_=false;QString language_="auto";quint64 twitchMessages_=0,youtubeMessages_=0;QStringList results_;
};
#ifndef STREAM_CHAT_PREVIEW
QPointer<ChatDock> dock;QString tlsPath;
void saveDockVisibility(){
    auto* window=static_cast<QMainWindow*>(obs_frontend_get_main_window());
    auto* widget=window?window->findChild<QDockWidget*>(DockId):nullptr;
    if(!widget)return;
    QSettings state(configPath("dock.ini"),QSettings::IniFormat);state.setValue("visible",widget->toggleViewAction()->isChecked());state.sync();
}
void createDock(){
    if(dock)return;
    auto* widget=new ChatDock;
    if(!obs_frontend_add_dock_by_id(DockId,"Stream Chat",widget)){delete widget;return;}
    dock=widget;
    auto* window=static_cast<QMainWindow*>(obs_frontend_get_main_window());
    auto* panel=window?window->findChild<QDockWidget*>(DockId):nullptr;
    if(!panel)return;
    QTimer::singleShot(0,panel,[window,panel]{
        QSettings state(configPath("dock.ini"),QSettings::IniFormat);
        if(!state.contains("visible")){panel->setFloating(false);window->addDockWidget(Qt::RightDockWidgetArea,panel);}
        panel->setVisible(state.value("visible",true).toBool());
        QObject::connect(panel->toggleViewAction(),&QAction::toggled,panel,[window](bool visible){
            if(!window->isVisible()||window->isMinimized())return;
            QSettings saved(configPath("dock.ini"),QSettings::IniFormat);saved.setValue("visible",visible);saved.sync();
        });
    });
}
void onFrontendEvent(enum obs_frontend_event event,void*){
    if(event==OBS_FRONTEND_EVENT_FINISHED_LOADING){createDock();return;}
    if(event==OBS_FRONTEND_EVENT_EXIT){saveDockVisibility();return;}
    if(dock&&(event==OBS_FRONTEND_EVENT_STREAMING_STARTED||event==OBS_FRONTEND_EVENT_STREAMING_STOPPED)){
        const QPointer<ChatDock> current=dock;
        QMetaObject::invokeMethod(current.data(),[current]{if(current)current->setStreaming(anyStreamingOutput());},Qt::QueuedConnection);
    }
}
#endif
}
#ifndef STREAM_CHAT_PREVIEW
bool obs_module_load(void){
    char* path=obs_module_file("qt-plugins");if(path){tlsPath=QString::fromUtf8(path);bfree(path);QCoreApplication::addLibraryPath(tlsPath);}
    if(!QSslSocket::supportsSsl()&&QSslSocket::availableBackends().contains("schannel"))QSslSocket::setActiveBackend("schannel");
    obs_frontend_add_event_callback(onFrontendEvent,nullptr);blog(LOG_INFO,"Stream Chat OBS %s loaded; HTTPS: %s",STREAM_CHAT_VERSION,QSslSocket::supportsSsl()?"available":"unavailable");return true;
}
void obs_module_unload(void){obs_frontend_remove_event_callback(onFrontendEvent,nullptr);if(dock)obs_frontend_remove_dock(DockId);dock=nullptr;if(!tlsPath.isEmpty())QCoreApplication::removeLibraryPath(tlsPath);tlsPath.clear();}
#endif
