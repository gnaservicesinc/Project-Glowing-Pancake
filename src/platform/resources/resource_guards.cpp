#include <pgp/resources/resource_guards.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQmlAbstractUrlInterceptor>
#include <QQmlEngine>
#include <QQmlNetworkAccessManagerFactory>
#include <QRegularExpression>
#include <QTimer>
#include <QVariant>

#include <stdexcept>
#include <utility>

namespace pgp::resources {
namespace {

bool below_root(const QString& path, const QString& root) {
    const auto relative = QDir(root).relativeFilePath(path);
    return !QDir::isAbsolutePath(relative) && relative != QStringLiteral("..")
        && !relative.startsWith(QStringLiteral("../"));
}

bool has_authority(const QUrl& url) {
    return !url.authority().isEmpty() || url.port() != -1;
}

class GuardedReply final : public QNetworkReply {
public:
    GuardedReply(const QNetworkRequest& request, QNetworkAccessManager::Operation operation,
                 bool allowed, QObject* parent)
        : QNetworkReply(parent), head_only_(operation == QNetworkAccessManager::HeadOperation) {
        setRequest(request);
        setUrl(request.url());
        setOperation(operation);
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);

        if (!allowed) {
            setError(QNetworkReply::ContentAccessDenied, QStringLiteral("Resource denied by offline policy"));
        } else {
            const auto url = request.url();
            file_.setFileName(url.scheme() == QStringLiteral("qrc")
                ? QStringLiteral(":") + url.path() : QFileInfo(url.toLocalFile()).canonicalFilePath());
            if (!file_.open(QIODevice::ReadOnly)) {
                setError(QNetworkReply::ContentNotFoundError, QStringLiteral("Local resource cannot be read"));
            } else {
                setHeader(QNetworkRequest::ContentLengthHeader, file_.size());
            }
        }
        // Consumers attach signals after get() returns. Never finish inline.
        QTimer::singleShot(0, this, [this] {
            if (isFinished()) {
                return;
            }
            setFinished(true);
            if (error() != QNetworkReply::NoError) {
                emit errorOccurred(error());
            } else {
                emit metaDataChanged();
                if (!head_only_ && file_.bytesAvailable() > 0) {
                    emit readyRead();
                }
            }
            emit finished();
        });
    }

    void abort() override {
        if (isFinished()) {
            return;
        }
        file_.close();
        setError(QNetworkReply::OperationCanceledError, QStringLiteral("Local resource request canceled"));
        setFinished(true);
        emit errorOccurred(error());
        emit finished();
    }

    [[nodiscard]] bool isSequential() const override { return true; }
    [[nodiscard]] qint64 bytesAvailable() const override {
        return (head_only_ ? 0 : file_.bytesAvailable()) + QNetworkReply::bytesAvailable();
    }

protected:
    qint64 readData(char* data, qint64 maximum) override {
        if (head_only_ || !file_.isOpen()) {
            return -1;
        }
        return file_.read(data, maximum);
    }

private:
    QFile file_;
    bool head_only_;
};

class GuardedNetworkManager final : public QNetworkAccessManager {
public:
    GuardedNetworkManager(std::shared_ptr<const ResourcePolicy> policy,
                          std::shared_ptr<ResourceGuardStats> stats, QObject* parent)
        : QNetworkAccessManager(parent), policy_(std::move(policy)), stats_(std::move(stats)) {
        setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
    }

protected:
    QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request,
                                 QIODevice* outgoing_data) override {
        Q_UNUSED(outgoing_data);
        const auto scheme = request.url().scheme();
        const bool allowed = (operation == GetOperation || operation == HeadOperation)
            && (scheme == QStringLiteral("file") || scheme == QStringLiteral("qrc"))
            && policy_->allows(request.url());
        if (allowed) {
            ++stats_->local_requests;
        } else {
            ++stats_->denied_requests;
        }
        auto guarded_request = request;
        guarded_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                     QNetworkRequest::ManualRedirectPolicy);
        // Do not call QNetworkAccessManager::createRequest, even for local URLs.
        // QFile replies have no HTTP redirect, DNS, proxy or socket path.
        return new GuardedReply(guarded_request, operation, allowed, this);
    }

private:
    std::shared_ptr<const ResourcePolicy> policy_;
    std::shared_ptr<ResourceGuardStats> stats_;
};

class GuardedNetworkFactory final : public QQmlNetworkAccessManagerFactory {
public:
    GuardedNetworkFactory(std::shared_ptr<const ResourcePolicy> policy,
                          std::shared_ptr<ResourceGuardStats> stats)
        : policy_(std::move(policy)), stats_(std::move(stats)) {}

    QNetworkAccessManager* create(QObject* parent) override {
        return new GuardedNetworkManager(policy_, stats_, parent);
    }

private:
    std::shared_ptr<const ResourcePolicy> policy_;
    std::shared_ptr<ResourceGuardStats> stats_;
};

class GuardedUrlInterceptor final : public QQmlAbstractUrlInterceptor {
public:
    GuardedUrlInterceptor(std::shared_ptr<const ResourcePolicy> policy,
                          std::shared_ptr<ResourceGuardStats> stats)
        : policy_(std::move(policy)), stats_(std::move(stats)) {}

    QUrl intercept(const QUrl& url, DataType type) override {
        Q_UNUSED(type);
        if (policy_->allows(url)) {
            return url;
        }
        ++stats_->denied_urls;
        // A private non-transport scheme also preserves asynchronous semantics
        // for denied remote imports; the factory returns an asynchronous error.
        return QUrl(QStringLiteral("pgp-denied:/blocked"));
    }

private:
    std::shared_ptr<const ResourcePolicy> policy_;
    std::shared_ptr<ResourceGuardStats> stats_;
};

class GuardOwner final : public QObject {
public:
    GuardOwner(QQmlEngine& engine, std::shared_ptr<const ResourcePolicy> policy,
               const std::shared_ptr<ResourceGuardStats>& stats)
        : QObject(&engine), interceptor_(policy, stats), factory_(std::move(policy), stats) {
        engine.setNetworkAccessManagerFactory(&factory_);
        engine.addUrlInterceptor(&interceptor_);
    }

private:
    GuardedUrlInterceptor interceptor_;
    GuardedNetworkFactory factory_;
};

} // namespace

ResourcePolicy::ResourcePolicy() : resource_prefixes_{QStringLiteral("/pgp/")} {}

bool ResourcePolicy::allow_local_root(const QString& directory) {
    const QFileInfo info(directory);
    const auto canonical = info.canonicalFilePath();
    if (!info.isDir() || canonical.isEmpty() || canonical.startsWith(QStringLiteral("//"))) {
        return false;
    }
    if (!local_roots_.contains(canonical)) {
        local_roots_.append(canonical);
    }
    return true;
}

bool ResourcePolicy::allow_resource_prefix(const QString& prefix) {
    const auto clean = QDir::cleanPath(prefix);
    if (!prefix.startsWith('/') || clean == QStringLiteral("/") || clean.contains('\\')) {
        return false;
    }
    const auto normalized = clean + '/';
    if (!resource_prefixes_.contains(normalized)) {
        resource_prefixes_.append(normalized);
    }
    return true;
}

bool ResourcePolicy::allow_image_provider(const QString& identifier) {
    static const QRegularExpression valid(QStringLiteral("^[a-z][a-z0-9_-]*$"));
    if (!valid.match(identifier).hasMatch()) {
        return false;
    }
    image_providers_.insert(identifier);
    return true;
}

bool ResourcePolicy::allows(const QUrl& url) const {
    if (!url.isValid() || url.isRelative() || !url.userInfo().isEmpty() || url.port() != -1) {
        return false;
    }
    const auto scheme = url.scheme();
    if (scheme == QStringLiteral("image")) {
        return image_providers_.contains(url.host());
    }
    if (has_authority(url) || url.path().startsWith(QStringLiteral("//")) || url.path().contains('\\')) {
        return false;
    }
    if (scheme == QStringLiteral("qrc")) {
        const auto path = QDir::cleanPath(url.path());
        for (const auto& prefix : resource_prefixes_) {
            if (path.startsWith(prefix)) {
                return true;
            }
        }
        return false;
    }
    if (scheme == QStringLiteral("file")) {
        const auto canonical = QFileInfo(url.toLocalFile()).canonicalFilePath();
        if (canonical.isEmpty()) {
            return false;
        }
        for (const auto& root : local_roots_) {
            if (below_root(canonical, root)) {
                return true;
            }
        }
    }
    return false;
}

std::shared_ptr<ResourceGuardStats> install_resource_guards(QQmlEngine& engine, ResourcePolicy policy) {
    constexpr auto installed = "pgp.resourceGuardsInstalled";
    if (engine.property(installed).toBool()) {
        throw std::logic_error("Resource guards must be installed only once per QML engine");
    }
    auto stats = std::make_shared<ResourceGuardStats>();
    new GuardOwner(engine, std::make_shared<const ResourcePolicy>(std::move(policy)), stats);
    engine.setProperty(installed, true);
    return stats;
}

} // namespace pgp::resources
