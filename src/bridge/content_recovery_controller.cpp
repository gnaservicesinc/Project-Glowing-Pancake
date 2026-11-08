#include "campaign_controller.hpp"
#include <pgp/persistence/sha256.hpp>
#include <QFileInfo>

namespace pgp::bridge {
bool CampaignController::canRestoreBundledContent() const {
    return needsOriginalContent() && snapshot_->missing_content_hash==persistence::sha256(program_.canonical_content_bytes);
}
QString CampaignController::campaignContentVersion() const {return QString::fromStdString(activeProgram().collection.content_version);}
void CampaignController::reviewOriginalContent(QUrl source) {
    try {
        if(!source.isEmpty() && (!source.isLocalFile() || !source.host().isEmpty() || source.hasQuery() || source.hasFragment() || !QFileInfo{source.toLocalFile()}.isAbsolute()))
            throw std::invalid_argument("Choose a local content collection.");
        const auto path=source.isEmpty()?std::filesystem::path{}:std::filesystem::path{source.toLocalFile().toStdU16String()};
        if(!beginOperation(true)) return;
        original_content_review_.clear();
        QMetaObject::invokeMethod(worker_,[worker=worker_,token=token_,path]{worker->reviewOriginalContent(token,path);},Qt::QueuedConnection);
    }catch(const std::exception& error){setNotice(QString::fromUtf8(error.what()));}
}
void CampaignController::confirmOriginalContent() {
    if(original_content_review_.isEmpty() || !beginOperation(true)) return;
    original_content_review_.clear();
    QMetaObject::invokeMethod(worker_,[worker=worker_,token=token_]{worker->confirmOriginalContent(token);},Qt::QueuedConnection);
}
void CampaignController::cancelOriginalContent() {
    original_content_review_.clear();emit stateChanged();
    QMetaObject::invokeMethod(worker_,[worker=worker_]{worker->cancelOriginalContent();},Qt::QueuedConnection);
}
}
