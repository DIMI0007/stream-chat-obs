#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>
#include "twitch_chat.hpp"
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
    ChatDock():twitch_(configPath("twitch-account.dat"),this){
        auto* layout=new QVBoxLayout(this);layout->setContentsMargins(8,8,8,8);
        auto* connections=new QHBoxLayout;
        auto* login=new QPushButton("Войти в Twitch");auto* logout=new QPushButton("Отключить Twitch");
        connections->addWidget(login);connections->addWidget(logout);layout->addLayout(connections);
        connect(login,&QPushButton::clicked,&twitch_,&sc::TwitchChat::signIn);
        connect(logout,&QPushButton::clicked,&twitch_,&sc::TwitchChat::signOut);
        twitchStatus_=new QLabel;twitchStatus_->setWordWrap(true);twitchStatus_->setTextFormat(Qt::PlainText);layout->addWidget(twitchStatus_);
        code_=new QLabel;code_->setTextFormat(Qt::PlainText);code_->setWordWrap(true);code_->setTextInteractionFlags(Qt::TextSelectableByMouse);code_->hide();layout->addWidget(code_);
        auto* youtube=new QLabel("YouTube: подключение разрабатывается");youtube->setWordWrap(true);layout->addWidget(youtube);
        messages_=new QPlainTextEdit;messages_->setReadOnly(true);messages_->document()->setMaximumBlockCount(500);
        messages_->setPlaceholderText("Сообщения появятся после входа. История на диск не записывается.");layout->addWidget(messages_,1);
        auto* targets=new QHBoxLayout;target_=new QCheckBox("Twitch");target_->setChecked(true);targets->addWidget(target_);
        auto* yt=new QCheckBox("YouTube");yt->setEnabled(false);targets->addWidget(yt);targets->addStretch();layout->addLayout(targets);
        auto* composer=new QHBoxLayout;input_=new QLineEdit;input_->setMaxLength(500);input_->setPlaceholderText("Сообщение в свой канал Twitch");
        send_=new QPushButton("Отправить");composer->addWidget(input_,1);composer->addWidget(send_);layout->addLayout(composer);
        delivery_=new QLabel;delivery_->setWordWrap(true);delivery_->setTextFormat(Qt::PlainText);layout->addWidget(delivery_);
        auto* version=new QLabel("Stream Chat " STREAM_CHAT_VERSION);version->setAlignment(Qt::AlignRight);layout->addWidget(version);
        batch_.setInterval(20);batch_.setSingleShot(true);
        connect(&batch_,&QTimer::timeout,this,[this]{flush();});
        connect(send_,&QPushButton::clicked,this,[this]{send();});connect(input_,&QLineEdit::returnPressed,this,[this]{send();});
        connect(target_,&QCheckBox::toggled,this,[this]{refresh();});
        connect(&twitch_,&sc::TwitchChat::changed,this,[this]{if(twitch_.ready())code_->hide();refresh();});
        connect(&twitch_,&sc::TwitchChat::signInCode,this,[this](const QString& code,const QUrl& url){code_->setText("Код Twitch: "+code.left(64)+"\n"+url.toString());code_->show();});
        connect(&twitch_,&sc::TwitchChat::message,this,[this](const sc::ChatMessage& message){pending_.enqueue(message);while(pending_.size()>500)pending_.dequeue();if(!batch_.isActive())batch_.start();});
        connect(&twitch_,&sc::TwitchChat::removeMessage,this,[this](const QString& id){remove([&](const sc::ChatMessage& m){return m.platform=="Twitch"&&m.id==id;});});
        connect(&twitch_,&sc::TwitchChat::clearAuthor,this,[this](const QString& id){remove([&](const sc::ChatMessage& m){return m.platform=="Twitch"&&m.authorId==id;});});
        connect(&twitch_,&sc::TwitchChat::clearChat,this,[this]{remove([](const sc::ChatMessage& m){return m.platform=="Twitch";});});
        connect(&twitch_,&sc::TwitchChat::sent,this,[this](bool confirmed,const QString& reason){sending_=false;delivery_->setText(reason);if(confirmed)input_->clear();refresh();});
        refresh();
    }
protected:
    void showEvent(QShowEvent* event)override{QWidget::showEvent(event);twitch_.setVisible(true);}
    void hideEvent(QHideEvent* event)override{twitch_.setVisible(false);sending_=false;refresh();QWidget::hideEvent(event);}
private:
    QString line(const sc::ChatMessage& message)const{auto text=message.text;auto author=message.author;text.replace('\n',' ');text.replace('\r',' ');author.replace('\n',' ');author.replace('\r',' ');return "["+message.platform+"] "+author+": "+text;}
    void refresh(){twitchStatus_->setText(twitch_.status());target_->setEnabled(twitch_.ready());input_->setEnabled(twitch_.ready()&&!sending_);send_->setEnabled(twitch_.ready()&&target_->isChecked()&&!sending_);}
    void send(){if(sending_||!target_->isChecked()||!twitch_.ready()||input_->text().trimmed().isEmpty())return;sending_=true;delivery_->setText("Twitch: отправляем…");refresh();twitch_.send(input_->text());}
    void flush(){int count=0;while(!pending_.isEmpty()&&count++<50){const auto message=pending_.dequeue();history_.enqueue(message);while(history_.size()>500)history_.dequeue();messages_->appendPlainText(line(message));}if(!pending_.isEmpty())batch_.start();}
    template<class Predicate>void remove(Predicate predicate){
        history_.erase(std::remove_if(history_.begin(),history_.end(),predicate),history_.end());
        pending_.erase(std::remove_if(pending_.begin(),pending_.end(),predicate),pending_.end());
        QStringList lines;for(const auto& message:history_)lines.append(line(message));messages_->setPlainText(lines.join('\n'));
    }
    sc::TwitchChat twitch_;QPlainTextEdit* messages_=nullptr;QLabel* twitchStatus_=nullptr;QLabel* code_=nullptr;QLabel* delivery_=nullptr;
    QLineEdit* input_=nullptr;QCheckBox* target_=nullptr;QPushButton* send_=nullptr;
    QQueue<sc::ChatMessage> history_,pending_;QTimer batch_;bool sending_=false;
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

