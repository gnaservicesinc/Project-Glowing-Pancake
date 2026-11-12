#include "development_tools.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>

namespace pgp::app {
namespace {
QString checked_checkout(const QString& folder) {
    const QFileInfo directory(folder);
    if(!directory.isAbsolute() || !directory.isDir()) return {};
    const auto canonical=directory.canonicalFilePath();
    if(canonical.isEmpty()) return {};
    const QDir checkout(canonical);
    if(!QFileInfo(checkout.filePath(".git")).exists() ||
       !QFileInfo(checkout.filePath("CMakeLists.txt")).isFile() ||
       !QFileInfo(checkout.filePath("project-versions.json")).isFile() ||
       !QFileInfo(checkout.filePath("data/runtime/first-playable.json")).isFile()) return {};
    return checkout.path();
}

QString editor_path(const QString& checkout) {
#ifdef Q_OS_WIN
    const auto editor=QStringLiteral("pgp-editor.exe");
#else
    const auto editor=QStringLiteral("pgp-editor");
#endif
    const QDir application(QCoreApplication::applicationDirPath());
    const QDir project(checkout);
    const QStringList candidates{
        application.filePath(editor),
#ifdef Q_OS_MACOS
        application.filePath("../../../"+editor),
#endif
        project.filePath("build/release/bin/"+editor),
        project.filePath("build/debug/bin/"+editor),
    };
    for(const auto& candidate:candidates) {
        const QFileInfo file(candidate);
        if(file.isFile() && file.isExecutable()) return file.canonicalFilePath();
    }
    return {};
}
}

DevelopmentTools::DevelopmentTools(QString settings_path) : settings_path_(std::move(settings_path)) {
    QSettings settings(settings_path_,QSettings::IniFormat);
    remembered_checkout_=settings.value("contentStudio/checkout").toString();
}

QString DevelopmentTools::checkout() const {
    if(!remembered_checkout_.isEmpty()) return checked_checkout(remembered_checkout_);
    const auto root=QStringLiteral("/opt/AI Startup");
    const auto direct=checked_checkout(root);
    return direct.isEmpty()?checked_checkout(QDir(root).filePath("Project-Glowing-Pancake")):direct;
}

void DevelopmentTools::setMessage(QString message) {
    message_=std::move(message);
    emit changed();
}

bool DevelopmentTools::selectCheckout(const QUrl& folder) {
    const auto selected=folder.isLocalFile()?checked_checkout(folder.toLocalFile()):QString{};
    if(selected.isEmpty()) {
        setMessage(tr("Choose the Project Glowing Pancake repository folder."));
        return false;
    }
    QSettings settings(settings_path_,QSettings::IniFormat);
    settings.setValue("contentStudio/checkout",selected);
    settings.sync();
    if(settings.status()!=QSettings::NoError) {
        setMessage(tr("Couldn't save the Content Studio folder preference."));
        return false;
    }
    remembered_checkout_=selected;
    setMessage({});
    return true;
}

bool DevelopmentTools::openContentStudio() {
    const auto root=checkout();
    if(root.isEmpty()) return false;
    const auto editor=editor_path(root);
    if(editor.isEmpty()) {
        setMessage(tr("Build Content Studio with ./make.sh --editor, then try again."));
        return false;
    }
    if(!QProcess::startDetached(editor,{QStringLiteral("--project"),root},root)) {
        setMessage(tr("Couldn't open Content Studio."));
        return false;
    }
    setMessage({});
    return true;
}
}
