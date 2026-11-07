#include "founding_controller.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>
#include <pgp/core/checked_arithmetic.hpp>
#include <pgp/runtime/campaign_identity.hpp>

#include <algorithm>

namespace pgp::bridge {
namespace {
QString text(const std::string& value) { return QString::fromStdString(value); }

QString money(std::int64_t cents) {
    const auto magnitude = cents < 0 ? -cents : cents;
    const QLocale locale{QLocale::English, QLocale::UnitedStates};
    return QStringLiteral("%1$%2.%3")
        .arg(cents < 0 ? QStringLiteral("−") : QString{}, locale.toString(magnitude / 100))
        .arg(magnitude % 100, 2, 10, QLatin1Char('0'));
}

QVariantMap defaults() {
    return {{"first_name","Alex"},{"last_name","Vale"},{"gender","nonbinary"},{"pronouns","they/them"},
        {"company","Daybreak Research"},{"seed","19972015"},{"headquarters","US-CA"},{"difficulty","difficulty.standard"},
        {"sociability","introverted"},{"activity","sedentary"},{"disposition","deliberate"},
        {"family","middle"},{"leg","genius"},{"route","partnership"},{"legal_form","corporation"},
        {"office","shared"},{"researcher","founder"},{"mission","survival"},
        {"choices",QVariantMap{}},{"backers",QStringList{}}};
}
}

FoundingController::FoundingController(const content::Collection& collection, std::filesystem::path path, QObject* parent)
    : QObject(parent), draft_path_(std::move(path)), form_(defaults()) {
    if (const auto* definition = collection.find("scenario.founder_campaign")) {
        const auto& scenario = std::get<content::ScenarioDefinition>(definition->data);
        catalog_ = scenario.founding;
        difficulties_ = scenario.difficulties;
        headquarters_ = scenario.headquarters_choices;
    }
    if (!available()) return;
    QFile file{QString::fromStdU16String(draft_path_.u16string())};
    if (!file.open(QIODevice::ReadOnly) || file.size() > 32768) return;
    const auto saved = QJsonDocument::fromJson(file.readAll()).object();
    const auto format = saved.value("format").toString();
    if (format != QStringLiteral("founding-draft-1") && format != QStringLiteral("founding-draft-2")) return;
    const auto fields = saved.value("form").toObject().toVariantMap();
    for (auto it = fields.begin(); it != fields.end(); ++it)
        if (form_.contains(it.key())) form_[it.key()] = it.value();
    if (format == QStringLiteral("founding-draft-1")) {
        // The older pages grouped multiple questions. Walk through the story
        // with their existing answers instead of guessing the last question.
        if (saved.value("page").toInt() > 0) beat_ = QStringLiteral("first_name");
    } else {
        const auto id = saved.value("beat").toString();
        if (!id.isEmpty() && id.size() <= 80) beat_ = id;
    }
}

void FoundingController::reset() {
    form_ = defaults();
    beat_ = QStringLiteral("opening");
    error_.clear();
    if (available()) saveDraft();
    emit changed();
    emit beatChanged();
}

content::FoundingSelection FoundingController::selection() const {
    content::FoundingSelection selected;
    const auto field = [&](const char* key) { return form_.value(QLatin1String{key}).toString().trimmed().toStdString(); };
    selected.first_name = field("first_name");
    selected.last_name = field("last_name");
    selected.gender = field("gender");
    selected.sociability = field("sociability");
    selected.activity = field("activity");
    selected.disposition = field("disposition");
    selected.family = field("family");
    selected.leg = field("leg");
    selected.route = field("route");
    selected.legal_form = field("legal_form");
    selected.office = field("office");
    selected.researcher = field("researcher");
    const auto choices = form_.value("choices").toMap();
    for (auto it = choices.begin(); it != choices.end(); ++it)
        selected.choices.emplace(it.key().toStdString(), it.value().toString().toStdString());
    for (const auto& id : form_.value("backers").toStringList()) selected.backers.push_back(id.toStdString());
    return selected;
}

QVariantList FoundingController::options(const std::vector<content::FoundingOption>& rows) const {
    QVariantList result;
    const auto selected = selection();
    for (const auto& row : rows) {
        auto reason = content::founding_ineligibility(row, selected);
        if (row.id == "wealthy" && selected.family != "rich") reason = "Requires rich parents at birth.";
        result.push_back(QVariantMap{{"id",text(row.id)},{"name",text(row.name)},{"detail",text(row.detail)},
            {"available",reason.empty()},{"reason",text(reason)}});
    }
    return result;
}

QVariantList FoundingController::families() const { return catalog_ ? options(catalog_->families) : QVariantList{}; }
QVariantList FoundingController::legs() const { return catalog_ ? options(catalog_->legs) : QVariantList{}; }
QVariantList FoundingController::routes() const { return catalog_ ? options(catalog_->routes) : QVariantList{}; }
QVariantList FoundingController::backers() const {
    if (!catalog_) return {};
    auto rows = options(catalog_->backers);
    for (qsizetype i = 0; i < rows.size(); ++i) {
        const auto& backer = catalog_->backers[static_cast<std::size_t>(i)];
        auto row = rows[i].toMap();
        const auto share = static_cast<double>(backer.shares_bp) / 100;
        row["terms"] = backer.legal_form == "nonprofit" ? tr("The gift comes with a trustee seat.") :
            tr("They ask for a board seat and %1% equity.").arg(QLocale().toString(share, 'f', backer.shares_bp % 100 ? 2 : 0));
        rows[i] = row;
    }
    return rows;
}
QVariantList FoundingController::offices() const { return catalog_ ? options(catalog_->offices) : QVariantList{}; }
QVariantList FoundingController::researchers() const { return catalog_ ? options(catalog_->researchers) : QVariantList{}; }

QVariantList FoundingController::chapters() const {
    QVariantList result;
    if (catalog_) {
        for (const auto& scene : catalog_->scenes)
            result.push_back(QVariantMap{{"id",text(scene.id)},{"title",text(scene.title)},
                {"date",text(scene.date)},{"text",text(scene.text)},{"choices",options(scene.choices)}});
    }
    return result;
}

QVariantList FoundingController::difficulties() const {
    QVariantList result;
    for (const auto& difficulty : difficulties_)
        result.push_back(QVariantMap{{"id",text(difficulty.id)},{"name",text(difficulty.name)},
            {"detail",text(difficulty.description)}});
    return result;
}

QVariantList FoundingController::headquarters() const {
    QVariantList result;
    for (const auto& id : headquarters_) {
        QString name = text(id);
        if (id == "US-CA") name = tr("California");
        else if (id == "US-TX") name = tr("Texas");
        else if (id == "US-WA") name = tr("Washington");
        result.push_back(QVariantMap{{"id",text(id)},{"name",name}});
    }
    return result;
}

QString FoundingController::preview() const {
    if (!catalog_) return {};
    try {
        const auto terms = content::founding_terms(*catalog_, selection());
        const auto difficulty = form_.value("difficulty").toString().toStdString();
        const auto found = std::ranges::find(difficulties_, difficulty, &content::CampaignDifficulty::id);
        if (found == difficulties_.end()) return tr("Choose a campaign difficulty.");
        const auto cash = static_cast<std::int64_t>(core::mul_div_floor(
            static_cast<std::uint64_t>(terms.contributed_cents), found->starting_cash_percent, 100).quotient);
        if (cash <= terms.paid_setup_cents) return tr("These terms cannot pay the founding expenses.");
        const auto monthly = terms.office_cents + terms.payroll_cents + catalog_->compute_monthly_cents + catalog_->founder_salary_cents;
        const auto equity = terms.nonprofit ? tr("The institution owns its assets.") :
            tr("Founder ownership: %1.%2%").arg(terms.founder_bp / 100).arg(terms.founder_bp % 100, 2, 10, QLatin1Char('0'));
        return tr("Contributions   %1\nSigning costs   %2\nCash at opening   %3\nMonthly commitments   %4\n%5\nScientific prestige %6  ·  Public buzz %7")
            .arg(money(cash), money(terms.paid_setup_cents), money(cash - terms.paid_setup_cents), money(monthly), equity)
            .arg(terms.prestige).arg(terms.buzz);
    } catch (const std::exception& error) {
        return QString::fromUtf8(error.what());
    }
}

void FoundingController::saveDraft() {
    try {
        std::filesystem::create_directories(draft_path_.parent_path());
        QSaveFile file{QString::fromStdU16String(draft_path_.u16string())};
        const auto bytes = QJsonDocument{QJsonObject::fromVariantMap(
            {{"format","founding-draft-2"},{"beat",beat_},{"form",form_}})}.toJson(QJsonDocument::Compact);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            error_ = tr("Could not save your place in the prologue.");
        else error_.clear();
    } catch (const std::exception&) {
        error_ = tr("Could not save your place in the prologue.");
    }
}

void FoundingController::setBeat(QString id) {
    if (!available() || id.isEmpty() || id.size() > 80 || id == beat_) return;
    beat_ = std::move(id);
    saveDraft();
    emit changed();
    emit beatChanged();
}

void FoundingController::setField(QString key, QVariant value) {
    const auto answer = value.toString();
    if (!form_.contains(key) || key == "choices" || key == "backers" || answer.size() > 128 ||
        form_.value(key).toString() == answer) return;
    form_[key] = answer;
    if (key == "legal_form" || key == "route") form_["backers"] = QStringList{};
    if (key == "legal_form") {
        form_["route"] = answer == "nonprofit" ? "mission_director" : "partnership";
        if (answer == "nonprofit" && form_.value("mission").toString() == "profit") form_["mission"] = "survival";
    }
    if (key == "leg" && answer != "genius") form_["researcher"] = "first_hire";
    saveDraft();
    emit changed();
}

QString FoundingController::answerText(QString field, QString value) {
    value = value.trimmed();
    if (field == "seed") {
        try {
            (void)runtime::parse_world_seed(value.toStdString());
        } catch (const std::exception&) {
            return tr("Enter a whole number from 0 to 18446744073709551615.");
        }
    } else {
        const qsizetype limit = field == "company" ? 128 : 64;
        if (value.isEmpty() && field != "pronouns") return tr("Enter an answer to continue.");
        if (value.toUtf8().size() > limit) return tr("This answer is too long.");
        if (field == "last_name" && form_.value("first_name").toString().toUtf8().size() + value.toUtf8().size() + 1 > 128)
            return tr("The full name is too long.");
        if (std::ranges::any_of(value, [](QChar character) { return character.unicode() < 32 || character.unicode() == 127; }))
            return tr("Enter the answer on one line.");
    }
    setField(std::move(field), value);
    return {};
}

void FoundingController::choose(QString scene, QString id) {
    if (!catalog_) return;
    const auto found = std::ranges::find(catalog_->scenes, scene.toStdString(), &content::FormativeScene::id);
    if (found == catalog_->scenes.end() ||
        std::ranges::find(found->choices, id.toStdString(), &content::FoundingOption::id) == found->choices.end()) return;
    auto choices = form_.value("choices").toMap();
    choices[scene] = id;
    form_["choices"] = choices;
    saveDraft();
    emit changed();
}

QString FoundingController::answerOffer(QString id, bool accept) {
    if (!catalog_) return {};
    auto selected = selection();
    std::erase(selected.backers, id.toStdString());
    if (accept) {
        selected.backers.push_back(id.toStdString());
        try {
            (void)content::founding_terms(*catalog_, selected, false);
        } catch (const std::exception& error) {
            return QString::fromUtf8(error.what());
        }
    }
    QStringList backers;
    for (const auto& backer : selected.backers) backers.push_back(text(backer));
    form_["backers"] = backers;
    saveDraft();
    emit changed();
    return {};
}
}
