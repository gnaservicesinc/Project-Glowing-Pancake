#include "flow_presentation.hpp"

#include <QCoreApplication>
#include <QLocale>
#include <algorithm>

namespace pgp::bridge {
namespace {
QString text(std::string_view value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
const std::string* localized(const content::Collection& collection, std::string_view key) {
    if (collection.translations.empty()) return collection.message(key);
    auto locale = QLocale().name();
    locale.replace(QLatin1Char('_'), QLatin1Char('-'));
    return collection.message(key, locale.toStdString());
}
// Authored news can open with a short headline on its own line.
bool has_news_headline(const QString& copy) {
    const auto line = copy.indexOf(u'\n');
    return line > 0 && line <= 120 && !copy.mid(line + 1).trimmed().isEmpty();
}
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
    if (collection) if (const auto* value=localized(*collection,choice.label_key)) return text(*value);
    const auto id = text(choice.id).toLower();
    if (id.contains(QStringLiteral("accept"))) return QStringLiteral("Accept");
    if (id.contains(QStringLiteral("decline")) || id.contains(QStringLiteral("reject"))) return QStringLiteral("Decline");
    if (id.contains(QStringLiteral("negotiate")) || id.contains(QStringLiteral("counter"))) return QStringLiteral("Negotiate");
    if (id.contains(QStringLiteral("continue"))) return QStringLiteral("Continue");
    return readable(choice.id);
}
QString presentation_heading(const runtime::Presentation& row, const content::Collection& collection) {
    if (!row.title_key.empty())
        if (const auto* value=localized(collection,row.title_key)) return text(*value);
    if (row.kind != runtime::PresentationKind::dialogue) {
        if (const auto* value=localized(collection,row.text_key)) {
            const auto copy=text(*value);
            if (has_news_headline(copy)) return copy.section(u'\n',0,0);
        }
        if (row.kind == runtime::PresentationKind::result) return presentation_title(row.kind);
        if (const auto* value=localized(collection,row.flow_id+".title")) return text(*value);
        return readable(row.flow_id);
    }
    return presentation_title(row.kind);
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
    if (collection) if (const auto* value=localized(*collection,row.text_key)) context=text(*value);
    if (collection && row.kind != runtime::PresentationKind::dialogue && has_news_headline(context))
        context=context.mid(context.indexOf(u'\n')+1).trimmed();
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
    if (!row.selected_choice.empty()) {
        const auto choice=std::ranges::find(row.choices,row.selected_choice,&runtime::PresentedChoice::id);
        if (choice!=row.choices.end())
            return QCoreApplication::translate("CampaignNews","%1\n\nYou chose: %2").arg(context,choice_label(*choice,collection));
    }
    return context;
}
} // namespace pgp::bridge
