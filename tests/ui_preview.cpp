#include <QApplication>
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
    QApplication app(argc,argv);
    app.setFont(QFont("Segoe UI",9));
    if(argc<2)return 2;
    const QString output=QString::fromLocal8Bit(argv[1]);
    if(!QDir().mkpath(output))return 3;
    const QString config=output+"/preview-config";
    if(!QDir().mkpath(config))return 4;
    qputenv("STREAM_CHAT_PREVIEW_CONFIG",config.toUtf8());
    {QSettings settings(config+"/ui.ini",QSettings::IniFormat);settings.setValue("language","ru");settings.sync();}
    {
        ChatDock narrow;narrow.resize(275,600);narrow.show();app.processEvents();
        auto* chat=narrow.findChild<QTextEdit*>();
        if(narrow.width()!=275||!chat||chat->lineWrapMode()!=QTextEdit::WidgetWidth||
           chat->wordWrapMode()!=QTextOption::WrapAtWordBoundaryOrAnywhere)return 17;
        if(!narrow.grab().save(output+"/chat-narrow-ru.png"))return 18;
        narrow.hide();app.processEvents();
    }
    for(const auto& language:{QString("ru"),QString("en")}){
        {QSettings settings(config+"/ui.ini",QSettings::IniFormat);settings.setValue("language",language);settings.setValue("textScale",100);settings.sync();}
        ChatDock previewDock;previewDock.resize(360,740);previewDock.show();app.processEvents();
        sc::ChatMessage twitch{"Twitch","preview-1","viewer-1","Viewer","Привет из Twitch!"};
        twitch.sentAt=QDateTime(QDate(2026,10,26),QTime(14,32),Qt::LocalTime);
        sc::ChatMessage youtube{"YouTube","preview-2","viewer-2","Guest","Привет из YouTube!"};
        youtube.sentAt=QDateTime(QDate(2026,10,26),QTime(14,33),Qt::LocalTime);
        if(language=="en"){twitch.text="Hello from Twitch!";youtube.text="Hello from YouTube!";}
        previewDock.previewAppend(twitch);previewDock.previewAppend(youtube);app.processEvents();
        if(!previewDock.grab().save(output+"/chat-"+language+".png"))return 5;
        auto* messages=previewDock.findChild<QTextEdit*>();auto* jump=previewDock.findChild<QPushButton*>("ScrollBottom");
        if(!messages||!jump)return 6;
        auto* settingsButton=previewDock.findChild<QPushButton*>("Settings");
        if(!settingsButton)return 10;
        auto* brand=previewDock.findChild<QWidget*>("HeaderBrand");
        if(!brand||brand->size()!=QSize(28,28)||
           settingsButton->size()!=QSize(30,28)||settingsButton->iconSize()!=QSize(19,19)||
           settingsButton->y()!=brand->y())return 16;
        const QString interfaceStyle=previewDock.styleSheet();
        settingsButton->click();app.processEvents();
        auto* settingsPage=previewDock.findChild<QWidget*>("SettingsPage");
        auto* slider=settingsPage?settingsPage->findChild<QSlider*>():nullptr;
        if(!settingsPage||!settingsPage->isVisible()||!slider)return 11;
        if(!previewDock.grab().save(output+"/settings-"+language+".png"))return 12;
        slider->setValue(140);app.processEvents();
        if(messages->document()->defaultFont().pixelSize()!=16||previewDock.styleSheet()!=interfaceStyle)return 13;
        auto* back=settingsPage->findChild<QPushButton*>();if(!back)return 14;
        back->click();app.processEvents();
        if(!messages->isVisible())return 15;
        for(int i=0;i<70;++i){auto next=twitch;next.id="preview-"+QString::number(i+3);previewDock.previewAppend(next);}
        previewDock.previewRedraw();
        if(messages->document()->resource(QTextDocument::ImageResource,QUrl("stream-chat://icon/twitch")).value<QPixmap>().isNull())return 9;
        messages->verticalScrollBar()->setValue(0);previewDock.previewAppend(youtube);app.processEvents();
        if(!jump->isVisible()||!jump->text().contains('1'))return 7;
        jump->click();app.processEvents();if(jump->isVisible())return 8;
        previewDock.hide();app.processEvents();
    }
    {QSettings settings(config+"/ui.ini",QSettings::IniFormat);settings.setValue("hideBots",true);settings.setValue("botNames","Nightbot");settings.setValue("mentionNames","Streamer");settings.setValue("highlightMentions",true);settings.sync();}
    {ChatDock filters;filters.resize(360,740);filters.show();app.processEvents();
        filters.previewAppend(sc::ChatMessage{"Twitch","bot","bot","Nightbot","hidden bot message"});
        filters.previewAppend(sc::ChatMessage{"YouTube","mention","guest","Guest","Hello @Streamer!"});
        auto* messages=filters.findChild<QTextEdit*>();if(!messages||messages->toPlainText().contains("hidden bot message")||!messages->toPlainText().contains("Hello @Streamer!"))return 19;
        QTextCursor cursor(messages->document());cursor.movePosition(QTextCursor::Start);if(cursor.blockFormat().background().color()!=QColor("#193b34"))return 20;
        if(!filters.grab().save(output+"/chat-mentions.png"))return 21;
    }
    for(const auto& language:{QString("ru"),QString("en")})for(int width:{275,360}){
        {QSettings settings(config+"/ui.ini",QSettings::IniFormat);settings.setValue("language",language);settings.setValue("kickEnabled",false);settings.sync();}
        ChatDock dock;dock.resize(width,740);dock.show();app.processEvents();
        auto* card=dock.findChild<QWidget*>("KickCard");if(!card||card->isVisible())return 22;
        auto* settings=dock.findChild<QPushButton*>("Settings");settings->click();app.processEvents();
        auto* page=dock.findChild<QWidget*>("SettingsPage");auto* toggle=page->findChild<QCheckBox*>("EnableKick");
        auto* account=page->findChild<QWidget*>("KickAccountSettings");
        if(!toggle||!account||toggle->isChecked()||account->isVisible())return 23;
        toggle->setChecked(true);app.processEvents();if(!account->isVisible()||card->isHidden())return 24;
        auto* secret=account->findChild<QLineEdit*>("KickClientSecret");if(!secret||secret->echoMode()!=QLineEdit::Password)return 25;
        auto* scroll=page->findChild<QScrollArea*>();scroll->ensureWidgetVisible(account,0,0);app.processEvents();
        if(dock.width()!=width)return 26;
        if(!dock.grab().save(output+"/kick-settings-"+language+"-"+QString::number(width)+".png"))return 27;
        auto* guide=account->findChild<QLabel*>("KickSetupGuide");account->findChild<QPushButton*>("KickGuideToggle")->click();app.processEvents();
        if(!guide->isVisible()||!guide->wordWrap()||!guide->text().contains(sc::KickChat::callback().mid(0,0)+"chat:write"))return 28;
        scroll->ensureWidgetVisible(guide,0,0);app.processEvents();
        if(!dock.grab().save(output+"/kick-guide-"+language+"-"+QString::number(width)+".png"))return 29;
        auto* back=page->findChild<QPushButton*>();back->click();app.processEvents();
        if(!card->isVisible()||dock.width()!=width)return 30;
        dock.previewAppend(sc::ChatMessage{"Kick","kick-ui-1","42","KickViewer","<b>Kick message</b>"});dock.previewRedraw();app.processEvents();
        auto* messages=dock.findChild<QTextEdit*>();
        if(!messages->toPlainText().contains("<b>Kick message</b>")||messages->document()->resource(QTextDocument::ImageResource,QUrl("stream-chat://icon/kick")).value<QPixmap>().isNull())return 31;
        if(!dock.grab().save(output+"/kick-chat-"+language+"-"+QString::number(width)+".png"))return 32;
        settings->click();app.processEvents();page=dock.findChild<QWidget*>("SettingsPage");page->findChild<QCheckBox*>("EnableKick")->setChecked(false);app.processEvents();
        if(!card->isHidden()||page->findChild<QWidget*>("KickAccountSettings")->isVisible())return 33;
        dock.hide();app.processEvents();
    }
    return 0;
}
