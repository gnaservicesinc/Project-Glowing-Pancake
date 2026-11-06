#pragma once

#include <pgp/core/lab.hpp>
#include <QRegularExpression>
#include <stdexcept>

namespace pgp::bridge::lab_inputs {
inline std::int64_t integer(const QString& input,std::int64_t maximum) {
    static const QRegularExpression pattern{QStringLiteral("^[0-9]{1,12}$")};
    if (!pattern.match(input).hasMatch()) throw std::invalid_argument("Enter a positive whole number");
    bool ok=false;
    const auto value=input.toLongLong(&ok);
    if (!ok || value<=0 || value>maximum) throw std::invalid_argument("The entered amount exceeds its allowed range");
    return value;
}
inline std::int64_t compute(const QString& input) {
    static const QRegularExpression pattern{QStringLiteral("^([0-9]{1,9})(?:\\.([0-9]{1,3}))?$")};
    const auto match=pattern.match(input);
    if (!match.hasMatch()) throw std::invalid_argument("Compute uses CU with at most three decimal places");
    const auto value=match.captured(1).toLongLong()*1000+match.captured(2).leftJustified(3,QLatin1Char('0')).toLongLong();
    if (value<=0) throw std::invalid_argument("Compute must be positive");
    return value;
}
inline core::LabAllocation allocation(const QString& source,const QString& rate,const QString& staff) {
    return {source.toStdString(),core::MilliCUPerDay{compute(rate)},static_cast<std::uint32_t>(integer(staff,1'000'000))};
}
inline core::Cents policy_money(const QString& input) {
    static const QRegularExpression pattern{QStringLiteral("^([0-9]{1,10})(?:\\.([0-9]{1,2}))?$")};
    const auto match=pattern.match(input);
    if (!match.hasMatch()) throw std::invalid_argument("Enter dollars with at most two decimal places");
    return core::Cents{match.captured(1).toLongLong()*100+match.captured(2).leftJustified(2,QLatin1Char('0')).toLongLong()};
}
}
