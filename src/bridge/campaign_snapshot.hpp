#pragma once

#include <pgp/session/campaign.hpp>

#include <QMetaType>
#include <memory>

namespace pgp::bridge {
// Worker-owned authority is copied into an immutable delivery. Generation
// changes only when a save is loaded, so an older saved revision is legitimate.
struct CampaignSnapshot {
    std::uint64_t generation = 1;
    runtime::Checkpoint checkpoint;
    persistence::Settings settings;
    bool driver_halted = false;
    bool current_campaign = false;
    bool storage_blocked = false;
};
using CampaignSnapshotPtr = std::shared_ptr<const CampaignSnapshot>;
}
Q_DECLARE_METATYPE(pgp::bridge::CampaignSnapshotPtr)
