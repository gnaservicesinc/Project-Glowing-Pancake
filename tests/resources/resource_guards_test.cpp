#include <pgp/resources/resource_guards.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QLibraryInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QTemporaryDir>
#include <QThread>

#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool wait_until(const std::function<bool()>& ready) {
    QElapsedTimer elapsed;
    elapsed.start();
    while (!ready() && elapsed.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
    return ready();
}

pgp::resources::ResourcePolicy qml_policy() {
    pgp::resources::ResourcePolicy policy;
    require(policy.allow_local_root(QLibraryInfo::path(QLibraryInfo::QmlImportsPath)),
            "Qt QML import root must exist");
    require(policy.allow_resource_prefix(QStringLiteral("/qt-project.org/imports/")), "Qt imports prefix");
    require(policy.allow_resource_prefix(QStringLiteral("/qt/qml/")), "Qt QML resource prefix");
    return policy;
}

void write_file(const QString& path, const QByteArray& content) {
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "open fixture");
    require(file.write(content) == content.size(), "write fixture");
}

class FixtureImageProvider final : public QQuickImageProvider {
public:
    FixtureImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString& id, QSize* size, const QSize& requested) override {
        Q_UNUSED(id);
        Q_UNUSED(requested);
        QImage image(2, 2, QImage::Format_RGBA8888);
        image.fill(Qt::green);
        if (size != nullptr) {
            *size = image.size();
        }
        return image;
    }
};

std::unique_ptr<QObject> create_component(QQmlEngine& engine, const QByteArray& source) {
    QQmlComponent component(&engine);
    component.setData(source, QUrl(QStringLiteral("qrc:/pgp/tests/inline.qml")));
    require(wait_until([&] { return !component.isLoading(); }), "QML compilation timed out");
    if (component.isError()) {
        std::cerr << component.errorString().toStdString();
    }
    auto object = std::unique_ptr<QObject>(component.create());
    require(object != nullptr, "QML component must load under guards");
    return object;
}

void test_policy() {
    QTemporaryDir scratch(QDir::currentPath() + QStringLiteral("/resource-policy-XXXXXX"));
    require(scratch.isValid(), "create local policy scratch");
    const auto approved = scratch.path() + QStringLiteral("/approved");
    require(QDir().mkdir(approved), "create approved fixture directory");
    const auto local = approved + QStringLiteral("/local.txt");
    const auto outside = scratch.path() + QStringLiteral("/outside.txt");
    write_file(local, "approved");
    write_file(outside, "outside");
    auto policy = qml_policy();
    require(policy.allow_local_root(approved), "register approved local root");
    require(!policy.allow_local_root(scratch.path() + QStringLiteral("/missing")), "reject missing root");
    require(policy.allow_image_provider(QStringLiteral("trusted")), "register exact image provider");
    require(!policy.allow_image_provider(QStringLiteral("trusted/other")), "reject malformed provider");
    require(policy.allows(QUrl::fromLocalFile(local)), "allow canonical local file");
    require(!policy.allows(QUrl::fromLocalFile(outside)), "deny unregistered local file");
    require(!policy.allows(QUrl::fromLocalFile(approved + QStringLiteral("/../outside.txt"))), "deny traversal");
    require(!policy.allows(QUrl::fromLocalFile(approved + QStringLiteral("/missing.txt"))), "deny missing file");
    const auto link = approved + QStringLiteral("/escape.txt");
    if (QFile::link(outside, link)) {
        require(!policy.allows(QUrl::fromLocalFile(link)), "deny symlink escape");
    } else {
        std::cout << "Symlink creation unavailable on this host; native symlink qualification remains open.\n";
    }
    require(policy.allows(QUrl(QStringLiteral("qrc:/pgp/tests/allowed.txt"))), "allow bundled file");
    require(!policy.allows(QUrl(QStringLiteral("qrc:/pgp/../private.txt"))), "deny qrc traversal");
    require(!policy.allows(QUrl(QStringLiteral("qrc://host/pgp/tests/allowed.txt"))), "deny qrc authority");
    require(policy.allows(QUrl(QStringLiteral("image://trusted/icon"))), "allow registered image provider");
    require(!policy.allows(QUrl(QStringLiteral("image://trusted.evil/icon"))), "deny provider prefix match");
    require(!policy.allows(QUrl(QStringLiteral("image://unknown/icon"))), "deny unknown provider");
    for (const auto* denied : {"https://example.invalid/asset", "http://127.0.0.1/asset", "ftp://example.invalid/a",
                               "file://localhost/private", "file://host/private", "file:////host/private",
                               "data:text/plain,hidden", "relative.txt", "qrc:/private.txt"}) {
        require(!policy.allows(QUrl(QString::fromLatin1(denied))), "deny remote/unregistered URL");
    }
}

void test_manager() {
    QTemporaryDir scratch(QDir::currentPath() + QStringLiteral("/resource-manager-XXXXXX"));
    require(scratch.isValid(), "create manager fixture root");
    const auto file_path = scratch.path() + QStringLiteral("/allowed.txt");
    write_file(file_path, "local approved bytes");
    auto policy = qml_policy();
    require(policy.allow_local_root(scratch.path()), "register manager fixture root");
    QQmlEngine engine;
    const auto stats = pgp::resources::install_resource_guards(engine, std::move(policy));
    auto* manager = engine.networkAccessManager();
    require(manager != nullptr, "factory returns real manager");
    require(manager->redirectPolicy() == QNetworkRequest::ManualRedirectPolicy, "disable automatic redirects");
    QNetworkRequest local(QUrl(QStringLiteral("qrc:/pgp/tests/allowed.txt")));
    local.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* accepted = manager->get(local);
    require(!accepted->isFinished(), "local replies complete asynchronously");
    require(wait_until([&] { return accepted->isFinished(); }), "local read completion");
    require(accepted->error() == QNetworkReply::NoError, "approved local read succeeds");
    require(accepted->readAll().contains("Original offline"), "approved local bytes arrive");
    require(accepted->request().attribute(QNetworkRequest::RedirectPolicyAttribute).toInt()
                == QNetworkRequest::ManualRedirectPolicy, "request cannot override redirect policy");
    accepted->deleteLater();
    auto* file_reply = manager->get(QNetworkRequest(QUrl::fromLocalFile(file_path)));
    require(wait_until([&] { return file_reply->isFinished(); }), "file read completion");
    require(file_reply->error() == QNetworkReply::NoError && file_reply->readAll() == "local approved bytes",
            "approved canonical file reads work");
    file_reply->deleteLater();
    // A would-be redirect target must pass the same deny decision. The manager
    // only serves QFile replies, so there is no HTTP response/redirect transport.
    for (const auto* forbidden : {"https://example.invalid/redirect-target", "http://127.0.0.1/never-connect",
                                  "file://host/remote", "qrc:/private.txt"}) {
        auto* reply = manager->get(QNetworkRequest(QUrl(QString::fromLatin1(forbidden))));
        require(!reply->isFinished(), "denied replies complete asynchronously");
        require(wait_until([&] { return reply->isFinished(); }), "denied request completion");
        require(reply->error() == QNetworkReply::ContentAccessDenied, "deny request before transport");
        require(reply->readAll().isEmpty(), "denied reply has no data");
        reply->deleteLater();
    }
    auto* post = manager->post(local, QByteArray("no local writes"));
    require(wait_until([&] { return post->isFinished(); }), "post denial completion");
    require(post->error() == QNetworkReply::ContentAccessDenied, "local resources are read-only");
    post->deleteLater();
    require(stats->local_requests == 2 && stats->denied_requests == 5, "manager counters prove decisions");
}

void test_qml() {
    QTemporaryDir scratch(QDir::currentPath() + QStringLiteral("/resource-qml-XXXXXX"));
    require(scratch.isValid(), "create QML fixture root");
    const auto approved = scratch.path() + QStringLiteral("/approved");
    require(QDir().mkdir(approved), "create approved QML fixture root");
    const auto approved_image = approved + QStringLiteral("/local.png");
    const auto outside_image = scratch.path() + QStringLiteral("/outside.png");
    QImage image(2, 2, QImage::Format_RGBA8888);
    image.fill(Qt::blue);
    require(image.save(approved_image) && image.save(outside_image), "create original local PNGs");
    auto escaped_image = approved + QStringLiteral("/escape.png");
    if (!QFile::link(outside_image, escaped_image)) {
        escaped_image = outside_image;
    }
    const auto approved_qml = approved + QStringLiteral("/Local.qml");
    write_file(approved_qml, "import QtQml\nQtObject { objectName: \"approved-file-qml\" }");
    auto policy = qml_policy();
    require(policy.allow_local_root(approved), "register QML selected root");
    require(policy.allow_image_provider(QStringLiteral("trusted")), "register QML provider");
    QQmlEngine engine;
    engine.addImageProvider(QStringLiteral("trusted"), new FixtureImageProvider);
    const auto stats = pgp::resources::install_resource_guards(engine, std::move(policy));
    QByteArray source = R"QML(
import QtQuick
Item {
    property bool requestDone: false
    property int remoteImageStatus: remoteImage.status
    property int localImageStatus: localImage.status
    property int fileImageStatus: fileImage.status
    property int escapedImageStatus: escapedImage.status
    property int providerImageStatus: providerImage.status
    property int unknownProviderStatus: unknownProvider.status
    property int remoteFontStatus: remoteFont.status
    Image { id: localImage; source: "qrc:/pgp/tests/checker.png" }
    Image { id: fileImage; source: "@LOCAL_IMAGE@" }
    Image { id: escapedImage; source: "@ESCAPED_IMAGE@" }
    Image { id: providerImage; source: "image://trusted/checker" }
    Image { id: unknownProvider; source: "image://unknown/checker" }
    Image { id: remoteImage; source: "https://example.invalid/image.png" }
    FontLoader { id: remoteFont; source: "https://example.invalid/font.ttf" }
    Component.onCompleted: {
        var request = new XMLHttpRequest();
        request.onreadystatechange = function() {
            if (request.readyState === XMLHttpRequest.DONE) requestDone = true;
        };
        request.open("GET", "https://example.invalid/request");
        request.send();
    }
}
)QML";
    source.replace("@LOCAL_IMAGE@", QUrl::fromLocalFile(approved_image).toEncoded());
    source.replace("@ESCAPED_IMAGE@", QUrl::fromLocalFile(escaped_image).toEncoded());
    auto object = create_component(engine, source);
    require(wait_until([&] {
        return object->property("requestDone").toBool()
            && object->property("remoteImageStatus").toInt() == 3
            && object->property("remoteFontStatus").toInt() == 3
            && object->property("escapedImageStatus").toInt() == 3
            && object->property("unknownProviderStatus").toInt() == 3;
    }), "QML remote image/font/XHR finish with errors");
    require(object->property("localImageStatus").toInt() == 1, "QML local PNG succeeds");
    require(object->property("fileImageStatus").toInt() == 1, "QML approved file PNG succeeds");
    require(object->property("providerImageStatus").toInt() == 1, "QML registered provider succeeds");
    require(stats->denied_urls > 0 && stats->denied_requests > 0, "QML uses installed guards");

    QQmlComponent remote(&engine, QUrl(QStringLiteral("https://example.invalid/Remote.qml")));
    require(wait_until([&] { return !remote.isLoading(); }), "remote QML denied completion");
    require(remote.isError(), "remote QML cannot load");
    QQmlComponent imported(&engine);
    imported.setData("import QtQml\nimport \"https://example.invalid/imports\" as Remote\nRemote.Ghost {}",
                     QUrl(QStringLiteral("qrc:/pgp/tests/import.qml")));
    require(wait_until([&] { return !imported.isLoading(); }), "remote import denied completion");
    require(imported.isError(), "remote QML import cannot load");
    QQmlComponent local(&engine, QUrl(QStringLiteral("qrc:/pgp/tests/Local.qml")));
    require(wait_until([&] { return !local.isLoading(); }), "local QML completes");
    auto local_object = std::unique_ptr<QObject>(local.create());
    require(local_object != nullptr && local_object->objectName() == QStringLiteral("approved-local-qml"),
            "bundled QML remains functional");
    QQmlComponent file_component(&engine, QUrl::fromLocalFile(approved_qml));
    require(wait_until([&] { return !file_component.isLoading(); }), "approved file QML completes");
    auto file_object = std::unique_ptr<QObject>(file_component.create());
    require(file_object != nullptr && file_object->objectName() == QStringLiteral("approved-file-qml"),
            "selected canonical QML root remains functional");
}

} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    try {
        test_policy();
        test_manager();
        test_qml();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Offline policy, local replies and actual QML deny paths passed.\n";
    return 0;
}
