#include "world_controller.hpp"
#include <QLocale>
#include <QRegularExpression>
#include <algorithm>

namespace pgp::bridge {
namespace {
QString q(const std::string &value) { return QString::fromStdString(value); }
QString money(core::Cents value) {
    return QLocale{QLocale::English, QLocale::UnitedStates}.toCurrencyString(static_cast<double>(value.value()) / 100);
}
QString field(core::Cents value) {
    return QStringLiteral("%1.%2").arg(value.value() / 100).arg(value.value() % 100, 2, 10, QLatin1Char('0'));
}
QString date(core::OrdinalDay value) { return q(core::iso_date(value)); }
core::Cents amount(QString input) {
    input = input.trimmed();
    static const QRegularExpression form{QStringLiteral("^[0-9]{1,8}(?:\\.[0-9]{1,2})?$")};
    if (!form.match(input).hasMatch())
        throw std::invalid_argument(
            "Enter dollars with at most two decimal places, without a currency sign or commas.");
    const auto parts = input.split('.');
    return core::Cents{parts[0].toLongLong() * 100 +
                       (parts.size() == 2 ? parts[1].leftJustified(2, '0').toLongLong() : 0)};
}
} // namespace
QVariantMap WorldController::recruitmentOffer(QString id) const {
    if (!snapshot_)
        return {};
    const auto &s = snapshot_->checkpoint.world.simulation;
    if (!s.management || !s.management->people || !s.management->catalog.people->recruitment)
        return {};
    const auto &applicants = s.management->people->applicants;
    const auto it = std::ranges::find(applicants, id.toStdString(), [](const auto &row) { return row.profile.id; });
    if (it == applicants.end() || !it->recruitment)
        return {};
    const auto &a = *it;
    const auto &n = *a.recruitment;
    const auto &r = *s.management->catalog.people->recruitment;
    const auto status = q(core::recruitment_status(a, s.date));
    QVariantList history, desks;
    QString summary = tr("Open to a compensation discussion.");
    if (status == "waiting")
        summary = tr("Considering your offer · response %1").arg(date(n.offers.back().respond_on));
    else if (status == "counter")
        summary = tr("Counteroffer · %1/month + %2 signing · available until %3")
                      .arg(money(n.offers.back().counter_salary), money(n.offers.back().counter_signing),
                           date(n.offers.back().expires_on));
    else if (status == "agreed") {
        const auto terms = core::agreed_employment(a);
        summary = tr("Agreed · %1/month + %2 signing · sign before %3")
                      .arg(money(terms.first), money(terms.second), date(n.offers.back().expires_on));
    } else if (status == "declined")
        summary = tr("This application has ended. No employment or payroll was created.");
    else if (status == "expired")
        summary = tr("The terms expired without a signed hire. No employment or payroll was created.");
    for (const auto &o : n.offers) {
        QString detail =
            tr("%1 · your offer %2/month + %3 signing").arg(date(o.submitted_on), money(o.salary), money(o.signing));
        if (o.reply == "pending")
            detail += tr("\nResponse due %1").arg(date(o.respond_on));
        else if (o.reply == "counter")
            detail += tr("\n%1 · requested %2/month + %3 signing")
                          .arg(date(o.respond_on), money(o.counter_salary), money(o.counter_signing));
        else
            detail +=
                tr("\n%1 · %2")
                    .arg(date(o.respond_on), o.reply == "accept" ? tr("Applicant accepted") : tr("Applicant declined"));
        history.push_back(QVariantMap{{"detail", detail}});
    }
    if (status == "agreed")
        for (const auto &row : headquarters().value("items").toList()) {
            const auto i = row.toMap();
            if (i.value("kind") == "desk" && i.value("placed").toBool() && i.value("delivered").toBool() &&
                i.value("person").toString().isEmpty())
                desks.push_back(QVariantMap{{"id", i.value("id")},
                                            {"name", tr("Desk %1 · column %2, row %3")
                                                         .arg(i.value("id").toInt())
                                                         .arg(i.value("x").toInt() + 1)
                                                         .arg(i.value("y").toInt() + 1)}});
        }
    const auto salary = core::Cents{core::mul_div_round_half_up(
        static_cast<std::uint64_t>(s.management->catalog.people->monthly_salary.value()), r.minimum_offer, 10000)};
    const auto maximum = core::Cents{core::mul_div_round_half_up(
        static_cast<std::uint64_t>(s.management->catalog.people->monthly_salary.value()), r.maximum_offer, 10000)};
    return {{"id", id},
            {"name", q(a.profile.name)},
            {"status", status},
            {"summary", summary},
            {"salary", field(a.salary)},
            {"signing", field(a.signing)},
            {"history", history},
            {"desks", desks},
            {"revision", reviewToken()},
            {"canOffer", status == "available" || status == "counter"},
            {"canAccept", status == "counter"},
            {"canHire", status == "agreed"},
            {"canDecline", status != "declined" && status != "expired"},
            {"terms", tr("Requested %1/month + %2 signing. You can propose %3–%4/month and $0–%5 signing. "
                         "Response in %6. Terms last %7 days. Up to %8 offer rounds. Signing creates %9 days of paid "
                         "onboarding and reserves the selected desk.")
                          .arg(money(a.salary), money(a.signing), money(salary), money(maximum),
                               money(core::Cents{s.management->catalog.people->signing_cost.value() *
                                                 r.maximum_signing_multiple}))
                          .arg(r.response==core::Days{1}?tr("one day"):tr("%1 days").arg(r.response.value()))
                          .arg(r.validity.value())
                          .arg(r.maximum_rounds)
                          .arg(s.management->catalog.people->onboarding.value())}};
}
void WorldController::proposeEmployment(QString id, QString salary, QString signing, QString revision) {
    if (!hqReview(revision))
        return;
    try {
        emit actionRequested(
            core::ManageRecruitment{core::ProposeEmployment{id.toStdString(), amount(salary), amount(signing)}});
    } catch (const std::exception &error) {
        emit noticeRaised(QString::fromUtf8(error.what()));
    }
}
void WorldController::acceptEmploymentCounter(QString id, QString revision) {
    if (hqReview(revision))
        emit actionRequested(core::ManageRecruitment{core::AcceptEmploymentCounter{id.toStdString()}});
}
void WorldController::declineApplicant(QString id, QString revision) {
    if (hqReview(revision))
        emit actionRequested(core::ManageRecruitment{core::DeclineApplicant{id.toStdString()}});
}
void WorldController::hireNegotiatedEmployee(QString id, int desk, QString revision) {
    if (hqReview(revision) && desk > 0)
        emit actionRequested(core::HireEmployee{id.toStdString(), static_cast<std::uint32_t>(desk)});
}
} // namespace pgp::bridge
