#include "founding_controller.hpp"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>
#include <pgp/runtime/campaign_identity.hpp>
#include <pgp/core/checked_arithmetic.hpp>
#include <algorithm>

namespace pgp::bridge {
namespace {
QString text(const std::string& s) {return QString::fromStdString(s);}
QString money(std::int64_t cents) {
    const auto magnitude=cents<0?-cents:cents;
    return QStringLiteral("%1$%2.%3").arg(cents<0?QStringLiteral("−"):QString{},
        QLocale{QLocale::English,QLocale::UnitedStates}.toString(magnitude/100)).arg(magnitude%100,2,10,QLatin1Char('0'));
}
}
FoundingController::FoundingController(const content::Collection& collection,std::filesystem::path path,QObject* parent)
    :QObject(parent),draft_path_(std::move(path)) {
    if(const auto* d=collection.find("scenario.founder_campaign")) {
        const auto& scenario=std::get<content::ScenarioDefinition>(d->data);
        catalog_=scenario.founding;difficulties_=scenario.difficulties;
    }
    reset();
    QFile file{QString::fromStdU16String(draft_path_.u16string())};
    if(file.exists() && file.open(QIODevice::ReadOnly) && file.size()<=32768) {
        const auto doc=QJsonDocument::fromJson(file.readAll());
        const auto saved=doc.object().toVariantMap();
        if(saved.value("format").toString()==QStringLiteral("founding-draft-1")) {
            const auto fields=saved.value("form").toMap();
            for(auto it=fields.begin();it!=fields.end();++it) if(form_.contains(it.key())) form_[it.key()]=it.value();
            page_=std::clamp(saved.value("page").toInt(),0,catalog_?static_cast<int>(catalog_->scenes.size())+7:0);
        }
    }
}
void FoundingController::reset() {
    form_={{"mode","campaign"},{"first_name","Alex"},{"last_name","Vale"},{"gender","nonbinary"},{"pronouns","they/them"},
        {"company","Daybreak Research"},{"seed","19972015"},{"headquarters","US-CA"},{"difficulty","difficulty.standard"},
        {"sociability","introverted"},{"activity","sedentary"},{"disposition","deliberate"},
        {"family","middle"},{"leg","genius"},{"route","partnership"},{"legal_form","corporation"},
        {"office","shared"},{"researcher","founder"},{"mission","survival"},
        {"choices",QVariantMap{}},{"backers",QStringList{}},{"sandbox_cash",""},{"sandbox_staff",""}};
    page_=0;error_.clear();emit changed();
}
content::FoundingSelection FoundingController::selection() const {
    content::FoundingSelection s;
    const auto field=[&](const char* key){return form_.value(QLatin1String{key}).toString().trimmed().toStdString();};
    s.mode=field("mode");s.first_name=field("first_name");s.last_name=field("last_name");s.gender=field("gender");
    s.sociability=field("sociability");s.activity=field("activity");s.disposition=field("disposition");
    s.family=field("family");s.leg=field("leg");s.route=field("route");s.legal_form=field("legal_form");
    s.office=field("office");s.researcher=field("researcher");
    const auto choices=form_.value("choices").toMap();
    for(auto it=choices.begin();it!=choices.end();++it) s.choices.emplace(it.key().toStdString(),it.value().toString().toStdString());
    for(const auto& id:form_.value("backers").toStringList()) s.backers.push_back(id.toStdString());
    if(s.mode=="sandbox") {
        if(!field("sandbox_cash").empty()) {
            const auto dollars=runtime::parse_world_seed(field("sandbox_cash"));
            if(dollars>1000000000ULL) throw std::invalid_argument("Sandbox cash supports up to one billion dollars.");
            s.sandbox_cash_cents=static_cast<std::int64_t>(dollars*100);
        }
        if(!field("sandbox_staff").empty()) {
            const auto staff=runtime::parse_world_seed(field("sandbox_staff"));
            if(staff<1 || staff>10000) throw std::invalid_argument("Choose 1 to 10,000 research staff.");
            s.sandbox_staff=static_cast<std::uint32_t>(staff);
        }
    }
    return s;
}
QVariantList FoundingController::options(const std::vector<content::FoundingOption>& rows) const {
    QVariantList result;content::FoundingSelection s;
    s.family=form_.value("family").toString().toStdString();s.leg=form_.value("leg").toString().toStdString();
    s.legal_form=form_.value("legal_form").toString().toStdString();
    for(const auto& r:rows) {
        auto reason=content::founding_ineligibility(r,s);
        if(r.id=="wealthy" && s.family!="rich") reason="Requires rich parents at birth.";
        result.push_back(QVariantMap{{"id",text(r.id)},{"name",text(r.name)},{"detail",text(r.detail)},
            {"available",reason.empty()},{"reason",text(reason)}});
    }
    return result;
}
QVariantList FoundingController::families() const {return catalog_?options(catalog_->families):QVariantList{};}
QVariantList FoundingController::legs() const {return catalog_?options(catalog_->legs):QVariantList{};}
QVariantList FoundingController::routes() const {return catalog_?options(catalog_->routes):QVariantList{};}
QVariantList FoundingController::backers() const {return catalog_?options(catalog_->backers):QVariantList{};}
QVariantList FoundingController::offices() const {return catalog_?options(catalog_->offices):QVariantList{};}
QVariantList FoundingController::researchers() const {return catalog_?options(catalog_->researchers):QVariantList{};}
QVariantList FoundingController::chapters() const {
    QVariantList result;if(!catalog_) return result;
    for(const auto& scene:catalog_->scenes) result.push_back(QVariantMap{{"id",text(scene.id)},{"title",text(scene.title)},
        {"date",text(scene.date)},{"text",text(scene.text)},{"choices",options(scene.choices)}});
    return result;
}
QString FoundingController::preview() const {
    if(!catalog_) return {};
    try {
        const auto s=selection();const auto t=content::founding_terms(*catalog_,s,false);
        auto cash=t.contributed_cents;
        const auto difficulty=form_.value("difficulty").toString();
        const auto found=std::find_if(difficulties_.begin(),difficulties_.end(),[&](const auto& d){return d.id==difficulty.toStdString();});
        if(found==difficulties_.end()) throw std::invalid_argument("Choose an available campaign difficulty.");
        const auto percent=found->starting_cash_percent;
        if(!s.sandbox_cash_cents) cash=static_cast<std::int64_t>(core::mul_div_floor(static_cast<std::uint64_t>(cash),static_cast<std::uint64_t>(percent),100).quotient);
        const auto monthly=t.office_cents+t.payroll_cents+catalog_->compute_monthly_cents+catalog_->founder_salary_cents;
        return QStringLiteral("%1 contributed · %2 paid to establish the lab\n%3 available after founding · %4/month opening commitments\n%5 research assignment(s) · %6 scientific prestige · %7 public buzz\n%8\n%9All money uses 2014 purchasing power.")
            .arg(money(cash),money(t.paid_setup_cents),money(cash-t.paid_setup_cents),money(monthly))
            .arg(s.sandbox_staff.value_or(1)).arg(t.prestige).arg(t.buzz)
            .arg(t.nonprofit?QStringLiteral("Nonprofit: institution-owned assets, no founder equity or VC equity; mission-related income exemption."):
                QStringLiteral("Founder retains %1.%2% equity; backers and partners retain their signed rights.").arg(t.founder_bp/100).arg(t.founder_bp%100,2,10,QLatin1Char('0')))
            .arg(s.mode=="campaign"?QStringLiteral("2015: age 18. Campaign closes at age 118 in 2115. "):QString{});
    } catch(const std::exception& e) {return QString::fromUtf8(e.what());}
}
void FoundingController::saveDraft() {
    try {
        std::filesystem::create_directories(draft_path_.parent_path());
        QSaveFile file{QString::fromStdU16String(draft_path_.u16string())};
        const auto bytes=QJsonDocument{QJsonObject::fromVariantMap({{"format","founding-draft-1"},{"page",page_},{"form",form_}})}.toJson(QJsonDocument::Compact);
        if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) error_=QStringLiteral("Could not save the opening draft. You can continue this session.");
    } catch(const std::exception& e) {error_=QString::fromUtf8(e.what());}
}
void FoundingController::setPage(int p) {page_=std::clamp(p,0,catalog_?static_cast<int>(catalog_->scenes.size())+7:0);saveDraft();emit changed();}
void FoundingController::setField(QString key,QVariant value) {
    if(!form_.contains(key) || key=="choices" || key=="backers" || value.toString().size()>128) return;
    form_[key]=value.toString();error_.clear();saveDraft();emit changed();
}
void FoundingController::choose(QString scene,QString id) {
    if(!catalog_) return;
    const auto it=std::find_if(catalog_->scenes.begin(),catalog_->scenes.end(),[&](const auto& r){return r.id==scene.toStdString();});
    if(it==catalog_->scenes.end() || std::none_of(it->choices.begin(),it->choices.end(),[&](const auto& r){return r.id==id.toStdString();})) return;
    auto choices=form_.value("choices").toMap();choices[scene]=id;form_["choices"]=choices;saveDraft();emit changed();
}
void FoundingController::back(QString id,bool selected) {
    auto rows=form_.value("backers").toStringList();rows.removeAll(id);if(selected) rows.push_back(id);
    form_["backers"]=rows;saveDraft();emit changed();
}
}
