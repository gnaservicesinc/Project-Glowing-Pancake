#pragma once
#include "campaign_snapshot.hpp"
#include <QVariantMap>

namespace pgp::bridge {
[[nodiscard]] QVariantMap lifeReport(const runtime::Checkpoint&,const runtime::CompiledProgram&);
}
