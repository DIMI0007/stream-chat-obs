#include <QApplication>
#include <QDir>
#include <QFont>
#include <QSettings>
#include <QStandardPaths>
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
        {QSettings settings(config+"/ui.ini",QSettings::IniFormat);settings.setValue("language",language);settings.sync();}
        ChatDock dock;dock.resize(360,740);dock.show();app.processEvents();
        if(!dock.grab().save(output+"/chat-"+language+".png"))return 5;
        dock.hide();app.processEvents();
    }
    return 0;
}
