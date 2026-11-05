#include "asset_controller.hpp"
#include <algorithm>

namespace pgp::studio {
namespace {
QString text(std::string_view v) { return QString::fromUtf8(v.data(),static_cast<qsizetype>(v.size())); }
QVariantMap values(const AssetRecord& r) {
    return {{"id",text(r.id)},{"path",text(r.path)},{"sha256",text(r.sha256)},{"kind",text(r.kind)},
        {"title",text(r.title)},{"creator",text(r.creator)},{"source_url",text(r.source_url)},{"retrieved_on",text(r.retrieved_on)},
        {"license_id",text(r.license_id)},{"license_url",text(r.license_url)},{"license_evidence_path",text(r.license_evidence_path)},
        {"modifications",text(r.modifications)},{"purpose",text(r.purpose)},{"ai_generated",r.ai_generated},
        {"generation_record",text(r.generation_record)},{"credit_text",text(r.credit_text)},{"credit_group",text(r.credit_group)},
        {"review_status",text(r.review_status)}};
}
}
QVariantList AssetController::entries() const {
    QVariantList result;
    if (register_) for(const auto& r:register_->records())
        result.push_back(QVariantMap{{"id",text(r.id)},{"label",text(r.title+" · "+r.id+" · "+r.review_status)}});
    return result;
}
bool AssetController::openRegister(QString checkout) {
    try {
        register_.emplace(std::filesystem::path{checkout.toStdU16String()});
        message_.clear();
        emit changed();
        return true;
    } catch(const std::exception& e) {
        message_=text(e.what());
        emit changed();
        return false;
    }
}
QVariantMap AssetController::record(QString id) const {
    if (register_) {
        const auto key=id.toStdString();
        for(const auto& row:register_->records()) if(row.id==key) return values(row);
    }
    AssetRecord empty;
    empty.kind="image";
    empty.review_status="pending";
    return values(empty);
}
bool AssetController::inspect(QString id) {
    try {
        if (!register_) throw std::invalid_argument("Open a validated checkout's asset register.");
        message_=text(register_->inspect(id.toStdString()));
        emit changed();
        return true;
    } catch(const std::exception& e) {
        message_=text(e.what());
        emit changed();
        return false;
    }
}
bool AssetController::save(QVariantMap fields, QString importPath) {
    try {
        if (!register_) throw std::invalid_argument("Open a validated checkout's asset register.");
        const auto get=[&](const char* key) {return fields.value(QString::fromLatin1(key)).toString().toStdString();};
        AssetRecord row;
        row.id=get("id");
        row.path=get("path");
        row.kind=get("kind");
        row.title=get("title");
        row.creator=get("creator");
        row.source_url=get("source_url");
        row.retrieved_on=get("retrieved_on");
        row.license_id=get("license_id");
        row.license_url=get("license_url");
        row.license_evidence_path=get("license_evidence_path");
        row.modifications=get("modifications");
        row.purpose=get("purpose");
        row.ai_generated=fields.value(QStringLiteral("ai_generated")).toBool();
        row.generation_record=get("generation_record");
        row.credit_text=get("credit_text");
        row.credit_group=get("credit_group");
        row.review_status=get("review_status");
        const auto id=row.id;
        register_->save(std::move(row),std::filesystem::path{importPath.toStdU16String()});
        message_=QStringLiteral("Saved asset record %1. ").arg(text(id))+text(register_->inspect(id));
        emit changed();
        return true;
    } catch(const std::exception& e) {
        message_=text(e.what());
        emit changed();
        return false;
    }
}
} // namespace pgp::studio
