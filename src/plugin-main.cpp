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
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHideEvent>
#include <QShowEvent>
#include <QCoreApplication>
#include <QSslSocket>
#include <QPointer>
#include <algorithm>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("stream-chat-obs", "en-US")
namespace {
constexpr const char* DockId="stream-chat-obs.chat";
QString configPath(const char* name){char* raw=obs_module_config_path(name);if(!raw)return {};const auto path=QString::fromUtf8(raw);bfree(raw);return path;}
class ChatDock final : public QWidget {
public:
    ChatDock():twitch_(configPath("twitch-account.dat"),this),youtube_(configPath("youtube-account.dat"),this){
        auto* layout=new QVBoxLayout(this);layout->setContentsMargins(8,8,8,8);
        auto* connections=new QHBoxLayout;
        auto* login=new QPushButton("Войти в Twitch");auto* logout=new QPushButton("Отключить Twitch");
        connections->addWidget(login);connections->addWidget(logout);layout->addLayout(connections);
        connect(login,&QPushButton::clicked,&twitch_,&sc::TwitchChat::signIn);
        connect(logout,&QPushButton::clicked,&twitch_,&sc::TwitchChat::signOut);
        twitchStatus_=new QLabel;twitchStatus_->setWordWrap(true);twitchStatus_->setTextFormat(Qt::PlainText);layout->addWidget(twitchStatus_);
        code_=new QLabel;code_->setTextFormat(Qt::PlainText);code_->setWordWrap(true);code_->setTextInteractionFlags(Qt::TextSelectableByMouse);code_->hide();layout->addWidget(code_);
        auto* googleButtons=new QHBoxLayout;
        auto* googleLogin=new QPushButton("Войти в YouTube");auto* googleLogout=new QPushButton("Отключить YouTube");
        googleButtons->addWidget(googleLogin);googleButtons->addWidget(googleLogout);layout->addLayout(googleButtons);
        connect(googleLogin,&QPushButton::clicked,&youtube_,&sc::YouTubeChat::signIn);
        connect(googleLogout,&QPushButton::clicked,&youtube_,&sc::YouTubeChat::signOut);
        youtubeStatus_=new QLabel;youtubeStatus_->setWordWrap(true);youtubeStatus_->setTextFormat(Qt::PlainText);layout->addWidget(youtubeStatus_);
        auto* broadcasts=new QHBoxLayout;broadcast_=new QComboBox;broadcast_->addItem("Эфир не выбран",QString());
        auto* find=new QPushButton("Найти эфир");broadcasts->addWidget(broadcast_,1);broadcasts->addWidget(find);layout->addLayout(broadcasts);
        connect(find,&QPushButton::clicked,&youtube_,&sc::YouTubeChat::discover);
        connect(broadcast_,&QComboBox::currentIndexChanged,this,[this](int index){const auto id=broadcast_->itemData(index).toString();if(!id.isEmpty())youtube_.selectChat(id);});
        connect(&youtube_,&sc::YouTubeChat::broadcasts,this,[this](const QJsonArray& choices){
            const QSignalBlocker block(broadcast_);broadcast_->clear();broadcast_->addItem("Выберите эфир",QString());
            for(const auto& value:choices){const auto choice=value.toObject();broadcast_->addItem(choice.value("title").toString(),choice.value("id").toString());}
            if(choices.size()==1)broadcast_->setCurrentIndex(1);
        });
        messages_=new QPlainTextEdit;messages_->setReadOnly(true);messages_->document()->setMaximumBlockCount(500);
        messages_->setPlaceholderText("Сообщения появятся после входа. История на диск не записывается.");layout->addWidget(messages_,1);
        auto* targets=new QHBoxLayout;target_=new QCheckBox("Twitch");target_->setChecked(true);targets->addWidget(target_);
        youtubeTarget_=new QCheckBox("YouTube");targets->addWidget(youtubeTarget_);targets->addStretch();layout->addLayout(targets);
        auto* composer=new QHBoxLayout;input_=new QLineEdit;input_->setMaxLength(500);input_->setPlaceholderText("Сообщение в выбранные каналы");
        send_=new QPushButton("Отправить");composer->addWidget(input_,1);composer->addWidget(send_);layout->addLayout(composer);
        delivery_=new QLabel;delivery_->setWordWrap(true);delivery_->setTextFormat(Qt::PlainText);layout->addWidget(delivery_);
        auto* version=new QLabel("Stream Chat " STREAM_CHAT_VERSION);version->setAlignment(Qt::AlignRight);layout->addWidget(version);
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
    QString line(const sc::ChatMessage& message)const{auto text=message.text;auto author=message.author;text.replace('\n',' ');text.replace('\r',' ');author.replace('\n',' ');author.replace('\r',' ');return "["+message.platform+"] "+author+": "+text;}
    void refresh(){
        twitchStatus_->setText(twitch_.status());youtubeStatus_->setText(youtube_.status());
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
    void flush(){int count=0;while(!pending_.isEmpty()&&count++<50){const auto message=pending_.dequeue();history_.enqueue(message);while(history_.size()>500)history_.dequeue();messages_->appendPlainText(line(message));}if(!pending_.isEmpty())batch_.start();}
    template<class Predicate>void remove(Predicate predicate){
        history_.erase(std::remove_if(history_.begin(),history_.end(),predicate),history_.end());
        pending_.erase(std::remove_if(pending_.begin(),pending_.end(),predicate),pending_.end());
        QStringList lines;for(const auto& message:history_)lines.append(line(message));messages_->setPlainText(lines.join('\n'));
    }
    sc::TwitchChat twitch_;sc::YouTubeChat youtube_;QPlainTextEdit* messages_=nullptr;QLabel* twitchStatus_=nullptr;QLabel* youtubeStatus_=nullptr;QLabel* code_=nullptr;QLabel* delivery_=nullptr;
    QLineEdit* input_=nullptr;QCheckBox* target_=nullptr;QPushButton* send_=nullptr;
    QCheckBox* youtubeTarget_=nullptr;QComboBox* broadcast_=nullptr;
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

