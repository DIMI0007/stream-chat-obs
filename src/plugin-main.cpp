#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>
#include "twitch_chat.hpp"
#include "youtube_chat.hpp"
#include "traffic.hpp"
#include "usage.hpp"
#include <QComboBox>
#include <QSignalBlocker>
#include <QStringList>
#include <QAbstractButton>
#include <QSizePolicy>
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
#include <QRegularExpression>
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
QPushButton#Settings { color:#e1ebff; background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #202a3b,stop:1 #141d2b); border:1px solid #39465d; border-radius:8px; min-width:30px; max-width:30px; min-height:28px; max-height:28px; padding:0; }
QPushButton#Settings:hover { background:#26344d; border-color:#6476a0; }
QPushButton#ScrollBottom { color:#b7efff; background:#17283a; border:1px solid #34627b; border-radius:7px; min-height:18px; max-height:18px; padding:0 6px; font-size:10px; }
QPushButton#ScrollBottom:hover { color:#ffffff; background:#24405a; }
QPushButton#Credit { color:#9299a5; background:transparent; border:0; min-height:20px; max-height:20px; padding:0; font-size:11px; }
QPushButton#Credit:hover { color:#59dfff; text-decoration:underline; }
QLineEdit,QTextEdit,QComboBox { color:#e7eeff; background:#0c1420; border:1px solid #344865; border-radius:6px; padding:5px; selection-background-color:#316ac4; }
QLineEdit:focus,QTextEdit:focus,QComboBox:focus { border-color:#776af5; background:#111c2e; }
QComboBox QAbstractItemView { background:#101a2b; color:#e7eeff; selection-background-color:#26344d; }
QScrollBar:vertical { border:0; background:#101929; width:9px; }
QScrollBar::handle:vertical { background:#384d6c; border-radius:4px; min-height:24px; }
QScrollBar::handle:vertical:hover { background:#52698d; }
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; background:transparent; border:0; }
)";
QString scaledChatStyle(int percent){
    const QString source=QString::fromUtf8(ChatStyle);
    const QRegularExpression sizes("font-size:(\\d+)px");
    auto matches=sizes.globalMatch(source);QString result;int last=0;
    while(matches.hasNext()){
        const auto match=matches.next();result+=source.mid(last,match.capturedStart()-last);
        result+="font-size:"+QString::number(std::max(8,match.captured(1).toInt()*percent/100))+"px";
        last=match.capturedEnd();
    }
    return result+source.mid(last);
}
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
        const QColor brand=platform_=="YouTube"?QColor("#ff1538"):QColor("#9146ff");
        p.setPen(Qt::NoPen);p.setBrush(brand);p.drawRoundedRect(QRectF(0,0,32,32),9,9);p.setBrush(Qt::white);
        if(platform_=="YouTube"){
            p.drawRoundedRect(QRectF(5,8,22,16),5,5);p.setBrush(brand);QPainterPath play;play.moveTo(13,11);play.lineTo(22,16);play.lineTo(13,21);play.closeSubpath();p.drawPath(play);
        }else{
            QPainterPath bubble;bubble.moveTo(7,6);bubble.lineTo(26,6);bubble.lineTo(26,21);bubble.lineTo(20,27);bubble.lineTo(14,27);bubble.lineTo(10,30);bubble.lineTo(10,25);bubble.lineTo(7,25);bubble.closeSubpath();p.drawPath(bubble);p.setPen(QPen(brand,2));p.drawLine(15,11,15,19);p.drawLine(21,11,21,19);
        }
        if(hasFocus()){p.resetTransform();p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor("#8a79ff"),1,Qt::DotLine));p.drawRoundedRect(QRectF(0.5,0.5,29,17),5,5);}
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
QString compactStatus(QString value,const QString& platform){
    value.remove(platform+": ");
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
void paintStatus(QLabel* label,const QString& raw,const QString& platform,bool ready,bool english,int textScale){
    const QString value=compactStatus(raw,platform);QString color="#8ea1c0";
    if(ready)color="#00df8c";
    else if(value.contains("ошиб",Qt::CaseInsensitive)||value.contains("отклон",Qt::CaseInsensitive))color="#ff7f88";
    else if(value=="Подключение"||value.contains("подтверд",Qt::CaseInsensitive)||value.contains("заверша",Qt::CaseInsensitive)||value.contains("ищем",Qt::CaseInsensitive)||value.contains("эфир не найден",Qt::CaseInsensitive))color="#ffcd57";
    const QString shown=english?englishStatus(value):value;
    if(english){const auto http=raw.indexOf("(HTTP ");label->setToolTip(http<0?shown:shown+" "+raw.mid(http));}
    else label->setToolTip(raw);
    label->setText("● "+shown);label->setStyleSheet(QString("color:%1;font-size:%2px;").arg(color).arg(std::max(9,11*textScale/100)));
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
class ChatDock final : public QWidget {
public:
#ifdef STREAM_CHAT_PREVIEW
    void previewAppend(const sc::ChatMessage& message){history_.enqueue(message);appendMessage(message);}
#endif
    ChatDock():twitch_(configPath("twitch-account.dat"),this),youtube_(configPath("youtube-account.dat"),this){
        {QSettings saved(configPath("ui.ini"),QSettings::IniFormat);textScale_=std::clamp(saved.value("textScale",100).toInt(),80,140);}
        setObjectName("StreamChatRoot");setMinimumWidth(320);setStyleSheet(scaledChatStyle(textScale_));
        auto* layout=new QVBoxLayout(this);layout->setContentsMargins(8,8,8,8);layout->setSpacing(5);
        auto* header=new QHBoxLayout;header->setContentsMargins(2,2,2,2);header->setSpacing(7);header->addWidget(new PlatformBadge("Stream Chat"));auto* title=new QLabel("Stream Chat");title->setObjectName("Title");header->addWidget(title,1);settings_=new QPushButton;settings_->setObjectName("Settings");settings_->setIcon(settingsIcon());settings_->setIconSize(QSize(19,19));settings_->setFixedSize(30,28);settings_->setToolTip("Настройки");settings_->setAccessibleName("Настройки");header->addWidget(settings_);layout->addLayout(header);

        auto* twitchCard=card();auto* twitchLayout=new QVBoxLayout(twitchCard);twitchLayout->setContentsMargins(8,6,8,6);twitchLayout->setSpacing(5);
        auto* twitchHead=new QHBoxLayout;twitchHead->setSpacing(6);twitchHead->addWidget(new PlatformBadge("Twitch"));twitchHead->addWidget(sectionTitle("Twitch"));twitchViewers_=new QLabel;twitchViewers_->setObjectName("Hint");twitchViewers_->setToolTip("Смотрят Twitch сейчас");twitchHead->addWidget(twitchViewers_);twitchStatus_=new QLabel;twitchStatus_->setAlignment(Qt::AlignRight|Qt::AlignVCenter);twitchStatus_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);twitchHead->addWidget(twitchStatus_,1);login_=new QPushButton("Войти");twitchHead->addWidget(login_);logout_=new QPushButton("×");logout_->setObjectName("Disconnect");logout_->setToolTip("Отключить Twitch");twitchHead->addWidget(logout_);twitchLayout->addLayout(twitchHead);
        connect(login_,&QPushButton::clicked,&twitch_,&sc::TwitchChat::signIn);connect(logout_,&QPushButton::clicked,&twitch_,&sc::TwitchChat::signOut);
        code_=new QLabel;code_->setObjectName("Callout");code_->setTextFormat(Qt::PlainText);code_->setWordWrap(true);code_->setTextInteractionFlags(Qt::TextSelectableByMouse);code_->hide();twitchLayout->addWidget(code_);layout->addWidget(twitchCard);

        auto* youtubeCard=card();auto* youtubeLayout=new QVBoxLayout(youtubeCard);youtubeLayout->setContentsMargins(8,6,8,6);youtubeLayout->setSpacing(5);
        auto* youtubeHead=new QHBoxLayout;youtubeHead->setSpacing(6);youtubeHead->addWidget(new PlatformBadge("YouTube"));youtubeHead->addWidget(sectionTitle("YouTube"));youtubeViewers_=new QLabel;youtubeViewers_->setObjectName("Hint");youtubeViewers_->setToolTip("Смотрят YouTube сейчас");youtubeHead->addWidget(youtubeViewers_);youtubeStatus_=new QLabel;youtubeStatus_->setAlignment(Qt::AlignRight|Qt::AlignVCenter);youtubeStatus_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);youtubeHead->addWidget(youtubeStatus_,1);googleLogin_=new QPushButton("Войти");youtubeHead->addWidget(googleLogin_);googleLogout_=new QPushButton("×");googleLogout_->setObjectName("Disconnect");googleLogout_->setToolTip("Отключить YouTube");youtubeHead->addWidget(googleLogout_);youtubeLayout->addLayout(youtubeHead);
        connect(googleLogin_,&QPushButton::clicked,&youtube_,&sc::YouTubeChat::signIn);connect(googleLogout_,&QPushButton::clicked,&youtube_,&sc::YouTubeChat::signOut);
        broadcast_=new QComboBox;broadcast_->addItem("Выберите эфир",QString());youtubeLayout->addWidget(broadcast_);layout->addWidget(youtubeCard);
        connect(broadcast_,&QComboBox::currentIndexChanged,this,[this](int index){const auto id=broadcast_->itemData(index).toString();if(!id.isEmpty())youtube_.selectChat(id);});
        connect(&youtube_,&sc::YouTubeChat::broadcasts,this,[this](const QJsonArray& choices){
            const QSignalBlocker block(broadcast_);broadcast_->clear();broadcast_->addItem(english_?"Choose stream":"Выберите эфир",QString());
            for(const auto& value:choices){const auto choice=value.toObject();broadcast_->addItem(choice.value("title").toString(),choice.value("id").toString());}
            if(choices.size()==1)broadcast_->setCurrentIndex(1);
            refresh();
        });
        auto* chatCard=card();auto* chatLayout=new QVBoxLayout(chatCard);chatLayout->setContentsMargins(8,7,8,8);chatLayout->setSpacing(5);auto* chatHead=new QHBoxLayout;chatTitle_=sectionTitle("Чат");chatHead->addWidget(chatTitle_);chatHead->addStretch();scrollBottom_=new QPushButton("↓");scrollBottom_->setObjectName("ScrollBottom");scrollBottom_->setToolTip("К новым сообщениям");scrollBottom_->hide();chatHead->addWidget(scrollBottom_);totalCount_=new QLabel;totalCount_->setObjectName("Hint");totalCount_->setToolTip("Всего сообщений за этот запуск OBS");chatHead->addWidget(totalCount_);chatLayout->addLayout(chatHead);
        messages_=new QTextEdit;messages_->setReadOnly(true);messages_->setAcceptRichText(false);messages_->setFocusPolicy(Qt::NoFocus);messages_->setTextInteractionFlags(Qt::NoTextInteraction);messages_->setContextMenuPolicy(Qt::NoContextMenu);messages_->setCursor(Qt::ArrowCursor);messages_->document()->setMaximumBlockCount(500);messages_->setPlaceholderText("Здесь появятся сообщения. История хранится только до закрытия OBS.");
        messages_->document()->addResource(QTextDocument::ImageResource,QUrl("stream-chat://icon/twitch"),messagePlatformIcon("Twitch"));messages_->document()->addResource(QTextDocument::ImageResource,QUrl("stream-chat://icon/youtube"),messagePlatformIcon("YouTube"));chatLayout->addWidget(messages_,1);
        auto* targets=new QHBoxLayout;targets->setSpacing(6);recipientHint_=new QLabel("Куда отправлять");recipientHint_->setObjectName("Hint");targets->addWidget(recipientHint_);target_=new PlatformToggle("Twitch");target_->setChecked(true);targets->addWidget(target_);twitchCount_=new QLabel;twitchCount_->setObjectName("Hint");twitchCount_->setToolTip("Сообщения Twitch за этот запуск OBS");targets->addWidget(twitchCount_);youtubeTarget_=new PlatformToggle("YouTube");youtubeTarget_->setChecked(true);targets->addWidget(youtubeTarget_);youtubeCount_=new QLabel;youtubeCount_->setObjectName("Hint");youtubeCount_->setToolTip("Сообщения YouTube за этот запуск OBS");targets->addWidget(youtubeCount_);targets->addStretch();chatLayout->addLayout(targets);layout->addWidget(chatCard,1);
        auto* composerCard=card();auto* composerLayout=new QVBoxLayout(composerCard);composerLayout->setContentsMargins(8,7,8,8);composerLayout->setSpacing(5);auto* composer=new QHBoxLayout;input_=new QLineEdit;input_->setMaxLength(500);input_->setPlaceholderText("Сообщение на стриминговые сервисы");send_=new QPushButton("Отправить");send_->setObjectName("Primary");composer->addWidget(input_,1);composer->addWidget(send_);composerLayout->addLayout(composer);delivery_=new QLabel;delivery_->setObjectName("Hint");delivery_->setWordWrap(true);delivery_->setTextFormat(Qt::PlainText);delivery_->hide();composerLayout->addWidget(delivery_);layout->addWidget(composerCard);
        auto* usageCard=card();usageCard->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);auto* usageLayout=new QVBoxLayout(usageCard);usageLayout->setContentsMargins(8,5,8,5);usageLayout->setSpacing(2);usageTitle_=sectionTitle("Нагрузка плагина");usageLayout->addWidget(usageTitle_);auto* metrics=new QHBoxLayout;metrics->setSpacing(4);auto* cpuName=new QLabel("CPU");cpuName->setObjectName("Hint");metrics->addWidget(cpuName);cpuValue_=new QLabel("—");metrics->addWidget(cpuValue_);metrics->addStretch();memoryName_=new QLabel("Память данных");memoryName_->setObjectName("Hint");metrics->addWidget(memoryName_);memoryValue_=new QLabel("—");metrics->addWidget(memoryValue_);usageLayout->addLayout(metrics);usageCard_=usageCard;layout->addWidget(usageCard);
        auto* footer=new QVBoxLayout;footer->setSpacing(0);auto* version=new QPushButton("developed by OTMOPOZOK · v" STREAM_CHAT_VERSION);version->setObjectName("Credit");version->setCursor(Qt::PointingHandCursor);version->setToolTip("https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/");connect(version,&QPushButton::clicked,this,[]{QDesktopServices::openUrl(QUrl("https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/"));});footer->addWidget(version);traffic_=new QLabel;traffic_->setAlignment(Qt::AlignCenter);traffic_->setStyleSheet(QString("color:#72e9f4;font-size:%1px;").arg(std::max(8,10*textScale_/100)));traffic_->setToolTip("Трафик HTTP и WebSocket этого плагина с запуска OBS; без видео и сетевых заголовков");footer->addWidget(traffic_);layout->addLayout(footer);
        batch_.setInterval(20);batch_.setSingleShot(true);trafficTimer_.setInterval(2000);
        connect(&batch_,&QTimer::timeout,this,[this]{flush();});
        connect(scrollBottom_,&QPushButton::clicked,this,[this]{messages_->verticalScrollBar()->setValue(messages_->verticalScrollBar()->maximum());unread_=0;updateScrollButton();});
        connect(messages_->verticalScrollBar(),&QScrollBar::valueChanged,this,[this]{if(redrawing_)return;if(atBottom())unread_=0;updateScrollButton();});
        connect(&trafficTimer_,&QTimer::timeout,this,[this]{setStreaming(anyStreamingOutput());updateTraffic();updateMetrics();});
        connect(send_,&QPushButton::clicked,this,[this]{send();});connect(input_,&QLineEdit::returnPressed,this,[this]{send();});
        connect(target_,&QAbstractButton::toggled,this,[this]{refresh();});
        connect(youtubeTarget_,&QAbstractButton::toggled,this,[this]{refresh();});
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
        streaming_=anyStreamingOutput();twitch_.setStreamActive(streaming_);youtube_.setStreamActive(streaming_);
        refresh();updateTraffic();updateMetrics();
    }
    void setStreaming(bool active){if(streaming_==active)return;streaming_=active;seenThisStream_=false;twitch_.setStreamActive(active);youtube_.setStreamActive(active);refresh();}
protected:
    void showEvent(QShowEvent* event)override{QWidget::showEvent(event);setStreaming(anyStreamingOutput());twitch_.setVisible(true);youtube_.setVisible(true);QTimer::singleShot(0,this,[this]{if(isVisible()){twitch_.setVisible(true);youtube_.setVisible(true);refresh();}});trafficTimer_.start();updateTraffic();updateMetrics();if(!pending_.isEmpty())batch_.start();}
    void hideEvent(QHideEvent* event)override{twitch_.setVisible(false);youtube_.setVisible(false);batch_.stop();trafficTimer_.stop();if(activeSends_)delivery_->setText(english_?"Dock hidden: delivery unconfirmed. No automatic retry.":"Панель скрыта: результат отправки не подтверждён. Автоматического повтора нет.");activeSends_=0;refresh();QWidget::hideEvent(event);}
private:
    void openSettings(){
        QDialog dialog(this);dialog.setWindowTitle(english_?"Settings":"Настройки");dialog.setStyleSheet("QDialog{background:#080f18;color:#dfe8ff;} QWidget#LanguageCard{background:#0b131e;border:1px solid #263244;border-radius:8px;} QLabel{color:#dfe8ff;font-weight:700;} QPushButton{color:#dfe8ff;background:#172236;border:1px solid #354863;border-radius:7px;padding:5px 12px;} QPushButton#ActiveNav{background:#18234e;border-color:#7663ff;color:#ffffff;font-weight:700;}");
        auto* outer=new QVBoxLayout(&dialog);outer->setContentsMargins(5,5,5,5);auto* card=new QWidget(&dialog);card->setObjectName("LanguageCard");outer->addWidget(card);
        auto* layout=new QVBoxLayout(card);layout->setContentsMargins(8,6,8,8);layout->setSpacing(4);
        auto* label=new QLabel(english_?"Interface language":"Язык интерфейса");layout->addWidget(label);
        auto* line=new QHBoxLayout;line->setSpacing(4);
        const struct {const char* code;QString title;} languages[]={
            {"auto",english_?"Auto":"Авто"},{"ru",QString::fromUtf8("Русский")},{"en",QString("English")}
        };
        QVector<QPushButton*> choices;
        for(const auto& item:languages){
            auto* choice=new QPushButton(item.title);choice->setCheckable(true);choice->setChecked(language_==item.code);
            if(choice->isChecked())choice->setObjectName("ActiveNav");
            const QString code=QString::fromLatin1(item.code);
            connect(choice,&QPushButton::clicked,&dialog,[this,&dialog,label,&choices,code]{
                language_=code;english_=language_=="en"||(language_=="auto"&&obsLanguageIsEnglish());
                QSettings saved(configPath("ui.ini"),QSettings::IniFormat);saved.setValue("language",language_);saved.sync();applyLanguage();
                dialog.setWindowTitle(english_?"Settings":"Настройки");label->setText(english_?"Interface language":"Язык интерфейса");
                choices[0]->setText(english_?"Auto":"Авто");
                for(auto* button:choices){button->setChecked(button->property("localeCode").toString()==language_);button->setObjectName(button->isChecked()?"ActiveNav":"");button->style()->unpolish(button);button->style()->polish(button);}
            });
            choice->setProperty("localeCode",code);choices.push_back(choice);line->addWidget(choice,1);
        }
        layout->addLayout(line);
        auto* scaleCard=new QWidget(&dialog);scaleCard->setObjectName("LanguageCard");outer->addWidget(scaleCard);
        auto* scaleLayout=new QVBoxLayout(scaleCard);scaleLayout->setContentsMargins(8,6,8,8);scaleLayout->setSpacing(4);
        auto* scaleTitle=new QLabel(english_?"Text size":"Размер текста");scaleLayout->addWidget(scaleTitle);
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
        for(auto* choice:choices)connect(choice,&QPushButton::clicked,&dialog,[this,scaleTitle,updatePreview]{scaleTitle->setText(english_?"Text size":"Размер текста");updatePreview();});
        connect(slider,&QSlider::valueChanged,&dialog,[this,updatePreview](int value){
            textScale_=value;setStyleSheet(scaledChatStyle(textScale_));traffic_->setStyleSheet(QString("color:#72e9f4;font-size:%1px;").arg(std::max(8,10*textScale_/100)));redrawMessages();refresh();updatePreview();
            QSettings saved(configPath("ui.ini"),QSettings::IniFormat);saved.setValue("textScale",textScale_);saved.sync();
        });
        connect(&dialog,&QDialog::finished,&dialog,[this]{setStyleSheet(scaledChatStyle(textScale_));});
        dialog.setMinimumWidth(360);dialog.exec();
    }
    void applyLanguage(){
        settings_->setToolTip(english_?"Settings":"Настройки");settings_->setAccessibleName(settings_->toolTip());
        login_->setText(english_?"Sign in":"Войти");googleLogin_->setText(login_->text());
        logout_->setToolTip(english_?"Disconnect Twitch":"Отключить Twitch");googleLogout_->setToolTip(english_?"Disconnect YouTube":"Отключить YouTube");
        chatTitle_->setText(english_?"Chat":"Чат");recipientHint_->setText(english_?"Send to":"Куда отправлять");
        scrollBottom_->setToolTip(english_?"Jump to new messages":"К новым сообщениям");
        messages_->setPlaceholderText(english_?"Messages appear here. History is kept until OBS closes.":"Здесь появятся сообщения. История хранится только до закрытия OBS.");
        input_->setPlaceholderText(english_?"Message to streaming platforms":"Сообщение на стриминговые сервисы");send_->setText(english_?"Send":"Отправить");
        usageTitle_->setText(english_?"Plugin usage":"Нагрузка плагина");memoryName_->setText(english_?"Data memory":"Память данных");
        usageCard_->setToolTip(english_?"CPU: measured plugin work; memory: tracked messages and buffers, not all OBS memory":"CPU — измеренные участки работы плагина; память — учтённые сообщения и буферы, не вся память OBS");
        traffic_->setToolTip(english_?"Plugin HTTP and WebSocket traffic since OBS started; excludes video and network headers":"Трафик HTTP и WebSocket этого плагина с запуска OBS; без видео и сетевых заголовков");
        totalCount_->setToolTip(english_?"Messages during this OBS session":"Всего сообщений за этот запуск OBS");twitchCount_->setToolTip(english_?"Twitch messages during this OBS session":"Сообщения Twitch за этот запуск OBS");youtubeCount_->setToolTip(english_?"YouTube messages during this OBS session":"Сообщения YouTube за этот запуск OBS");
        target_->setToolTip(english_?"Send to Twitch":"Отправлять в Twitch");target_->setAccessibleName(target_->toolTip());youtubeTarget_->setToolTip(english_?"Send to YouTube":"Отправлять в YouTube");youtubeTarget_->setAccessibleName(youtubeTarget_->toolTip());
        if(broadcast_->count())broadcast_->setItemText(0,english_?"Choose stream":"Выберите эфир");
        refresh();updateTraffic();updateMetrics();
    }
    void updateTraffic(){traffic_->setText((english_?"Session traffic · ↑ ":"Трафик за сеанс · ↑ ")+volume(sc::traffic::sent.load(std::memory_order_relaxed),english_)+"  |  ↓ "+volume(sc::traffic::received.load(std::memory_order_relaxed),english_));}
    void requestEmote(const QString& id){
        if(id.isEmpty()||emotes_.contains(id)||pendingEmotes_.contains(id)||failedEmotes_.contains(id)||emotes_.size()+pendingEmotes_.size()+failedEmotes_.size()>=64)return;
        QNetworkRequest request(QUrl("https://static-cdn.jtvnw.net/emoticons/v2/"+id+"/static/dark/1.0"));
        request.setTransferTimeout(10000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
        request.setRawHeader("Accept","image/png");pendingEmotes_.insert(id);
        auto* reply=emoteHttp_.get(request);
        connect(reply,&QNetworkReply::readyRead,reply,[reply]{if(reply->bytesAvailable()>65536)reply->abort();});
        connect(reply,&QNetworkReply::finished,this,[this,reply,id]{
            sc::CpuScope cpu;pendingEmotes_.remove(id);
            const auto bytes=reply->readAll();sc::traffic::addReceived(bytes.size());
            const bool ok=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200&&bytes.size()<=65536;
            QPixmap image;if(ok)image.loadFromData(bytes,"PNG");reply->deleteLater();
            if(image.isNull()||image.width()>128||image.height()>128){failedEmotes_.insert(id);return;}
            emotes_.insert(id,image.scaled(20,20,Qt::KeepAspectRatio,Qt::SmoothTransformation));
            redrawMessages();
        });
    }
    void updateMetrics(){
        const auto cpuNow=sc::measuredCpuNs.load(std::memory_order_relaxed);
        if(usageClock_.isValid()&&usageClock_.nsecsElapsed()>0){
            const double cores=std::max(1u,std::thread::hardware_concurrency());
            const double percent=100.0*double(cpuNow-lastCpuNs_)/double(usageClock_.nsecsElapsed())/cores;
            cpuValue_->setText(percent<0.01?(english_?"<0.01 %":"<0,01 %"):QLocale(english_?QLocale::English:QLocale::Russian).toString(percent,'f',2)+" %");
        }
        lastCpuNs_=cpuNow;usageClock_.restart();
        quint64 bytes=quint64(messages_->document()->characterCount())*sizeof(QChar);
        auto add=[&bytes](const sc::ChatMessage& m){for(const auto* s:{&m.platform,&m.id,&m.authorId,&m.author,&m.text})bytes+=quint64(s->capacity())*sizeof(QChar);for(const auto& part:m.fragments){bytes+=quint64(part.text.capacity()+part.emoteId.capacity())*sizeof(QChar);}};
        for(const auto& m:history_)add(m);for(const auto& m:pending_)add(m);
        for(auto it=emotes_.cbegin();it!=emotes_.cend();++it)bytes+=quint64(it.value().width())*quint64(it.value().height())*4;
        memoryValue_->setText(volume(bytes,english_));
    }
    bool atBottom()const{const auto* bar=messages_->verticalScrollBar();return bar->value()>=bar->maximum()-3;}
    void updateScrollButton(){scrollBottom_->setVisible(!atBottom()&&unread_>0);scrollBottom_->setText(unread_>0?"↓ "+QString::number(unread_):"↓");}
    void redrawMessages(){
        auto* bar=messages_->verticalScrollBar();const bool bottom=atBottom();const int oldPosition=bar->value();
        redrawing_=true;messages_->clear();for(const auto& message:history_)appendMessage(message,false);
        bar->setValue(bottom?bar->maximum():oldPosition);redrawing_=false;updateScrollButton();
    }
    void appendMessage(const sc::ChatMessage& message,bool fresh=true){
        auto* bar=messages_->verticalScrollBar();const bool bottom=atBottom();const int oldPosition=bar->value();
        const bool wasRedrawing=redrawing_;redrawing_=true;
        auto text=message.text,author=message.author;text.replace('\n',' ');text.replace('\r',' ');author.replace('\n',' ');author.replace('\r',' ');
        QTextCursor cursor(messages_->document());cursor.movePosition(QTextCursor::End);if(!messages_->document()->isEmpty())cursor.insertBlock();
        QTextImageFormat icon;icon.setName(message.platform=="YouTube"?"stream-chat://icon/youtube":"stream-chat://icon/twitch");icon.setWidth(14);icon.setHeight(14);cursor.insertImage(icon);cursor.insertText(" ");
        const bool youtube=message.platform=="YouTube";const QColor accent=youtube?QColor("#ff8491"):QColor("#b995ff");const QColor tint=youtube?QColor("#32181d"):QColor("#241b35");
        QTextCharFormat authorStyle;authorStyle.setForeground(accent);authorStyle.setBackground(tint);authorStyle.setFontWeight(QFont::DemiBold);cursor.insertText(" "+author+" ",authorStyle);
        if(message.sentAt.isValid()){QTextCharFormat timeStyle;timeStyle.setForeground(QColor("#8ea1c0"));cursor.insertText(" "+message.sentAt.toLocalTime().toString("HH:mm"),timeStyle);}
        QTextCharFormat messageStyle;messageStyle.setForeground(QColor("#dfe8ff"));cursor.insertText("  ",messageStyle);
        if(message.platform=="Twitch"&&!message.fragments.isEmpty()){
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
        paintStatus(twitchStatus_,twitch_.status(),"Twitch",twitch_.ready(),english_,textScale_);paintStatus(youtubeStatus_,youtube_.status(),"YouTube",youtube_.ready(),english_,textScale_);
        const QLocale ru(english_?QLocale::English:QLocale::Russian);
        twitchViewers_->setVisible(streaming_&&twitch_.viewerCount()>=0);
        youtubeViewers_->setVisible(streaming_&&youtube_.viewerCount()>=0);
        if(twitch_.viewerCount()>=0){twitchViewers_->setText("👥 "+ru.toString(twitch_.viewerCount()));twitchViewers_->setStyleSheet(QString("color:%1;font-size:%2px;").arg(twitch_.viewerFresh()?"#dfe8ff":"#8ea1c0").arg(std::max(9,11*textScale_/100)));twitchViewers_->setToolTip(twitch_.viewerFresh()?(english_?"Twitch viewers now":"Смотрят Twitch сейчас"):(english_?"Last known Twitch viewer count":"Последнее известное число зрителей Twitch"));}
        if(youtube_.viewerCount()>=0){youtubeViewers_->setText("👥 "+ru.toString(youtube_.viewerCount()));youtubeViewers_->setStyleSheet(QString("color:%1;font-size:%2px;").arg(youtube_.viewerFresh()?"#dfe8ff":"#8ea1c0").arg(std::max(9,11*textScale_/100)));youtubeViewers_->setToolTip(youtube_.viewerFresh()?(english_?"YouTube viewers now":"Смотрят YouTube сейчас"):(english_?"Last known YouTube viewer count":"Последнее известное число зрителей YouTube"));}
        const bool showCounts=streaming_&&seenThisStream_;
        totalCount_->setVisible(showCounts&&(twitchMessages_+youtubeMessages_>0));
        twitchCount_->setVisible(showCounts&&twitchMessages_>0);youtubeCount_->setVisible(showCounts&&youtubeMessages_>0);
        if(showCounts){totalCount_->setText("✉ "+QString::number(twitchMessages_+youtubeMessages_));twitchCount_->setText("✉ "+QString::number(twitchMessages_));youtubeCount_->setText("✉ "+QString::number(youtubeMessages_));}
        const bool twitchBusy=code_->isVisible()&&twitch_.status().contains("подтвердите",Qt::CaseInsensitive);if(code_->isVisible()&&!twitchBusy&&!twitch_.ready())code_->hide();
        login_->setVisible(!twitch_.ready());logout_->setVisible(twitch_.ready());login_->setEnabled(!twitchBusy);
        googleLogin_->setVisible(!youtube_.signedIn());googleLogout_->setVisible(youtube_.signedIn());
        const bool youtubeBusy=youtube_.status().contains("подтвердите",Qt::CaseInsensitive)||youtube_.status().contains("завершаем",Qt::CaseInsensitive);googleLogin_->setEnabled(!youtubeBusy);broadcast_->setVisible(youtube_.signedIn()&&broadcast_->count()>2);
        delivery_->setVisible(!delivery_->text().isEmpty());
        const bool destinations=(target_->isChecked()&&twitch_.ready())||(youtubeTarget_->isChecked()&&youtube_.ready());
        input_->setEnabled((twitch_.ready()||youtube_.ready())&&!activeSends_);
        send_->setEnabled(destinations&&!activeSends_&&!input_->text().trimmed().isEmpty());
        target_->setEnabled(!activeSends_);youtubeTarget_->setEnabled(!activeSends_);
    }
    void send(){
        if(activeSends_||input_->text().trimmed().isEmpty())return;
        const bool tw=target_->isChecked()&&twitch_.ready(),yt=youtubeTarget_->isChecked()&&youtube_.ready();
        if(!tw&&!yt)return;
        if(yt&&input_->text().size()>200){delivery_->setText(english_?"YouTube messages are limited to 200 characters.":"Для YouTube сократите сообщение до 200 символов.");return;}
        activeSends_=int(tw)+int(yt);allSent_=true;twitchSent_=false;youtubeSent_=false;results_.clear();
        delivery_->setText(english_?"Sending to selected channels…":"Отправляем в выбранные каналы…");refresh();const auto text=input_->text();
        if(tw)twitch_.send(text);if(yt)youtube_.send(text);
    }
    void completed(const QString& platform,bool confirmed,const QString& reason){
        if(!activeSends_)return;allSent_=allSent_&&confirmed;results_.append(english_?(platform+(confirmed?": Message sent":": Delivery unconfirmed; no automatic retry")):reason);
        if(platform=="Twitch")twitchSent_=confirmed;else youtubeSent_=confirmed;
        --activeSends_;if(!activeSends_){if(allSent_)input_->clear();else {if(twitchSent_)target_->setChecked(false);if(youtubeSent_)youtubeTarget_->setChecked(false);}}
        delivery_->setText(results_.join('\n'));refresh();
    }
    void flush(){sc::CpuScope cpu;int count=0;while(!pending_.isEmpty()&&count++<50){const auto message=pending_.dequeue();history_.enqueue(message);while(history_.size()>500)history_.dequeue();appendMessage(message);if(streaming_){if(message.platform=="Twitch")++twitchMessages_;else if(message.platform=="YouTube")++youtubeMessages_;seenThisStream_=true;}}refresh();if(!pending_.isEmpty())batch_.start();}
    template<class Predicate>void remove(Predicate predicate){
        history_.erase(std::remove_if(history_.begin(),history_.end(),predicate),history_.end());
        pending_.erase(std::remove_if(pending_.begin(),pending_.end(),predicate),pending_.end());
        redrawMessages();
    }
    sc::TwitchChat twitch_;sc::YouTubeChat youtube_;QTextEdit* messages_=nullptr;QLabel* twitchStatus_=nullptr;QLabel* youtubeStatus_=nullptr;QLabel* twitchViewers_=nullptr;QLabel* youtubeViewers_=nullptr;QLabel* code_=nullptr;QLabel* delivery_=nullptr;QLabel* totalCount_=nullptr;QLabel* twitchCount_=nullptr;QLabel* youtubeCount_=nullptr;QLabel* traffic_=nullptr;QLabel* cpuValue_=nullptr;QLabel* memoryValue_=nullptr;QLabel* chatTitle_=nullptr;QLabel* recipientHint_=nullptr;QLabel* usageTitle_=nullptr;QLabel* memoryName_=nullptr;QWidget* usageCard_=nullptr;QPushButton* settings_=nullptr;QPushButton* scrollBottom_=nullptr;
    QLineEdit* input_=nullptr;QAbstractButton* target_=nullptr;QPushButton* send_=nullptr;QPushButton* login_=nullptr;QPushButton* logout_=nullptr;
    QAbstractButton* youtubeTarget_=nullptr;QComboBox* broadcast_=nullptr;QPushButton* googleLogin_=nullptr;QPushButton* googleLogout_=nullptr;
    QQueue<sc::ChatMessage> history_,pending_;QTimer batch_,trafficTimer_;QElapsedTimer usageClock_;QNetworkAccessManager emoteHttp_;QHash<QString,QPixmap> emotes_;QSet<QString> pendingEmotes_,failedEmotes_;std::uint64_t lastCpuNs_=0;int activeSends_=0,unread_=0,textScale_=100;bool allSent_=false,twitchSent_=false,youtubeSent_=false;bool streaming_=false,seenThisStream_=false,english_=false,redrawing_=false;QString language_="auto";quint64 twitchMessages_=0,youtubeMessages_=0;QStringList results_;
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
