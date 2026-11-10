#include "founding_controller.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>
#include <pgp/core/checked_arithmetic.hpp>
#include <pgp/core/objectives.hpp>
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
    return {{"first_name","Alex"},{"last_name","Vale"},{"birth_sex","indeterminate"},{"gender","nonbinary"},{"pronouns","they/them"},{"orientation","questioning"},
        {"home_app","room"},{"home_room","Bedroom"},{"home_guide",""},{"home_offer",""},{"home_replies",QVariantMap{}},{"company_draft","Daybreak Research"},
        {"investment","-1"},{"charter",""},{"company","Daybreak Research"},{"seed","19972015"},{"headquarters","US-CA"},{"difficulty","difficulty.standard"},
        {"sociability","introverted"},{"activity","sedentary"},{"disposition","deliberate"},
        {"family","middle"},{"leg","genius"},{"route","investment"},{"legal_form","corporation"},
        {"office","home"},{"researcher","founder"},{"mission","survival"},{"mission_draft","survival"},
        {"style",QVariantList{0,0,0,0,0,0,0,0,0,0,0,0}},{"appearance",QVariantList{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
        {"choices",QVariantMap{}},{"backers",QStringList{}},{"partner",""}};
}

bool eligible_ceo(const content::FoundingSelection& selection) {
    return selection.leg == "genius" || selection.leg == "political" || selection.leg == "wealthy";
}

QString percent(std::uint32_t basis_points) {
    return QLocale().toString(static_cast<double>(basis_points)/100, 'f', basis_points % 100 ? 2 : 0);
}

std::int64_t backer_amount(const content::FoundingCatalog& catalog, const content::FoundingTerms& terms,
                          const content::FoundingOption& backer) {
    const auto premium=std::min(catalog.maximum_finance_bonus_bp,
        terms.prestige*catalog.prestige_finance_bp+terms.buzz*catalog.buzz_finance_bp);
    return static_cast<std::int64_t>(core::mul_div_floor(static_cast<std::uint64_t>(backer.capital_cents),10000+premium,10000).quotient);
}

void prepare_offer(content::FoundingSelection& selected, std::string legal_form, std::string route) {
    if (selected.route != route || selected.legal_form != legal_form) selected.backers.clear();
    selected.legal_form=std::move(legal_form);
    selected.route=std::move(route);
    selected.office=selected.route=="hired_ceo" || selected.route=="partnership" ? "shared" : "home";
    if(selected.route!="partnership") selected.partner.clear();
    if (selected.personal_investment_cents<0) selected.personal_investment_cents=0;
}
}

FoundingController::FoundingController(const content::Collection& collection, std::filesystem::path path, QObject* parent)
    : QObject(parent), draft_path_(std::move(path)), form_(defaults()) {
    if (const auto* definition = collection.find("scenario.founder_campaign")) {
        const auto& scenario = std::get<content::ScenarioDefinition>(definition->data);
        catalog_ = scenario.founding;
        starts_on_=scenario.starts_on;
        headquarters_ = scenario.headquarters_choices;
        if(scenario.infrastructure && scenario.infrastructure->workplace) premises_rules_=scenario.infrastructure->workplace->headquarters;
        if(scenario.management && scenario.management->governance)charters_=scenario.management->governance->charters;
        if(scenario.management && scenario.management->people)audiences_=scenario.management->people->founder_audiences;
    }
    deriveOpening();
    if (!available()) return;
    QFile file{QString::fromStdU16String(draft_path_.u16string())};
    if (!file.open(QIODevice::ReadOnly) || file.size() > 32768) return;
    const auto saved = QJsonDocument::fromJson(file.readAll()).object();
    const auto format = saved.value("format").toString();
    if (format != QStringLiteral("founding-draft-5")) return;
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
    selected.birth_sex = field("birth_sex");
    selected.orientation = field("orientation");
    selected.sociability = field("sociability");
    selected.activity = field("activity");
    selected.disposition = field("disposition");
    selected.family = field("family");
    selected.leg = field("leg");
    selected.route = field("route");
    selected.partner = field("partner");
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

QVariantList FoundingController::families() const {
    if(!catalog_)return {};
    auto rows=options(catalog_->families);
    for(auto& value:rows) {
        auto row=value.toMap();const auto id=row.value("id").toString().toStdString();
        row["detail"]=row.value("detail").toString()+tr(" · Family savings at 18: %1").arg(money(content::founding_option(catalog_->families,id).capital_cents));
        if(premises_rules_)row["detail"]=row.value("detail").toString()+tr(" · %1 free home workspaces").arg(core::headquarters_home(*premises_rules_,id).plan.rooms.size());
        value=row;
    }
    return rows;
}
QVariantList FoundingController::legs() const { return catalog_ ? options(catalog_->legs) : QVariantList{}; }
QVariantList FoundingController::routes() const {
    return catalog_?options(catalog_->routes):QVariantList{};
}
QVariantList FoundingController::backers() const {
    if (!catalog_) return {};
    if(catalog_->opening && form_.value("route")!="investment" && form_.value("route")!="mission_director") return {};
    return options(catalog_->backers);
}

QVariantMap FoundingController::homeOffer(const QString& id) const {
    if (beat_!="home" || !catalog_ || !catalog_->opening) return {};
    const auto reply=form_.value("home_replies").toMap().value(id).toMap();
    auto status=reply.value("status").toString();
    auto source=reply.value("source").toString();
    const auto selected=selection();
    if(status.isEmpty() && id=="alder" && selected.leg=="genius") {status="offered";source="recruiter";}
    if(status.isEmpty() && id=="frontier" && selected.leg=="genius" && selected.family!="rich" &&
        selected.route!="hired_ceo" && selected.route!="partnership" && selected.backers.empty()) {
        status="offered";source="introduction";
    }
    const bool selected_in_draft=id=="alder"?selected.route=="hired_ceo":id==text(selected.partner) ||
        std::ranges::find(selected.backers,id.toStdString())!=selected.backers.end();
    if(selected_in_draft && status=="considering") status="selected";
    return presentOffer(*catalog_,selected,id,source,status);
}

QVariantMap FoundingController::presentOffer(const content::FoundingCatalog& catalog,content::FoundingSelection selected,
                                            const QString& id,const QString& source,const QString& status) {
    if(!catalog.opening) return {};
    QVariantMap row{{"id",id}};
    if (id=="alder") {
        const auto route=std::ranges::find(catalog.routes,std::string_view{"hired_ceo"},&content::FoundingOption::id);
        if (route==catalog.routes.end()) return {};
        row["kind"]="career";
        row["name"]=tr("Leena Rao");
        row["organization"]=tr("Alder Research · Recruiting");
        row["subject"]=tr("A seat at the table");
        row["available"]=true;
        row["body"]=status=="rejected"
            ? tr("Thank you for applying. We are still in the early planning stages and cannot offer you the leadership role. I wanted you to hear that directly from me.")
            : source=="recruiter"
                ? tr("Your work reached our board before your résumé did. We need someone who can decide what is worth building. Would you consider leading Alder Research?")
                : tr("The board reviewed your application and would like to offer you the chief executive role. I can send the agreement for your signature.");
        row["terms"]=tr("%1 in company cash · %2 inherited employees · shared office · %3% equity · %4 per month salary.")
            .arg(money(route->capital_cents)).arg(catalog.opening->inherited_staff)
            .arg(percent(catalog.opening->ceo_equity_bp),money(catalog.founder_salary_cents));
    } else if(const auto* partner=content::founding_partner(catalog,id.toStdString())) {
        const auto reason=content::partner_ineligibility(*partner,selected);
        row["kind"]="partner";row["name"]=text(partner->name);row["organization"]=tr("Prospective cofounder");
        row["subject"]=tr("An offer to build together");row["body"]=text(partner->letter);row["available"]=reason.empty();
        row["terms"]=tr("%1 at signing · %2% equity · %3 board vote(s) · %4/month salary\nOpening mission · %5\nSupports proposals with company cash of at least %6 afterward and dilution of at most %7%.")
            .arg(money(partner->capital_cents),percent(partner->shares_bp)).arg(partner->seats).arg(money(partner->monthly_salary_cents),
                QString::fromStdString(std::string{core::objective_name(*core::objective_kind(partner->objective))}),
                money(partner->cash_floor_cents),percent(partner->maximum_dilution_bp));
        if(!reason.empty()) row["terms"]=text(reason)+"\n"+row["terms"].toString();
        const auto recipe=core::make_portrait(19972015,"cofounder/"+partner->id);
        auto portrait=QStringLiteral("image://portraits/1");
        for(auto part:recipe.parts) portrait+='/'+QString::number(part);
        for(auto style:recipe.style) portrait+='/'+QString::number(style);
        row["portrait"]=portrait;
    } else {
        const auto backer=std::ranges::find(catalog.backers,id.toStdString(),&content::FoundingOption::id);
        if (backer==catalog.backers.end()) return {};
        const auto legal=backer->legal_form.empty()?selected.legal_form:backer->legal_form;
        const bool nonprofit=legal=="nonprofit";
        prepare_offer(selected,legal,nonprofit?"mission_director":"investment");
        const auto reason=content::founding_ineligibility(*backer,selected);
        row["kind"]=nonprofit?"donor":"investor";
        row["name"]=id=="frontier"?tr("Zora Bell"):text(backer->name);
        row["organization"]=id=="frontier"?text(backer->name):nonprofit?tr("Grant office"):tr("Investment desk");
        row["subject"]=nonprofit?tr("Research grant"):tr("Founding investment");
        row["available"]=reason.empty();
        row["body"]=(source=="introduction"
            ? tr("We have been following your work. If you decide to start a lab of your own, we would like to be your first investor.\n\n")
            : tr("Thank you for bringing us your proposal. Here is what our committee can commit to at signing.\n\n"))+text(backer->detail);
        if (reason.empty()) {
            // Price the same biography without optional commitments in other drafts.
            selected.personal_investment_cents=0;
            selected.backers.clear();
            try {
                const auto base=content::founding_terms(catalog,selected,false);
                row["terms"]=tr("%1 · %2 at signing · %3").arg(nonprofit?tr("Nonprofit"):tr("Corporation"),money(backer_amount(catalog,base,*backer)),
                    nonprofit?tr("one trustee seat, no equity"):tr("one board seat, %1% equity").arg(percent(backer->shares_bp)));
            } catch (const std::exception& error) {
                row["available"]=false;
                row["terms"]=QString::fromUtf8(error.what());
            }
        } else row["terms"]=text(reason);
    }
    row["status"]=status.isEmpty()?QStringLiteral("listed"):status;
    row["source"]=source;
    row["statusText"]=status=="offered"?tr("Offer ready"):status=="considering"?tr("Saved for later"):
        status=="selected"?tr("Selected for signing"):status=="declined"?tr("Declined"):status=="rejected"?tr("Application closed"):tr("Available to contact");
    return row;
}

QVariantList FoundingController::homeOffers() const {
    if (beat_!="home" || !catalog_ || !catalog_->opening) return {};
    QVariantList offers;
    if (auto alder=homeOffer("alder"); !alder.isEmpty()) offers.push_back(std::move(alder));
    for (const auto& backer:catalog_->backers) offers.push_back(homeOffer(text(backer.id)));
    for (const auto& partner:catalog_->partners) offers.push_back(homeOffer(text(partner.id)));
    return offers;
}

std::vector<core::PeopleContact> FoundingController::openingContacts() const {
    std::vector<core::PeopleContact> contacts;
    const auto replies=form_.value("home_replies").toMap();
    for(auto reply=replies.cbegin();reply!=replies.cend();++reply) {
        const auto offer=homeOffer(reply.key());
        const auto status=offer.value("status").toString();
        if(status.isEmpty() || status=="listed") continue;
        const auto outcome=status=="selected"?QStringLiteral("signed"):
            status=="declined" || status=="rejected"?status:QStringLiteral("unsigned");
        contacts.push_back({std::string{core::opening_contact_prefix}+reply.key().toStdString(),starts_on_,{},
            core::OpeningConversation{offer.value("source").toString().toStdString(),outcome.toStdString()}});
    }
    return contacts;
}

QString FoundingController::commitHomeDraft(QVariantMap next) {
    form_.swap(next);
    deriveOpening();
    saveDraft();
    if (!error_.isEmpty()) form_.swap(next);
    emit changed();
    return error_;
}

QString FoundingController::contactHomeOffer(QString id) {
    const auto offer=homeOffer(id);
    if (!offer.value("available").toBool()) return tr("This opportunity is unavailable to you.");
    auto replies=form_.value("home_replies").toMap();
    if (!replies.contains(id)) {
        auto status=offer.value("status").toString();
        auto source=offer.value("source").toString();
        if (status=="listed") {
            status=id=="alder" && !eligible_ceo(selection())?"rejected":"offered";
            source=id=="alder"?"application":offer.value("kind")=="partner"?"partnership":"pitch";
        }
        replies[id]=QVariantMap{{"status",status},{"source",source}};
    }
    if (form_.value("home_replies")==replies && form_.value("home_offer")==id) return {};
    auto next=form_;
    next["home_offer"]=id;
    next["home_replies"]=replies;
    return commitHomeDraft(std::move(next));
}

QString FoundingController::closeHomeOffer() {
    if (beat_!="home" || form_.value("home_offer").toString().isEmpty()) return {};
    auto next=form_;
    next["home_offer"]="";
    return commitHomeDraft(std::move(next));
}

QString FoundingController::respondHomeOffer(QString id, QString response) {
    const auto offer=homeOffer(id);
    auto replies=form_.value("home_replies").toMap();
    auto reply=replies.value(id).toMap();
    if (offer.isEmpty() || form_.value("home_offer")!=id ||
        (reply.value("status")!="offered" && reply.value("status")!="considering"))
        return tr("Open an active offer before replying.");
    if (response!="accept" && response!="later" && response!="decline") return tr("Choose a reply to the offer.");
    auto selected=selection();
    if (response=="accept") {
        if (id=="alder") {
            prepare_offer(selected,"corporation","hired_ceo");
        } else if(const auto* partner=content::founding_partner(*catalog_,id.toStdString())) {
            prepare_offer(selected,"corporation","partnership");
            selected.partner=partner->id;
        } else {
            const auto& backer=content::founding_option(catalog_->backers,id.toStdString());
            const auto legal=backer.legal_form.empty()?selected.legal_form:backer.legal_form;
            prepare_offer(selected,legal,legal=="nonprofit"?"mission_director":"investment");
            if (std::ranges::find(selected.backers,backer.id)==selected.backers.end()) selected.backers.push_back(backer.id);
        }
        try {
            (void)content::founding_terms(*catalog_,selected,false);
        } catch (const std::exception& error) {
            return QString::fromUtf8(error.what());
        }
    } else if (offer.value("status")=="selected") {
        if (id=="alder") prepare_offer(selected,"corporation","investment");
        else if(id==text(selected.partner)) prepare_offer(selected,"corporation","investment");
        else std::erase(selected.backers,id.toStdString());
    }
    const bool was_ceo=form_.value("route")=="hired_ceo";
    const bool is_ceo=selected.route=="hired_ceo";
    auto next=form_;
    if(form_.value("partner").toString().isEmpty() && !selected.partner.empty()) next["mission_draft"]=form_.value("mission");
    if(const auto* partner=content::founding_partner(*catalog_,selected.partner)) next["mission"]=text(partner->objective);
    else if(!form_.value("partner").toString().isEmpty()) next["mission"]=form_.value("mission_draft","survival");
    next["partner"]=text(selected.partner);
    if (!was_ceo && is_ceo) {
        next["company_draft"]=form_.value("company");
        next["company"]="Alder Research";
        next["charter"]="";
    } else if (was_ceo && !is_ceo) next["company"]=form_.value("company_draft");
    if (response=="accept" && form_.value("investment")=="-1") next["investment"]="0";
    next["route"]=text(selected.route);
    next["legal_form"]=text(selected.legal_form);
    QStringList backers;
    for (const auto& backer:selected.backers) backers.push_back(text(backer));
    next["backers"]=backers;
    reply["status"]=response=="decline"?"declined":"considering";
    replies[id]=reply;
    next["home_replies"]=replies;
    return commitHomeDraft(std::move(next));
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
QVariantMap FoundingController::familyHome() const {
    if(!premises_rules_)return {};
    const auto family=form_.value("leg")=="political"?std::string{"rich"}:form_.value("family").toString().toStdString();
    const auto home=core::headquarters_home(*premises_rules_,family);
    QVariantList rooms;QStringList names;
    for(std::size_t i=0;i<home.plan.rooms.size();++i) {
        const auto& r=home.plan.rooms[i];const auto name=text(home.room_names[i]);names.push_back(name);
        rooms.push_back(QVariantMap{{"name",name},{"x",r.x},{"y",r.y},{"width",r.width},{"depth",r.depth}});
    }
    return {{"name",text(home.plan.name)},{"width",home.plan.width},{"depth",home.plan.depth},{"rooms",rooms},{"roomNames",names}};
}
QVariantList FoundingController::charters() const {
    QVariantList rows{QVariantMap{{"id",""},{"name",tr("No optional bylaws")},{"detail",tr("Use the ordinary board terms, with no additional employee-program commitment.")}}};
    for(const auto& c:charters_) rows.push_back(QVariantMap{{"id",text(c.id)},{"name",text(c.name)},
        {"detail",tr("%1\nEmployees expect %2\nBoard cash floor %3; dilution ceiling %4%. Employee program %5/month. Stricter existing board terms still apply. Later amendments require a board majority.")
            .arg(text(c.priorities),text(c.employee_expectations),money(c.cash_floor.value())).arg(static_cast<double>(c.maximum_dilution.value())/100).arg(money(c.monthly_employee_budget.value()))}});
    return rows;
}
QVariantMap FoundingController::startupLoan() const {
    if(!catalog_ || !catalog_->startup_loan) return {};
    const auto& loan=*catalog_->startup_loan;
    const auto due=core::advance_days(starts_on_,core::Days{loan.term_days});
    const auto monthly=core::mul_div_round_half_up(static_cast<std::uint64_t>(loan.principal_cents),loan.monthly_interest_bp,10000);
    return {{"lender",text(loan.lender)},{"detail",tr("%1 paid into company accounts at signing.\nInterest %2% per month on outstanding principal (%3 for a full month at the opening balance).\nRemaining principal due %4. Repay early from company accounts.\nApproval requires at least %5 of your own savings invested in the company. The bank takes no shares.")
        .arg(money(loan.principal_cents)).arg(static_cast<double>(loan.monthly_interest_bp)/100).arg(money(static_cast<std::int64_t>(monthly)),text(core::iso_date(due)),money(loan.minimum_investment_cents))}};
}
QString FoundingController::genderAudience(QString gender) const {
    content::FoundingSelection identity;
    identity.birth_sex=form_.value("birth_sex").toString().toStdString();
    identity.gender=gender.toStdString();
    identity.orientation="straight";
    return content::queer_founder(identity) ? communitySupport(gender) :
        tr("Your identity does not set employees’ pay or workplace mood.");
}
QString FoundingController::communitySupport(QString gender) const {
    if(!catalog_) return {};
    content::FoundingSelection identity;
    identity.birth_sex=form_.value("birth_sex").toString().toStdString();
    identity.gender=gender.toStdString();
    identity.orientation=form_.value("orientation").toString().toStdString();
    if(!content::queer_founder(identity)) return {};
    return recognitionTerms();
}
QString FoundingController::recognitionTerms() const {
    if(!catalog_) return {};
    return tr("Queer community recognition: +%1 company prestige and +%2 public founding buzz. Applied once across gender and orientation. Company mood and funding conversations use this reputation. Employees keep their own pay preferences.")
        .arg(catalog_->queer_community_prestige).arg(catalog_->queer_community_buzz);
}
QString FoundingController::orientationAudience(QString orientation) const {
    auto identity=selection();identity.orientation=orientation.toStdString();
    return content::queer_founder(identity)?recognitionTerms():tr("No additional community recognition from this orientation.");
}

QString FoundingController::signingError() const {
    if(!catalog_) return tr("The founding catalog is unavailable.");
    try {
        const auto selected=selection();
        if(selected.route=="hired_ceo" && (!eligible_ceo(selected) ||
            (beat_=="home" && homeOffer("alder").value("status")!="selected")))
            return tr("Review and accept a leadership offer before signing an appointment.");
        if(selected.route=="partnership" && !selected.partner.empty() && beat_=="home" &&
            homeOffer(text(selected.partner)).value("status")!="selected")
            return tr("Review and accept your cofounder's offer before signing.");
        (void)content::founding_terms(*catalog_,selected);
        const auto charter=form_.value("charter").toString().toStdString();
        if(!charter.empty() && (selected.route=="hired_ceo" || std::ranges::none_of(charters_,[&](const auto& c){return c.id==charter;}))) return tr("Choose an available founding charter. A hired CEO inherits the company's existing terms.");
        runtime::validate_campaign_identity({form_.value("company").toString().trimmed().toStdString(),selected.first_name+" "+selected.last_name,form_.value("pronouns").toString().toStdString()});
        return {};
    } catch(const std::exception& e) {return QString::fromUtf8(e.what());}
}
QString FoundingController::preview() const {
    if (!catalog_) return {};
    try {
        const auto terms = content::founding_terms(*catalog_, selection());
        const auto cash=core::checked_add(terms.contributed_cents,terms.startup_loan?terms.startup_loan->principal_cents:0);
        const auto equity = terms.nonprofit ? tr("The institution owns its assets.") :
            tr("Your ownership: %1.%2%").arg(terms.founder_bp / 100).arg(terms.founder_bp % 100, 2, 10, QLatin1Char('0'));
        {
            const auto selected=selection();
            QStringList sources;
            if(selected.route!="hired_ceo") {
                sources<<tr("Your investment %1 · personal savings retained %2").arg(money(terms.personal_contribution_cents),money(terms.personal_remaining_cents));
            }
            if(terms.startup_loan) sources<<tr("%1 loan: %2 borrowed by the company").arg(text(terms.startup_loan->lender),money(terms.startup_loan->principal_cents));
            const auto& route=content::founding_option(catalog_->routes,selected.route);
            if(route.capital_cents>0 && selected.route!="mission_director") sources<<tr("%1: %2").arg(text(route.name),money(route.capital_cents));
            if(const auto* partner=content::founding_partner(*catalog_,selected.partner))
                sources<<tr("%1: %2 for %3% equity and %4 board vote(s). Opening mission · %5")
                    .arg(text(partner->name),money(partner->capital_cents),percent(partner->shares_bp)).arg(partner->seats)
                    .arg(QString::fromStdString(std::string{core::objective_name(*core::objective_kind(partner->objective))}));
            for(const auto& id:selected.backers) {
                const auto& offer=content::founding_option(catalog_->backers,id);
                sources<<tr("%1: %2").arg(text(offer.name),money(backer_amount(*catalog_,terms,offer)));
            }
            const auto desks=selected.office=="home"?0U:terms.opening_staff+(terms.founder_researcher?0U:1U);
            std::int64_t compute=0;
            if(premises_rules_) for(const auto& offer:premises_rules_->equipment) if(offer.id=="desktop") compute=offer.compute.value()*desks;
            const auto equipment=premises_rules_ && desks?tr("Included with the office: %1 desks and %1 desktop computers. You bring your phone and laptop; no compute lease.").arg(desks):selected.office=="home"?tr("You bring your existing phone and laptop; no desktop or compute lease."):tr("The office includes a workstation; you bring your phone and laptop. No compute lease.");
            QString bylaws;
            for(const auto& c:charters_)if(c.id==form_.value("charter").toString().toStdString())bylaws=tr("\nOptional bylaws: %1 · employee program %2/mo").arg(text(c.name),money(c.monthly_employee_budget.value()));
            return tr("%1\nCash %2 · signing %3\nRent %4/mo · payroll %5/mo · your salary %6/mo\n%7\n%8 research-capable people · %9 CU/day\n%10")
                .arg(sources.join(" · "),money(cash-terms.paid_setup_cents),money(terms.paid_setup_cents),money(terms.office_cents),money(terms.payroll_cents),money(catalog_->founder_salary_cents),equity).arg(terms.opening_staff).arg(static_cast<double>(compute)/1000).arg(equipment)+bylaws;
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
            {{"format","founding-draft-5"},{"beat",beat_},{"form",form_}})}.toJson(QJsonDocument::Compact);
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
    if (beat_=="home") {
        auto replies=form_.value("home_replies").toMap();
        for (const auto& offer_id : {QStringLiteral("alder"),QStringLiteral("frontier")}) {
            if (replies.contains(offer_id)) continue;
            const auto offer=homeOffer(offer_id);
            if (offer.value("status")=="offered")
                replies[offer_id]=QVariantMap{{"status","offered"},{"source",offer.value("source")}};
        }
        form_["home_replies"]=replies;
    }
    saveDraft();
    emit changed();
    emit beatChanged();
}

void FoundingController::setField(QString key, QVariant value) {
    const auto answer = value.toString();
    if (!form_.contains(key) || key == "choices" || key == "backers" || key=="partner" || key=="mission_draft" || key=="home_replies" || key=="home_offer" || key=="company_draft" ||
        key=="appearance" || key=="style" || answer.size() > 128 ||
        form_.value(key).toString() == answer) return;
    if (key=="route" && answer=="hired_ceo" && beat_=="home") return;
    if (key=="mission" && !form_.value("partner").toString().isEmpty()) return;
    if(key=="gender" && answer!="woman" && answer!="man" && answer!="nonbinary") return;
    if(key=="birth_sex" && answer!="female" && answer!="male" && answer!="indeterminate") return;
    if(key=="orientation" && std::ranges::find(content::founder_orientations,answer.toStdString())==content::founder_orientations.end()) return;
    if(key=="pronouns" && !pronounOptions().contains(answer)) return;
    if(key=="investment") {bool ok=false;const auto dollars=answer.toLongLong(&ok);if(!ok || dollars < -1 || dollars>10000000000)return;}
    const bool was_ceo=form_.value("route")=="hired_ceo";
    form_[key] = answer;
    if(key=="gender" || key=="birth_sex") {
        auto parts=form_.value("appearance").toList(),style=form_.value("style").toList();
        if(parts.size()==15 && style.size()==12) {
            if(key=="birth_sex") style[0]=answer=="female"?5:answer=="male"?0:2;
            else {parts[1]=answer=="woman"?2:answer=="man"?0:1;parts[9]=0;style[2]=answer=="woman"?1:0;}
            form_["appearance"]=parts;form_["style"]=style;
        }
    }
    if(key=="route" && answer=="hired_ceo") {form_["company_draft"]=form_.value("company");form_["company"]="Alder Research";form_["charter"]="";}
    if (key == "legal_form" || key == "route") form_["backers"] = QStringList{};
    if (key == "legal_form") {
        form_["route"] = answer == "nonprofit" ? "mission_director" : "investment";
        if (answer == "nonprofit" && form_.value("mission").toString() == "profit") form_["mission"] = "survival";
    }
    if (was_ceo && form_.value("route")!="hired_ceo") form_["company"]=form_.value("company_draft");
    if (key == "leg" && answer != "genius") form_["researcher"] = "first_hire";
    deriveOpening();
    saveDraft();
    emit changed();
}

QString FoundingController::answerText(QString field, QString value) {
    if(field=="gender" || field=="birth_sex" || field=="pronouns" || field=="orientation") return tr("Select an identity option.");
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
    if(route=="bootstrap" && content::founding_option(catalog_->routes,"bootstrap").family=="rich" && form_.value("family")!="rich" && form_.value("leg")!="political") route=form_.value("legal_form")=="nonprofit"?"mission_director":"investment";
    form_["route"]=route;
    form_["researcher"]=form_.value("leg")=="genius"?"founder":"none";
    form_["office"]=(route=="hired_ceo" || route=="partnership")?"shared":"home";
    form_["difficulty"]="";
    if(route!="investment" && route!="mission_director") form_["backers"]=QStringList{};
    if(!form_.value("partner").toString().isEmpty()) {
        const auto* partner=content::founding_partner(*catalog_,form_.value("partner").toString().toStdString());
        content::FoundingSelection biography;
        biography.leg=form_.value("leg").toString().toStdString();
        biography.family=form_.value("family").toString().toStdString();
        if(route!="partnership" || !partner || !content::partner_ineligibility(*partner,biography).empty()) {
            form_["partner"]="";
            form_["mission"]=form_.value("mission_draft","survival");
        } else form_["mission"]=text(partner->objective);
    }
    if(form_.value("gender")!="woman" && form_.value("gender")!="man" && form_.value("gender")!="nonbinary") form_["gender"]="nonbinary";
    if(form_.value("birth_sex")!="female" && form_.value("birth_sex")!="male" && form_.value("birth_sex")!="indeterminate") form_["birth_sex"]="indeterminate";
    if(std::ranges::find(content::founder_orientations,form_.value("orientation").toString().toStdString())==content::founder_orientations.end()) form_["orientation"]="questioning";
    if(!pronounOptions().contains(form_.value("pronouns").toString())) form_["pronouns"]="they/them";
}

QStringList FoundingController::pronounOptions() const {
    // Sources and editorial policy: docs/IMMERSIVE_COMPANY_PLAN.md.
    return {"she/her","he/him","they/them","e/em","ey/em","fae/faer","hu/hum","hir/hir",
        "per/per","ve/ver","xe/xem","ze/hir","ze/zir","zie/hir","thon/thon"};
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
    const QVariantList palettes{
        QStringList{"#f3d3b3","#e4b08a","#cb936c","#b57952","#995f43","#7b4936","#5c392e","#dfbaa0"},
        QStringList{"#273332","#46352e","#714b32","#a96c3d","#c3a371","#674a43","#a6a9a0","#d8d7c7"},
        QStringList{"#356a69","#a46a4c","#4f6684","#756487","#748259","#b18b48"},
        QStringList{"#c5d6c6","#dfcead","#b8cdcd","#d3c5bf","#bbc4d2","#c9cbb7"}};
    for(qsizetype i=0;i<labels.size();++i) rows.push_back(QVariantMap{{"name",labels[i]},{"count",core::portrait_part_counts[static_cast<std::size_t>(i)]},
        {"palette",i>=11?palettes[i-11]:QVariant{}}});
    const QStringList styles{tr("Body shape"),tr("Height"),tr("Clothing style"),tr("Shirt pattern"),tr("Eye color"),tr("Lip color"),tr("Glasses color"),tr("Pattern color"),tr("Brow color"),tr("Facial hair color"),tr("Jacket color"),tr("Tie color")};
    const QVariantList stylePalettes{QVariant{},QVariant{},QVariant{},QVariant{},
        QStringList{"#64766a","#527c9c","#714e38","#443c38","#a4a177","#888e92","#548a78","#7b6590"},
        QStringList{"#804d4b","#a86e65","#be8a7a","#804c68","#604848","#c18d89"},
        QStringList{"#ead4ae","#374f67","#bd8581","#80ac91","#b8a2ca","#e4dfd3"},
        QStringList{"#ead4ae","#374f67","#bd8581","#80ac91","#b8a2ca","#e4dfd3"}};
    for(qsizetype i=0;i<styles.size();++i) rows.push_back(QVariantMap{{"name",styles[i]},{"count",core::portrait_style_counts[static_cast<std::size_t>(i)]},
        {"palette",i<8?stylePalettes[i]:palettes[1]}});
    return rows;
}
void FoundingController::cycleAppearance(int part,int direction) {
    if(part<0 || part>=27 || (direction!=1 && direction!=-1)) return;
    const auto key=part<15?"appearance":"style";const auto index=part<15?part:part-15;
    auto values=form_.value(key).toList();if(values.size()!=(part<15?15:12)) return;
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
    if (beat_=="home") return respondHomeOffer(std::move(id),accept?"accept":"decline");
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

void pgp::bridge::FoundingController::setAppearance(int part,int value) {
    if(part<0 || part>=27) return;
    const auto count=part<15?core::portrait_part_counts[static_cast<std::size_t>(part)]:core::portrait_style_counts[static_cast<std::size_t>(part-15)];
    if(value<0 || value>=count) return;
    const auto key=part<15?"appearance":"style";auto values=form_.value(key).toList();
    if(values.size()!=(part<15?15:12)) return;
    values[part<15?part:part-15]=value;form_[key]=values;saveDraft();emit changed();
}
