#include "campaign_worker.hpp"
#include <pgp/persistence/sha256.hpp>
#include <QLocale>
#include <limits>

namespace pgp::bridge {
void CampaignWorker::cancelOriginalContent() {requireOwningThread();content_review_.reset();}
void CampaignWorker::reviewOriginalContent(std::uint64_t token,std::filesystem::path source) {
    requireOwningThread();content_review_.reset();
    try {
        if(missing_content_hash_.empty() || content_target_.empty() || !writer_lock_ || !writer_lock_->isLocked())
            throw std::runtime_error("Choose a company that needs its original content before reviewing a restoration.");
        auto selected=installed_;
        if(!source.empty()) {
            const auto registry=content::foundation_registry();
            selected=std::make_shared<const runtime::CompiledProgram>(runtime::compile_collection(content::load_collection(
                persistence::read_bounded(source,content::maximum_document_bytes),registry),registry));
        }
        ContentReview review;review.source=selected;review.target=content_target_;review.current=content_target_current_;
        if(review.current) {
            review.continuing=std::make_unique<persistence::CurrentCampaign>(paths_.saves_directory/"continuing");
            review.fingerprint=review.continuing->fingerprint();
            auto checkpoint=review.continuing->resume(*selected);
            if(!checkpoint) throw std::runtime_error("The continuing company is missing.");
            review.checkpoint=std::move(*checkpoint);
            if(review.fingerprint!=review.continuing->fingerprint()) throw std::runtime_error("The company changed during review. Review its current files again.");
        } else {
            review.fingerprint=persistence::sha256(persistence::read_bounded(review.target));
            review.checkpoint=persistence::load_file(*selected,review.target).checkpoint;
            if(review.fingerprint!=persistence::sha256(persistence::read_bounded(review.target)))
                throw std::runtime_error("The save changed during review. Review its current file again.");
        }
        requireCampaignMode(review.checkpoint);
        const auto& world=review.checkpoint.world;
        const auto name=world.identity?QString::fromStdString(world.identity->company_name):tr("Saved company");
        const auto raw=world.simulation.cash.value();
        const auto magnitude=raw<0?static_cast<std::uint64_t>(-(raw+1))+1:static_cast<std::uint64_t>(raw);
        const auto cash=(raw<0?QStringLiteral("−$"):QStringLiteral("$"))+
            QLocale{QLocale::English,QLocale::UnitedStates}.toString(static_cast<qulonglong>(magnitude/100))+QStringLiteral(".%1").arg(magnitude%100,2,10,QLatin1Char('0'));
        const auto copy=tr("Resume %1 on %2\nCompany cash: %3 (2014 dollars)\nOriginal rules: %4\n\nKeeps the recorded accounts, people, obligations and unfinished decisions. The clock stops at this saved morning. Future new companies use the installed rules.\n\n%5\nAny unusable cached rules file is preserved before its verified replacement.")
            .arg(name,QString::fromStdString(core::iso_date(runtime::campaign_day(world))),cash,QString::fromStdString(selected->collection.content_version),
                review.current?tr("The current company and day journal remain in place."):tr("This saved point becomes the current company. Existing continuing files are preserved in a recovery folder. Any newer in-memory changes are replaced by this saved point."));
        content_review_=std::move(review);emit originalContentReviewReady(copy);
        emit operationCompleted(static_cast<qulonglong>(token),true,{});
    } catch(const std::exception& error) {
        const auto message=QString::fromUtf8(error.what());emit noticeRaised(message);emit operationCompleted(static_cast<qulonglong>(token),false,message);
    }
}
void CampaignWorker::confirmOriginalContent(std::uint64_t token) {
    requireOwningThread();
    try {
        if(!content_review_ || !writer_lock_ || !writer_lock_->isLocked()) throw std::runtime_error("Review the original content before restoring this company.");
        if(generation_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("Campaign generation is exhausted");
        auto review=std::move(*content_review_);content_review_.reset();
        if(missing_content_hash_.empty() || content_target_!=review.target || content_target_current_!=review.current)
            throw std::runtime_error("The recovery target changed. Review the company again.");
        const auto current=review.current?review.continuing->fingerprint():persistence::sha256(persistence::read_bounded(review.target));
        if(current!=review.fingerprint) throw std::runtime_error("The saved company changed. Review its current files again.");
        session::Campaign next{*review.source,review.checkpoint,paths_.saves_directory/"autosaves"/persistence::sha256(review.source->canonical_content_bytes),campaign_.settings()};
        current_.content().restore(*review.source);
        if(review.current) current_=std::move(*review.continuing);
        else {current_.preserve_for_recovery();current_.commit(*review.source,review.checkpoint);}
        campaign_=std::move(next);active_content_=std::move(review.source);active_=true;storage_blocked_=false;
        missing_content_hash_.clear();content_target_.clear();halted_=true;milestone_.reset();++generation_;publish();
        emit operationCompleted(static_cast<qulonglong>(token),true,tr("Original rules restored. Your company is ready at its saved morning."));
    } catch(const std::exception& error) {
        const auto message=QString::fromUtf8(error.what());pauseDriver();emit noticeRaised(message);publish();emit operationCompleted(static_cast<qulonglong>(token),false,message);
    }
}
}
