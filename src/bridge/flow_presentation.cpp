#include "flow_presentation.hpp"

#include <QCoreApplication>
#include <algorithm>

namespace pgp::bridge {
namespace {
QString text(std::string_view value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
}
QString cu(std::int64_t amount) {
    const auto whole = amount / 1000;
    const auto residue = amount % 1000;
    return residue == 0 ? QString::number(static_cast<qlonglong>(whole)) :
        QStringLiteral("%1.%2").arg(static_cast<qlonglong>(whole)).arg(static_cast<qlonglong>(residue),3,10,QLatin1Char('0'));
}
QString readable(std::string_view source) {
    auto result = text(source);
    const auto separator = result.lastIndexOf(QLatin1Char('.'));
    if (separator >= 0) result = result.mid(separator + 1);
    result.replace(QLatin1Char('_'),QLatin1Char(' '));
    if (!result.isEmpty()) result[0] = result[0].toUpper();
    return result;
}
QString choice_label(const runtime::PresentedChoice& choice, const content::Collection* collection) {
    if (collection) if (const auto found=collection->messages.find(choice.label_key);found!=collection->messages.end()) return text(found->second);
    const auto id = text(choice.id).toLower();
    if (id.contains(QStringLiteral("accept"))) return QStringLiteral("Accept");
    if (id.contains(QStringLiteral("decline")) || id.contains(QStringLiteral("reject"))) return QStringLiteral("Decline");
    if (id.contains(QStringLiteral("negotiate")) || id.contains(QStringLiteral("counter"))) return QStringLiteral("Negotiate");
    if (id.contains(QStringLiteral("continue"))) return QStringLiteral("Continue");
    return readable(choice.id);
}
QString presentation_heading(const runtime::Presentation& row, const content::Collection& collection) {
    const auto found=collection.messages.find(row.title_key);
    return found==collection.messages.end()?presentation_title(row.kind):text(found->second);
}
QString presentation_title(runtime::PresentationKind kind) {
    if (kind == runtime::PresentationKind::dialogue) return QStringLiteral("Campaign decision");
    if (kind == runtime::PresentationKind::result) return QStringLiteral("Decision result");
    return QStringLiteral("Company briefing");
}
QString presentation_context(const runtime::Checkpoint& checkpoint, const runtime::Presentation& row, const content::Collection* collection) {
    auto context = readable(row.flow_id);
    if (row.kind == runtime::PresentationKind::news) {
        const auto localized = QCoreApplication::translate("CampaignNews", row.text_key.c_str());
        if (localized != text(row.text_key)) context = localized;
    }
    if (collection) if (const auto found=collection->messages.find(row.text_key);found!=collection->messages.end()) context=text(found->second);
    const auto& activations = checkpoint.runtime.activations;
    const auto found = std::lower_bound(activations.begin(), activations.end(), row.activation_id,
        [](const auto& activation, std::uint64_t id) { return activation.id < id; });
    if (found != activations.end() && found->id == row.activation_id && found->committed_source) {
        const auto& source = *found->committed_source;
        context += QStringLiteral("\n%1 · %2\nSettled %3\n%4")
            .arg(readable(source.fact_id), text(source.subject.id), text(core::iso_date(source.settled_on)), text(source.cause));
        if (source.unit == content::Unit::milli_CU)
            context += QStringLiteral("\n%1 CU completed").arg(cu(std::get<std::int64_t>(source.result)));
    }
    return context;
}
QString presentation_detail(const runtime::Checkpoint& checkpoint, const runtime::Presentation& row, const content::Collection* collection) {
    const auto context = presentation_context(checkpoint, row, collection);
    if (row.kind == runtime::PresentationKind::dialogue && !row.resolved)
        return QStringLiteral("%1\nA response is required before work continues. Review the available options.").arg(context);
    if (!row.selected_choice.empty()) return QStringLiteral("%1\nSelected response: %2").arg(context,readable(row.selected_choice));
    return QStringLiteral("%1\nThis briefing records a committed campaign outcome.").arg(context);
}
} // namespace pgp::bridge
