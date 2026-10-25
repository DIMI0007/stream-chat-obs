#pragma once
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <utility>
#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace sc {
class SecureStore {
public:
    explicit SecureStore(QString path):path_(std::move(path)){}
    QJsonObject load(bool* ok=nullptr) const {
        if(ok)*ok=false;QFile file(path_);
        if(!file.exists()){if(ok)*ok=true;return {};}
        if(!file.open(QIODevice::ReadOnly)||file.size()>1048576)return {};
#ifdef Q_OS_WIN
        auto bytes=file.readAll();DATA_BLOB input{DWORD(bytes.size()),reinterpret_cast<BYTE*>(bytes.data())},output{};
        if(!CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output))return {};
        const QByteArray plain(reinterpret_cast<const char*>(output.pbData),int(output.cbData));
        SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);
        QJsonParseError error;const auto document=QJsonDocument::fromJson(plain,&error);
        if(error.error!=QJsonParseError::NoError||!document.isObject())return {};
        if(ok)*ok=true;return document.object();
#else
        return {};
#endif
    }
    bool save(const QJsonObject& value) const {
#ifdef Q_OS_WIN
        auto plain=QJsonDocument(value).toJson(QJsonDocument::Compact);
        DATA_BLOB input{DWORD(plain.size()),reinterpret_cast<BYTE*>(plain.data())},output{};
        if(!CryptProtectData(&input,L"Stream Chat OBS accounts",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output))return false;
        SecureZeroMemory(plain.data(),SIZE_T(plain.size()));
        const QByteArray encrypted(reinterpret_cast<const char*>(output.pbData),int(output.cbData));LocalFree(output.pbData);
        if(!QDir().mkpath(QFileInfo(path_).absolutePath()))return false;
        QSaveFile file(path_);return file.open(QIODevice::WriteOnly)&&file.write(encrypted)==encrypted.size()&&file.commit();
#else
        Q_UNUSED(value);return false;
#endif
    }
private:QString path_;
};
}

