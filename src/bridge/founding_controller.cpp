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
        {"home_app","room"},{"home_room","Bedroom"},{"home_guide",""},{"home_contact",""},{"investment","-1"},{"company","Daybreak Research"},{"seed","19972015"},{"headquarters","US-CA"},{"difficulty","difficulty.standard"},
        {"sociability","introverted"},{"activity","sedentary"},{"disposition","deliberate"},
        {"family","middle"},{"leg","genius"},{"route","investment"},{"legal_form","corporation"},
        {"office","home"},{"researcher","founder"},{"mission","survival"},
        {"style",QVariantList{0,0,0,0,0,0,0,0}},{"appearance",QVariantList{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
        {"choices",QVariantMap{}},{"backers",QStringList{}}};
}
}

FoundingController::FoundingController(const content::Collection& collection, std::filesystem::path path, QObject* parent)
    : QObject(parent), draft_path_(std::move(path)), form_(defaults()) {
    if (const auto* definition = collection.find("scenario.founder_campaign")) {
        const auto& scenario = std::get<content::ScenarioDefinition>(definition->data);
        catalog_ = scenario.founding;
        headquarters_ = scenario.headquarters_choices;
        if(scenario.infrastructure && scenario.infrastructure->workplace) premises_rules_=scenario.infrastructure->workplace->headquarters;
    }
    deriveOpening();
    if (!available()) return;
    QFile file{QString::fromStdU16String(draft_path_.u16string())};
    if (!file.open(QIODevice::ReadOnly) || file.size() > 32768) return;
    const auto saved = QJsonDocument::fromJson(file.readAll()).object();
    const auto format = saved.value("format").toString();
    if (format != QStringLiteral("founding-draft-3")) return;
    const auto fields = saved.value("form").toObject().toVariantMap();
    for (auto it = fields.begin(); it != fields.end(); ++it)
        if (form_.contains(it.key())) form_[it.key()] = it.value();
    const auto id=saved.value("beat").toString();
    if(!id.isEmpty() && id.size()<=80) beat_=id;
    deriveOpening();
}

void FoundingController::reset() {
    form_ = defaults();
    deriveOpening();
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
    bool investment_ok=false;const auto dollars=form_.value("investment").toString().toLongLong(&investment_ok);
    if(!investment_ok || dollars < -1 || dollars>10000000000) throw std::invalid_argument("Enter a whole-dollar personal investment within your savings.");
    selected.personal_investment_cents=dollars<0?-1:dollars*100;
    selected.legal_form = field("legal_form");
    selected.office = field("office");
    selected.researcher = field("researcher");
    if(catalog_ && catalog_->opening) {
        core::PortraitRecipe recipe;
        const auto parts=form_.value("appearance").toList();
        if(parts.size()!=static_cast<qsizetype>(recipe.parts.size())) throw std::invalid_argument("Choose your appearance.");
        for(std::size_t i=0;i<recipe.parts.size();++i) {
            bool okay=false;const auto part=parts[static_cast<qsizetype>(i)].toInt(&okay);
            if(!okay || part<0 || part>=core::portrait_part_counts[i]) throw std::invalid_argument("Unknown appearance choice.");
            recipe.parts[i]=static_cast<std::uint8_t>(part);
        }
        const auto style=form_.value("style").toList();
        if(style.size()!=static_cast<qsizetype>(recipe.style.size())) throw std::invalid_argument("Choose your portrait style.");
        for(std::size_t i=0;i<recipe.style.size();++i) {bool okay=false;const auto value=style[static_cast<qsizetype>(i)].toInt(&okay);if(!okay || value<0 || value>=core::portrait_style_counts[i]) throw std::invalid_argument("Unknown portrait style.");recipe.style[i]=static_cast<std::uint8_t>(value);}
        selected.appearance=recipe;
    }
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
        result.push_back(QVariantMap{{"id",text(row.id)},{"name",text(row.name)},{"detail",text(row.detail)+(row.prestige || row.buzz?tr(" · +%1 prestige / +%2 buzz").arg(row.prestige).arg(row.buzz):QString{})},
            {"available",reason.empty()},{"reason",text(reason)}});
    }
    return result;
}

QVariantList FoundingController::families() const { return catalog_ ? options(catalog_->families) : QVariantList{}; }
QVariantList FoundingController::legs() const { return catalog_ ? options(catalog_->legs) : QVariantList{}; }
QVariantList FoundingController::routes() const {
    return catalog_?options(catalog_->routes):QVariantList{};
}
QVariantList FoundingController::backers() const {
    if (!catalog_) return {};
    if(catalog_->opening && form_.value("route")!="investment" && form_.value("route")!="mission_director") return {};
    auto rows = options(catalog_->backers);
    for (qsizetype i = 0; i < rows.size(); ++i) {
        const auto& backer = catalog_->backers[static_cast<std::size_t>(i)];
        auto row = rows[i].toMap();
        const auto share = static_cast<double>(backer.shares_bp) / 100;
        row["terms"] = backer.legal_form == "nonprofit" ? tr("The gift comes with a trustee seat.") :
            tr("They ask for a board seat and %1% equity.").arg(QLocale().toString(share, 'f', backer.shares_bp % 100 ? 2 : 0));
        const auto terms=content::founding_terms(*catalog_,selection(),false);
        const auto premium=std::min(catalog_->maximum_finance_bonus_bp,terms.prestige*catalog_->prestige_finance_bp+terms.buzz*catalog_->buzz_finance_bp);
        const auto amount=core::mul_div_floor(static_cast<std::uint64_t>(backer.capital_cents),10000+premium,10000).quotient;
        row["terms"]=tr("Your pitch receives an offer of %1. %2").arg(money(static_cast<std::int64_t>(amount)),row.value("terms").toString());
        rows[i] = row;
    }
    return rows;
}
QVariantList FoundingController::chapters() const {
    QVariantList result;
    if (catalog_) {
        for (const auto& scene : catalog_->scenes)
            result.push_back(QVariantMap{{"id",text(scene.id)},{"title",text(scene.title)},
                {"date",text(scene.date)},{"text",text(scene.text)},{"choices",options(scene.choices)}});
    }
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

QVariantMap FoundingController::personalFunds() const {
    if(!catalog_) return {};
    const auto family=form_.value("leg")=="political"?"rich":form_.value("family").toString().toStdString();
    const auto total=content::founding_option(catalog_->families,family).capital_cents+content::founding_option(catalog_->legs,form_.value("leg").toString().toStdString()).capital_cents;
    return {{"total",money(total)},{"dollars",QString::number(total/100)}};
}
QString FoundingController::signingError() const {
    if(!catalog_) return tr("The founding catalog is unavailable.");
    try {
        const auto selected=selection();
        if(selected.route=="hired_ceo" && selected.leg!="genius" && selected.leg!="political" && selected.leg!="wealthy") return tr("The lab has declined this leadership application.");
        const auto terms=content::founding_terms(*catalog_,selected);
        if(terms.contributed_cents<=terms.paid_setup_cents) return tr("Secure enough funding to pay formation expenses and leave the company some working cash.");
        runtime::validate_campaign_identity({form_.value("company").toString().trimmed().toStdString(),selected.first_name+" "+selected.last_name,form_.value("pronouns").toString().toStdString()});
        return {};
    } catch(const std::exception& e) {return QString::fromUtf8(e.what());}
}
QString FoundingController::preview() const {
    if (!catalog_) return {};
    try {
        const auto terms = content::founding_terms(*catalog_, selection());
        const auto cash=terms.contributed_cents;
        if(cash<=terms.paid_setup_cents) return tr("Secure funding sufficient to pay formation expenses.");
        const auto equity = terms.nonprofit ? tr("The institution owns its assets.") :
            tr("Your ownership: %1.%2%").arg(terms.founder_bp / 100).arg(terms.founder_bp % 100, 2, 10, QLatin1Char('0'));
        {
            const auto selected=selection();
            QStringList sources;
            if(selected.route!="hired_ceo") {
                const auto family=content::founding_option(catalog_->families,selected.leg=="political"?"rich":selected.family).capital_cents;
                const auto personal=family+content::founding_option(catalog_->legs,selected.leg).capital_cents;
                (void)personal;sources<<tr("Your investment %1 · personal savings retained %2").arg(money(terms.personal_contribution_cents),money(terms.personal_remaining_cents));
            }
            const auto& route=content::founding_option(catalog_->routes,selected.route);
            if(route.capital_cents>0 && selected.route!="mission_director") sources<<tr("%1: %2").arg(text(route.name),money(route.capital_cents));
            const auto premium=std::min(catalog_->maximum_finance_bonus_bp,terms.prestige*catalog_->prestige_finance_bp+terms.buzz*catalog_->buzz_finance_bp);
            for(const auto& id:selected.backers) {
                const auto& offer=content::founding_option(catalog_->backers,id);
                const auto amount=core::mul_div_floor(static_cast<std::uint64_t>(offer.capital_cents),10000+premium,10000).quotient;
                sources<<tr("%1: %2").arg(text(offer.name),money(static_cast<std::int64_t>(amount)));
            }
            const auto desks=selected.office=="home"?0U:terms.opening_staff+(terms.founder_researcher?0U:1U);
            std::int64_t compute=0;
            if(premises_rules_) for(const auto& offer:premises_rules_->equipment) if(offer.id=="desktop") compute=offer.compute.value()*desks;
            const auto equipment=premises_rules_ && desks?tr("Inherited company equipment: %1 desks and %1 desktop computers. You bring your phone and laptop; no compute lease.").arg(desks):selected.office=="home"?tr("You bring your existing phone and laptop; no desktop or compute lease."):tr("The office includes a workstation; you bring your phone and laptop. No compute lease.");
            return tr("%1\nCash %2 · signing %3\nRent %4/mo · payroll %5/mo · your salary %6/mo\n%7\n%8 research-capable people · %9 CU/day\n%10")
                .arg(sources.join(" · "),money(cash-terms.paid_setup_cents),money(terms.paid_setup_cents),money(terms.office_cents),money(terms.payroll_cents),money(catalog_->founder_salary_cents),equity).arg(terms.opening_staff).arg(static_cast<double>(compute)/1000).arg(equipment);
        }
    } catch (const std::exception& error) {
        return QString::fromUtf8(error.what());
    }
}

void FoundingController::saveDraft() {
    try {
        std::filesystem::create_directories(draft_path_.parent_path());
        QSaveFile file{QString::fromStdU16String(draft_path_.u16string())};
        const auto bytes = QJsonDocument{QJsonObject::fromVariantMap(
            {{"format","founding-draft-3"},{"beat",beat_},{"form",form_}})}.toJson(QJsonDocument::Compact);
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
    if (!form_.contains(key) || key == "choices" || key == "backers" || (key=="appearance" || key=="style") || answer.size() > 128 ||
        form_.value(key).toString() == answer) return;
    if(key=="gender" && answer!="woman" && answer!="man" && answer!="nonbinary" && answer!="unspecified") return;
    if(key=="pronouns" && !pronounOptions().contains(answer)) return;
    if(key=="investment") {bool ok=false;const auto dollars=answer.toLongLong(&ok);if(!ok || dollars < -1 || dollars>10000000000)return;}
    form_[key] = answer;
    if(key=="gender") {
        auto parts=form_.value("appearance").toList(),style=form_.value("style").toList();
        if(parts.size()==15 && style.size()==8) {parts[1]=answer=="woman"?2:answer=="man"?0:1;parts[9]=0;style[0]=answer=="woman"?5:0;style[2]=answer=="woman"?1:0;form_["appearance"]=parts;form_["style"]=style;}
    }
    if(key=="route" && answer=="hired_ceo") form_["company"]="Alder Research";
    if (key == "legal_form" || key == "route") form_["backers"] = QStringList{};
    if (key == "legal_form") {
        form_["route"] = answer == "nonprofit" ? "mission_director" : "investment";
        if (answer == "nonprofit" && form_.value("mission").toString() == "profit") form_["mission"] = "survival";
    }
    if (key == "leg" && answer != "genius") form_["researcher"] = "first_hire";
    deriveOpening();
    saveDraft();
    emit changed();
}

QString FoundingController::answerText(QString field, QString value) {
    if(field=="gender" || field=="pronouns") return tr("Select an identity option.");
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

void FoundingController::deriveOpening() {
    if(!catalog_ || !catalog_->opening) return;
    const auto leg=form_.value("leg").toString();
    if(leg=="wealthy" && form_.value("family")!="rich") form_["leg"]="genius";
    auto route=form_.value("route").toString();
    if(route=="bootstrap" && form_.value("family")!="rich" && form_.value("leg")!="political") route=form_.value("legal_form")=="nonprofit"?"mission_director":"investment";
    form_["route"]=route;
    form_["researcher"]=form_.value("leg")=="genius"?"founder":"none";
    form_["office"]=(route=="hired_ceo" || route=="partnership")?"shared":"home";
    form_["difficulty"]="";
    if(route!="investment" && route!="mission_director") form_["backers"]=QStringList{};
    if(form_.value("gender")!="woman" && form_.value("gender")!="man" && form_.value("gender")!="nonbinary" && form_.value("gender")!="unspecified") form_["gender"]="unspecified";
    if(!pronounOptions().contains(form_.value("pronouns").toString())) form_["pronouns"]="they/them";
}

QStringList FoundingController::pronounOptions() const {
    // Sources and editorial policy: docs/IMMERSIVE_COMPANY_PLAN.md.
    return {"she/her","he/him","they/them","she/her + they/them","he/him + they/them",
        "she/her + he/him","she/her + he/him + they/them","xe/xem","ze/hir","ze/zir",
        "zie/hir","ey/em","e/em","per/per","fae/faer"};
}
QString FoundingController::portrait() const {
    QString url="image://portraits/1";
    for(const auto& part:form_.value("appearance").toList()) url+='/'+QString::number(part.toInt());
    for(const auto& part:form_.value("style").toList()) url+='/'+QString::number(part.toInt());
    return url;
}
QVariantList FoundingController::appearanceParts() const {
    QVariantList rows;
    const QStringList labels{tr("Head"),tr("Hair style"),tr("Nose"),tr("Ears"),tr("Mouth"),tr("Chin"),tr("Neck"),tr("Eyes"),tr("Brows"),tr("Facial hair"),tr("Glasses"),tr("Skin tone"),tr("Hair color"),tr("Clothing"),tr("Background")};
    for(qsizetype i=0;i<labels.size();++i) rows.push_back(QVariantMap{{"name",labels[i]},{"count",core::portrait_part_counts[static_cast<std::size_t>(i)]}});
    const QStringList styles{tr("Body shape"),tr("Height"),tr("Clothing style"),tr("Shirt pattern"),tr("Eye color"),tr("Lip color"),tr("Glasses color"),tr("Pattern color")};
    for(qsizetype i=0;i<styles.size();++i) rows.push_back(QVariantMap{{"name",styles[i]},{"count",core::portrait_style_counts[static_cast<std::size_t>(i)]}});
    return rows;
}
void FoundingController::cycleAppearance(int part,int direction) {
    if(part<0 || part>=23 || (direction!=1 && direction!=-1)) return;
    const auto key=part<15?"appearance":"style";const auto index=part<15?part:part-15;
    auto values=form_.value(key).toList();if(values.size()!=(part<15?15:8)) return;
    const auto count=part<15?core::portrait_part_counts[static_cast<std::size_t>(index)]:core::portrait_style_counts[static_cast<std::size_t>(index)];
    values[index]=(values[index].toInt()+direction+count)%count;form_[key]=values;
    saveDraft();emit changed();
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
