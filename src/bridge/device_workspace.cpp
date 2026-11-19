#include "world_controller.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QScreen>
#include <algorithm>

namespace pgp::bridge {
namespace {
QString q(const std::string& s) { return QString::fromStdString(s); }
bool has_devices(const CampaignSnapshotPtr& p) {
    return p && p->checkpoint.world.simulation.infrastructure &&
           p->checkpoint.world.simulation.infrastructure->workplace &&
           p->checkpoint.world.simulation.infrastructure->workplace->devices;
}
bool valid_app_id(const QString& app) {
    static const QStringList kAllowed{
        "devices", "office", "lab", "people", "facilities", "inbox", "operations",
        "civic", "society", "institutions", "frontier", "leadership", "objectives",
        "research", "data", "models", "accounts", "products", "messages", "world", "personal"
    };
    return kAllowed.contains(app);
}
QVariantMap empty_workspace() {
    return {{"openApps", QStringList{}}, {"activeApp", QString{}}, {"poppedOut", false}, {"sessions", QVariantMap{}}};
}
QVariantMap sanitize_workspace(const QVariantMap& raw) {
    QVariantMap out = empty_workspace();
    QStringList open;
    for (const auto& entry : raw.value("openApps").toStringList()) {
        if (valid_app_id(entry) && !open.contains(entry) && open.size() < 32) open.push_back(entry);
    }
    const auto active = raw.value("activeApp").toString();
    out["openApps"] = open;
    out["activeApp"] = open.contains(active) ? active : QString{};
    out["poppedOut"] = raw.value("poppedOut", false).toBool();
    QVariantMap sessions;
    const auto raw_sessions = raw.value("sessions").toMap();
    for (auto it = raw_sessions.begin(); it != raw_sessions.end(); ++it) {
        if (!valid_app_id(it.key()) || sessions.size() >= 32) continue;
        const auto m = it.value().toMap();
        QVariantMap clean;
        for (auto sit = m.begin(); sit != m.end(); ++sit) {
            if (sit.key().size() <= 96 && clean.size() < 64) clean[sit.key()] = sit.value();
        }
        sessions[it.key()] = clean;
    }
    out["sessions"] = sessions;
    return out;
}
} // namespace

void WorldController::restoreWorkspaces() {
    device_workspaces_.clear();
    window_geometries_.clear();
    detached_research_tree_ = {{"open", false}, {"deviceId", QString{}}};
    if (!path_.empty()) {
        QFile f{QString::fromStdU16String(path_.u16string()) + QStringLiteral(".workspaces.json")};
        if (f.open(QIODevice::ReadOnly) && f.size() <= 512 * 1024) {
            const auto doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject()) {
                const auto root = doc.object().toVariantMap();
                const auto devs = root.value("devices").toMap();
                for (auto it = devs.begin(); it != devs.end(); ++it) {
                    if (it.key().size() <= 48 && device_workspaces_.size() < 64 &&
                        (it.key().startsWith("personal/") || it.key().startsWith("hq/"))) {
                        device_workspaces_[it.key()] = sanitize_workspace(it.value().toMap());
                    }
                }
                const auto wins = root.value("windows").toMap();
                for (auto it = wins.begin(); it != wins.end(); ++it) {
                    if (it.key().size() <= 64 && window_geometries_.size() < 64) {
                        const auto g = it.value().toMap();
                        if (g.contains("w") && g.contains("h")) {
                            window_geometries_[it.key()] = QVariantMap{
                                {"x", g.value("x").toInt()},
                                {"y", g.value("y").toInt()},
                                {"w", std::clamp(g.value("w").toInt(), 360, 7680)},
                                {"h", std::clamp(g.value("h").toInt(), 300, 4320)}
                            };
                        }
                    }
                }
                const auto tree = root.value("detachedResearchTree").toMap();
                detached_research_tree_["open"] = tree.value("open", false).toBool();
                detached_research_tree_["deviceId"] = tree.value("deviceId").toString().left(48);
            }
        }
    }
    emit workspacesChanged();
}

bool WorldController::saveWorkspaces() {
    if (!snapshot_ || !snapshot_->current_campaign || snapshot_->storage_blocked || path_.empty()) return true;
    if (!QDir{}.mkpath(QString::fromStdU16String(directory_.u16string()))) {
        emit noticeRaised(tr("Could not save device workspace state. Check local storage."));
        return false;
    }
    QVariantMap root{
        {"version", 1},
        {"devices", device_workspaces_},
        {"windows", window_geometries_},
        {"detachedResearchTree", detached_research_tree_}
    };
    const auto bytes = QJsonDocument{QJsonObject::fromVariantMap(root)}.toJson(QJsonDocument::Compact);
    if (bytes.size() > 512 * 1024) {
        emit noticeRaised(tr("Device workspace storage limit reached. Existing workspaces were preserved."));
        return false;
    }
    QSaveFile f{QString::fromStdU16String(path_.u16string()) + QStringLiteral(".workspaces.json")};
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit()) {
        emit noticeRaised(tr("Could not save device workspace state. Check local storage."));
        return false;
    }
    return true;
}

QString WorldController::canonicalPhysicalDeviceId(const QString& idOrKind, const QString& appId) const {
    if (!has_devices(snapshot_)) return {};
    const auto& s = snapshot_->checkpoint.world.simulation;
    const bool hq = atHeadquarters();
    const auto all = core::physical_devices(s, hq);
    if (idOrKind.startsWith("personal/") || idOrKind.startsWith("hq/")) {
        const auto target = idOrKind.toStdString();
        for (const auto& info : all) if (info.device_id == target) return idOrKind;
        return {};
    }
    if (!idOrKind.isEmpty()) {
        const auto kind = idOrKind.toStdString();
        const auto app = appId.toStdString();
        for (const auto& info : all) {
            if (info.kind == kind && info.available &&
                (app.empty() || core::physical_device_supports_app(s, info.device_id, app, hq)))
                return q(info.device_id);
        }
        for (const auto& info : all) {
            if (info.kind == kind && info.available) return q(info.device_id);
        }
        for (const auto& info : all) {
            if (info.kind == kind) return q(info.device_id);
        }
    }
    const auto resolved = core::resolve_physical_app_device(s, {}, appId.toStdString(), hq);
    if (!resolved.empty()) return q(resolved);
    return all.empty() ? QString{} : q(all.front().device_id);
}

QString WorldController::resolvePhysicalDevice(QString preferredDeviceId, QString appId) const {
    return canonicalPhysicalDeviceId(preferredDeviceId, appId);
}

QVariantList WorldController::physicalDevices() const {
    QVariantList rows;
    if (!has_devices(snapshot_)) return rows;
    const auto& s = snapshot_->checkpoint.world.simulation;
    const bool archive = companyArchive();
    for (const auto& info : core::physical_devices(s, atHeadquarters())) {
        const auto dev_id = q(info.device_id);
        const auto ws = device_workspaces_.contains(dev_id) ? device_workspaces_.value(dev_id).toMap() : empty_workspace();
        QStringList apps;
        for (const auto& a : info.apps) apps.push_back(q(a));
        const auto open_apps = ws.value("openApps").toStringList();
        rows.push_back(QVariantMap{
            {"deviceId", dev_id},
            {"id", dev_id},
            {"kind", q(info.kind)},
            {"numericId", static_cast<qulonglong>(info.numeric_id)},
            {"name", q(info.name)},
            {"owner", q(info.owner)},
            {"ownerLabel", info.owner == "protagonist" ? tr("Personal") : tr("Company")},
            {"location", q(info.location)},
            {"portable", info.portable},
            {"powered", info.powered},
            {"available", info.available || archive},
            {"canPower", !archive && info.can_power},
            {"canService", !archive && info.can_service},
            {"canRetire", !archive && info.can_retire},
            {"unavailabilityReason", archive ? QString{} : q(info.unavailability_reason)},
            {"apps", apps},
            {"openApps", open_apps},
            {"activeApp", ws.value("activeApp").toString()},
            {"poppedOut", ws.value("poppedOut", false).toBool()},
            {"hasOpenApps", !open_apps.isEmpty()},
            {"revision", reviewToken()}
        });
    }
    return rows;
}

QVariantMap WorldController::physicalDevice(QString deviceId) const {
    const auto resolved = canonicalPhysicalDeviceId(deviceId);
    for (const auto& row : physicalDevices()) {
        const auto m = row.toMap();
        if (m.value("deviceId").toString() == resolved) return m;
    }
    return {};
}

QVariantList WorldController::poppedOutDevices() const {
    QVariantList out;
    for (const auto& row : physicalDevices()) {
        if (row.toMap().value("poppedOut").toBool()) out.push_back(row);
    }
    return out;
}

QVariantMap WorldController::detachedResearchTree() const {
    const bool open = detached_research_tree_.value("open", false).toBool();
    const auto dev_id = detached_research_tree_.value("deviceId").toString();
    const auto dev = dev_id.isEmpty() ? QVariantMap{} : physicalDevice(dev_id);
    return {
        {"open", open && !dev.isEmpty()},
        {"deviceId", dev.value("deviceId").toString()},
        {"deviceName", dev.value("name").toString()},
        {"available", dev.value("available", false).toBool()},
        {"unavailabilityReason", dev.value("unavailabilityReason").toString()}
    };
}

QVariantMap WorldController::deviceWorkspace(QString deviceId) const {
    const auto resolved = canonicalPhysicalDeviceId(deviceId);
    if (resolved.isEmpty()) return empty_workspace();
    return device_workspaces_.contains(resolved) ? device_workspaces_.value(resolved).toMap() : empty_workspace();
}

bool WorldController::openDeviceApp(QString deviceId, QString appId, QString section) {
    if (!snapshot_ || !valid_app_id(appId)) return false;
    const auto resolved = canonicalPhysicalDeviceId(deviceId, appId);
    if (resolved.isEmpty()) return false;
    if (has_devices(snapshot_) && !companyArchive()) {
        const auto& s = snapshot_->checkpoint.world.simulation;
        if (!core::physical_device_supports_app(s, resolved.toStdString(), appId.toStdString(), atHeadquarters()))
            return false;
    }
    auto ws = device_workspaces_.contains(resolved) ? device_workspaces_.value(resolved).toMap() : empty_workspace();
    auto open_apps = ws.value("openApps").toStringList();
    if (!open_apps.contains(appId) && open_apps.size() < 32) open_apps.push_back(appId);
    ws["openApps"] = open_apps;
    ws["activeApp"] = appId;
    if (!section.isEmpty()) {
        auto sessions = ws.value("sessions").toMap();
        auto state = sessions.value(appId).toMap();
        state["section"] = section.left(48);
        sessions[appId] = state;
        ws["sessions"] = sessions;
    }
    device_workspaces_[resolved] = ws;
    saveWorkspaces();
    emit workspacesChanged();
    return true;
}

bool WorldController::switchDeviceApp(QString deviceId, QString appId) {
    if (!snapshot_ || !valid_app_id(appId)) return false;
    const auto resolved = canonicalPhysicalDeviceId(deviceId, appId);
    if (resolved.isEmpty()) return false;
    auto ws = device_workspaces_.contains(resolved) ? device_workspaces_.value(resolved).toMap() : empty_workspace();
    const auto open_apps = ws.value("openApps").toStringList();
    if (!open_apps.contains(appId)) return openDeviceApp(resolved, appId, {});
    ws["activeApp"] = appId;
    device_workspaces_[resolved] = ws;
    saveWorkspaces();
    emit workspacesChanged();
    return true;
}

void WorldController::suspendDeviceApp(QString deviceId) {
    const auto resolved = canonicalPhysicalDeviceId(deviceId);
    if (resolved.isEmpty() || !device_workspaces_.contains(resolved)) return;
    auto ws = device_workspaces_.value(resolved).toMap();
    if (ws.value("activeApp").toString().isEmpty()) return;
    ws["activeApp"] = QString{};
    device_workspaces_[resolved] = ws;
    saveWorkspaces();
    emit workspacesChanged();
}

void WorldController::closeDeviceApp(QString deviceId, QString appId) {
    const auto resolved = canonicalPhysicalDeviceId(deviceId);
    if (resolved.isEmpty() || !device_workspaces_.contains(resolved)) return;
    auto ws = device_workspaces_.value(resolved).toMap();
    auto open_apps = ws.value("openApps").toStringList();
    open_apps.removeAll(appId);
    auto sessions = ws.value("sessions").toMap();
    sessions.remove(appId);
    ws["openApps"] = open_apps;
    ws["sessions"] = sessions;
    if (ws.value("activeApp").toString() == appId)
        ws["activeApp"] = open_apps.isEmpty() ? QString{} : open_apps.back();
    device_workspaces_[resolved] = ws;
    if (detached_research_tree_.value("open").toBool() &&
        detached_research_tree_.value("deviceId").toString() == resolved &&
        (appId == "lab" || appId == "research")) {
        detached_research_tree_["open"] = false;
    }
    saveWorkspaces();
    emit workspacesChanged();
}

QVariantMap WorldController::deviceAppState(QString deviceId, QString appId) const {
    const auto resolved = canonicalPhysicalDeviceId(deviceId);
    if (resolved.isEmpty() || !device_workspaces_.contains(resolved)) return {};
    return device_workspaces_.value(resolved).toMap().value("sessions").toMap().value(appId).toMap();
}

void WorldController::setDeviceAppState(QString deviceId, QString appId, QString key, QVariant value) {
    if (!snapshot_ || !valid_app_id(appId) || key.isEmpty() || key.size() > 96) return;
    const auto resolved = canonicalPhysicalDeviceId(deviceId, appId);
    if (resolved.isEmpty()) return;
    auto ws = device_workspaces_.contains(resolved) ? device_workspaces_.value(resolved).toMap() : empty_workspace();
    auto sessions = ws.value("sessions").toMap();
    auto state = sessions.value(appId).toMap();
    if (state.value(key) == value) return;
    if (state.size() >= 64 && !state.contains(key)) return;
    state[key] = std::move(value);
    sessions[appId] = state;
    ws["sessions"] = sessions;
    device_workspaces_[resolved] = ws;
    saveWorkspaces();
    emit workspacesChanged();
}

bool WorldController::setDevicePoppedOut(QString deviceId, bool poppedOut) {
    if (!snapshot_) return false;
    const auto resolved = canonicalPhysicalDeviceId(deviceId);
    if (resolved.isEmpty()) return false;
    auto ws = device_workspaces_.contains(resolved) ? device_workspaces_.value(resolved).toMap() : empty_workspace();
    if (ws.value("poppedOut", false).toBool() == poppedOut) return true;
    ws["poppedOut"] = poppedOut;
    device_workspaces_[resolved] = ws;
    saveWorkspaces();
    emit workspacesChanged();
    return true;
}

bool WorldController::openDetachedResearchTree(QString deviceId) {
    if (!snapshot_) return false;
    const auto resolved = canonicalPhysicalDeviceId(deviceId, "lab");
    if (resolved.isEmpty()) return false;
    if (!openDeviceApp(resolved, "lab", "research")) return false;
    detached_research_tree_ = {{"open", true}, {"deviceId", resolved}};
    saveWorkspaces();
    emit workspacesChanged();
    return true;
}

void WorldController::closeDetachedResearchTree() {
    if (!detached_research_tree_.value("open", false).toBool()) return;
    detached_research_tree_["open"] = false;
    saveWorkspaces();
    emit workspacesChanged();
}

QVariantList WorldController::alternateDevicesForApp(QString appId, QString excludeDeviceId) const {
    QVariantList out;
    if (!has_devices(snapshot_)) return out;
    const auto& s = snapshot_->checkpoint.world.simulation;
    const bool hq = atHeadquarters();
    for (const auto& row : physicalDevices()) {
        const auto m = row.toMap();
        const auto id = m.value("deviceId").toString();
        if (id == excludeDeviceId || !m.value("available").toBool()) continue;
        if (appId.isEmpty() || appId == "devices" ||
            core::physical_device_supports_app(s, id.toStdString(), appId.toStdString(), hq)) {
            out.push_back(m);
        }
    }
    return out;
}

QVariantMap WorldController::clampedWindowGeometry(QString windowKey, int defaultW, int defaultH) const {
    int w = std::clamp(defaultW, 520, 3840);
    int h = std::clamp(defaultH, 400, 2160);
    int x = 80;
    int y = 60;
    bool has_saved = false;
    if (window_geometries_.contains(windowKey)) {
        const auto g = window_geometries_.value(windowKey).toMap();
        x = g.value("x", x).toInt();
        y = g.value("y", y).toInt();
        w = std::clamp(g.value("w", w).toInt(), 520, 3840);
        h = std::clamp(g.value("h", h).toInt(), 400, 2160);
        has_saved = true;
    }
    QRect bounds{0, 0, 1280, 720};
    if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) != nullptr) {
        const auto screens = QGuiApplication::screens();
        QScreen* chosen = QGuiApplication::primaryScreen();
        if (has_saved && !screens.isEmpty()) {
            const QRect wanted{x, y, w, h};
            int best_area = 0;
            for (QScreen* screen : screens) {
                if (!screen) continue;
                const auto inter = screen->availableGeometry().intersected(wanted);
                const int area = inter.width() * inter.height();
                if (area > best_area) {
                    best_area = area;
                    chosen = screen;
                }
            }
        }
        if (chosen) bounds = chosen->availableGeometry();
    }
    if (bounds.width() >= 520) w = std::min(w, bounds.width());
    if (bounds.height() >= 400) h = std::min(h, bounds.height());
    if (!has_saved) {
        x = bounds.left() + std::max(0, (bounds.width() - w) / 2);
        y = bounds.top() + std::max(0, (bounds.height() - h) / 2);
    } else {
        const int max_x = std::max(bounds.left(), bounds.right() - w + 1);
        const int max_y = std::max(bounds.top(), bounds.bottom() - h + 1);
        x = std::clamp(x, bounds.left(), max_x);
        y = std::clamp(y, bounds.top(), max_y);
    }
    return {{"x", x}, {"y", y}, {"w", w}, {"h", h}};
}

void WorldController::saveWindowGeometry(QString windowKey, int x, int y, int w, int h) {
    if (!snapshot_ || windowKey.isEmpty() || windowKey.size() > 64) return;
    if (w < 360 || h < 300) return;
    const QVariantMap next{{"x", x}, {"y", y}, {"w", std::clamp(w, 360, 7680)}, {"h", std::clamp(h, 300, 4320)}};
    if (window_geometries_.value(windowKey).toMap() == next) return;
    if (window_geometries_.size() >= 64 && !window_geometries_.contains(windowKey)) return;
    window_geometries_[windowKey] = next;
    saveWorkspaces();
}

void WorldController::powerPhysicalDevice(QString deviceId, bool powered, QString revision) {
    if (!snapshot_ || !has_devices(snapshot_)) return;
    if (deviceId.startsWith("personal/")) {
        bool ok = false;
        const auto id = deviceId.mid(9).toULongLong(&ok);
        if (ok) emit actionRequested(core::SetDevicePower{id, powered});
        return;
    }
    if (deviceId.startsWith("hq/")) {
        if (!revision.isEmpty() && !hqReview(revision)) return;
        bool ok = false;
        const auto id = deviceId.mid(3).toUInt(&ok);
        if (ok) emit actionRequested(core::ManageHeadquarters{core::PowerHQEquipment{id, powered}});
    }
}
} // namespace pgp::bridge
