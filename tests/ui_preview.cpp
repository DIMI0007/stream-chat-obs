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
    return 0;
}
