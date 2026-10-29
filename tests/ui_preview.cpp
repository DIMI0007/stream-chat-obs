#include <QApplication>
#include <iostream>
static int previewResult(int code){std::cout<<"UI preview result: "<<code<<std::endl;return code;}
#include <QDir>
#include <QFont>
#include <QSettings>
#include <QStandardPaths>
#include <QScrollBar>
#include <QTextEdit>
#include <QPushButton>
#include <QSlider>
#include "../src/plugin-main.cpp"

int main(int argc,char** argv){
    std::cout<<"UI preview start"<<std::endl;QApplication app(argc,argv);
    app.setFont(QFont("Segoe UI",9));
    if(argc<2)return previewResult(2);
    const QString output=QString::fromLocal8Bit(argv[1]);
    if(!QDir().mkpath(output))return previewResult(3);
    const QString config=output+"/preview-config";
    if(!QDir().mkpath(config))return previewResult(4);
    qputenv("STREAM_CHAT_PREVIEW_CONFIG",config.toUtf8());
    {QSettings settings(config+"/ui.ini",QSettings::IniFormat);settings.setValue("language","ru");settings.sync();}
    {
        std::cout<<"Narrow dock"<<std::endl;ChatDock narrow;narrow.resize(275,600);narrow.show();app.processEvents();
        auto* chat=narrow.findChild<QTextEdit*>("ChatMessages");
        if(narrow.width()!=275||!chat||chat->lineWrapMode()!=QTextEdit::WidgetWidth||
           chat->wordWrapMode()!=QTextOption::WrapAtWordBoundaryOrAnywhere)return previewResult(17);
        if(!narrow.grab().save(output+"/chat-narrow-ru.png"))return previewResult(18);
        narrow.hide();app.processEvents();
    }
    for(const auto& language:{QString("ru"),QString("en")}){
        {QSettings settings(config+"/ui.ini",QSettings::IniFormat);settings.setValue("language",language);settings.setValue("textScale",100);settings.sync();}
        std::cout<<"Main/settings: "<<language.toStdString()<<std::endl;ChatDock previewDock;previewDock.resize(360,740);previewDock.show();app.processEvents();
        sc::ChatMessage twitch{"Twitch","preview-1","viewer-1","Viewer","Привет из Twitch!"};
        twitch.sentAt=QDateTime(QDate(2026,10,26),QTime(14,32),Qt::LocalTime);
        sc::ChatMessage youtube{"YouTube","preview-2","viewer-2","Guest","Привет из YouTube!"};
        youtube.sentAt=QDateTime(QDate(2026,10,26),QTime(14,33),Qt::LocalTime);
        if(language=="en"){twitch.text="Hello from Twitch!";youtube.text="Hello from YouTube!";}
        previewDock.previewAppend(twitch);previewDock.previewAppend(youtube);app.processEvents();
        if(!previewDock.grab().save(output+"/chat-"+language+".png"))return previewResult(5);
        auto* messages=previewDock.findChild<QTextEdit*>("ChatMessages");auto* jump=previewDock.findChild<QPushButton*>("ScrollBottom");
        if(!messages||!jump)return previewResult(6);
        auto* settingsButton=previewDock.findChild<QPushButton*>("Settings");
        if(!settingsButton)return previewResult(10);
        auto* brand=previewDock.findChild<QWidget*>("HeaderBrand");
        if(!brand||brand->size()!=QSize(28,28)||
           settingsButton->size()!=QSize(30,28)||settingsButton->iconSize()!=QSize(19,19)||
           settingsButton->y()!=brand->y())return previewResult(16);
        const QString interfaceStyle=previewDock.styleSheet();
        settingsButton->click();app.processEvents();
        auto* settingsPage=previewDock.findChild<QWidget*>("SettingsPage");
        auto* slider=settingsPage?settingsPage->findChild<QSlider*>():nullptr;
        if(!settingsPage||!settingsPage->isVisible()||!slider)return previewResult(11);
        if(!previewDock.grab().save(output+"/settings-"+language+".png"))return previewResult(12);
        slider->setValue(140);app.processEvents();
        if(messages->document()->defaultFont().pixelSize()!=16||previewDock.styleSheet()!=interfaceStyle)return previewResult(13);
        auto* back=settingsPage->findChild<QPushButton*>();if(!back)return previewResult(14);
        back->click();app.processEvents();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        if(!messages->isVisible())return previewResult(15);
        for(int i=0;i<70;++i){auto next=twitch;next.id="preview-"+QString::number(i+3);previewDock.previewAppend(next);}
        previewDock.previewRedraw();
        if(messages->document()->resource(QTextDocument::ImageResource,QUrl("stream-chat://icon/twitch")).value<QPixmap>().isNull())return previewResult(9);
        messages->verticalScrollBar()->setValue(0);previewDock.previewAppend(youtube);app.processEvents();
        if(!jump->isVisible()||!jump->text().contains('1'))return previewResult(7);
        jump->click();app.processEvents();if(jump->isVisible())return previewResult(8);
        previewDock.hide();app.processEvents();
    }
    {QSettings settings(config+"/ui.ini",QSettings::IniFormat);settings.setValue("hideBots",true);settings.setValue("botNames","Nightbot");settings.setValue("mentionNames","Streamer");settings.setValue("highlightMentions",true);settings.sync();}
    {ChatDock filters;filters.resize(360,740);filters.show();app.processEvents();
        filters.previewAppend(sc::ChatMessage{"Twitch","bot","bot","Nightbot","hidden bot message"});
        filters.previewAppend(sc::ChatMessage{"YouTube","mention","guest","Guest","Hello @Streamer!"});
        auto* messages=filters.findChild<QTextEdit*>("ChatMessages");if(!messages||messages->toPlainText().contains("hidden bot message")||!messages->toPlainText().contains("Hello @Streamer!"))return previewResult(19);
        QTextCursor cursor(messages->document());cursor.movePosition(QTextCursor::Start);if(cursor.blockFormat().background().color()!=QColor("#193b34"))return previewResult(20);
        if(!filters.grab().save(output+"/chat-mentions.png"))return previewResult(21);
    }
    for(const auto& language:{QString("ru"),QString("en")})for(int width:{275,360}){
        {QSettings settings(config+"/ui.ini",QSettings::IniFormat);settings.setValue("language",language);settings.setValue("kickEnabled",false);settings.sync();}
        std::cout<<"Kick: "<<language.toStdString()<<" width "<<width<<std::endl;ChatDock dock;dock.resize(width,740);dock.show();app.processEvents();
        auto* card=dock.findChild<QWidget*>("KickCard");if(!card||card->isVisible())return previewResult(22);
        auto* settings=dock.findChild<QPushButton*>("Settings");settings->click();app.processEvents();
        auto* page=dock.findChild<QWidget*>("SettingsPage");auto* toggle=page->findChild<QCheckBox*>("EnableKick");
        auto* account=page->findChild<QWidget*>("KickAccountSettings");
        if(!toggle||!account||toggle->isChecked()||account->isVisible())return previewResult(23);
        toggle->setChecked(true);app.processEvents();if(!account->isVisible()||card->isHidden())return previewResult(24);
        auto* secret=account->findChild<QLineEdit*>("KickClientSecret");if(!secret||secret->echoMode()!=QLineEdit::Password)return previewResult(25);
        auto* scroll=page->findChild<QScrollArea*>();scroll->ensureWidgetVisible(account,0,0);app.processEvents();
        if(dock.width()!=width)return previewResult(26);
        if(!dock.grab().save(output+"/kick-settings-"+language+"-"+QString::number(width)+".png"))return previewResult(27);
        auto* guide=account->findChild<QLabel*>("KickSetupGuide");account->findChild<QPushButton*>("KickGuideToggle")->click();app.processEvents();
        if(!guide->isVisible()||!guide->wordWrap()||!guide->text().contains("chat:write"))return previewResult(28);
        scroll->ensureWidgetVisible(guide,0,0);app.processEvents();
        if(!dock.grab().save(output+"/kick-guide-"+language+"-"+QString::number(width)+".png"))return previewResult(29);
        auto* back=page->findChild<QPushButton*>();back->click();app.processEvents();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        if(!card->isVisible()||dock.width()!=width)return previewResult(30);
        dock.previewAppend(sc::ChatMessage{"Kick","kick-ui-1","42","KickViewer","<b>Kick message</b>"});dock.previewRedraw();app.processEvents();
        auto* messages=dock.findChild<QTextEdit*>("ChatMessages");
        if(!messages->toPlainText().contains("<b>Kick message</b>")||messages->document()->resource(QTextDocument::ImageResource,QUrl("stream-chat://icon/kick")).value<QPixmap>().isNull())return previewResult(31);
        if(!dock.grab().save(output+"/kick-chat-"+language+"-"+QString::number(width)+".png"))return previewResult(32);
        settings->click();app.processEvents();page=dock.findChild<QWidget*>("SettingsPage");page->findChild<QCheckBox*>("EnableKick")->setChecked(false);app.processEvents();
        if(!card->isHidden()||page->findChild<QWidget*>("KickAccountSettings")->isVisible())return previewResult(33);
        dock.hide();app.processEvents();
    }
    return previewResult(0);
}
