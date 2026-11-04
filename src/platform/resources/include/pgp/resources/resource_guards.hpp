#pragma once

#include <QSet>
#include <QStringList>
#include <QUrl>

#include <atomic>
#include <memory>

class QQmlEngine;

namespace pgp::resources {

// Build this allowlist before installing it. Installation freezes a copy shared
// by the QML loader and its per-thread network managers. File roots are read-only.
class ResourcePolicy {
public:
    ResourcePolicy();

    [[nodiscard]] bool allow_local_root(const QString& directory);
    [[nodiscard]] bool allow_resource_prefix(const QString& prefix);
    [[nodiscard]] bool allow_image_provider(const QString& identifier);
    [[nodiscard]] bool allows(const QUrl& url) const;

private:
    QStringList local_roots_;
    QStringList resource_prefixes_;
    QSet<QString> image_providers_;
};

// Bounded counters for local qualification. No URLs, player data or disk writes.
struct ResourceGuardStats {
    std::atomic_size_t denied_urls{0};
    std::atomic_size_t denied_requests{0};
    std::atomic_size_t local_requests{0};
};

// Call once, before loading QML or requesting the engine's network manager.
// The engine owns both guards; they remain alive throughout engine destruction.
// This guards QML loading/requests, not arbitrary native socket APIs.
[[nodiscard]] std::shared_ptr<ResourceGuardStats> install_resource_guards(
    QQmlEngine& engine, ResourcePolicy policy);

} // namespace pgp::resources
