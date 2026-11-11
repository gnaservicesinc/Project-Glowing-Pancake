#pragma once
#include <pgp/core/simulation.hpp>
#include <QVariantMap>

namespace pgp::bridge {
[[nodiscard]] QVariantMap departureArticle(const core::SimulationState&,const core::DepartureRelease&);
}
