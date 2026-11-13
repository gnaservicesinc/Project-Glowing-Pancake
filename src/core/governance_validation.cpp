#include "governance_internal.hpp"
#include <map>
#include <set>

namespace pgp::core {
using namespace governance_detail;
namespace {
void label(const std::string& s,std::size_t max=128,bool empty=false) {
    if ((!empty && s.empty()) || s.size()>max || s.find('\0')!=std::string::npos) invalid("Corporate text exceeds its supported bounds");
}
const Posting& posting(const SimulationState& s,std::string_view key,OrdinalDay day,Cents amount,Account debit,Account credit) {
    for (const auto& p:s.accounts->postings) if (p.key==key) {
        if (p.day!=day || p.amount!=amount || p.debit!=debit || p.credit!=credit) invalid("Corporate consideration differs from its actual posting");
        return p;
    }
    invalid("Corporate consideration has no actual posting");
}
Cents cash_at(const AccountsState& accounts,std::uint64_t cursor,OrdinalDay day) {
    if (cursor>accounts.postings.size()) invalid("Corporate evidence cursor exceeds actual postings");
    std::int64_t cash=0;
    for (std::size_t i=0;i<cursor;++i) {
        const auto& p=accounts.postings[i];if (p.day>day) invalid("Corporate evidence includes future postings");
        if (p.debit==Account::cash) cash=checked_add(cash,p.amount.value());
        if (p.credit==Account::cash) cash=checked_subtract(cash,p.amount.value());
    }
    return Cents{cash};
}
}
void validate_governance_catalog(const GovernanceCatalog& c) {
    if (c.charters.size()>8 || c.financing.size()>8 || c.executives.size()>8 || c.board.empty() || c.board.size()>8 ||
        c.vote_window.value()<7 || c.vote_window.value()>60 || c.concern_period.value()<7 || c.concern_period>=c.response_window ||
        c.response_window.value()>90 || c.maximum_pool_expansion.value()==0 || c.maximum_pool_expansion.value()>2000 ||
        c.acquisition_equity.value()==0 || c.acquisition_equity.value()>2000 || c.acquisition_premium.value()<0 || c.acquisition_premium.value()>100000000 ||
        c.subsidiary_cash_floor.value()<0 || c.subsidiary_cash_floor.value()>100000000 || c.subsidiary_reserve_days.value()<7 || c.subsidiary_reserve_days.value()>90)
        invalid("Corporate catalogs require finite response windows, dilution and consideration");
    std::set<std::string> ids;
    for (const auto& f:c.financing) {
        label(f.id,64);label(f.name);label(f.negotiation,64);
        if (!ids.insert(f.id).second || f.proceeds.value()<=0 || f.proceeds.value()>1000000000 || f.covenant_cash_floor.value()<0 ||
            f.waiver_fee.value()<0 || f.waiver_fee>f.proceeds || f.monthly_interest.value()>1000 ||
            (f.donation ? f.acquired.value()!=0 || f.term.value()!=0 || f.monthly_interest.value()!=0 || f.covenant_cash_floor.value()!=0 || f.waiver_fee.value()!=0:
             f.term.value()==0 ? f.acquired.value()==0 || f.acquired.value()>3000 || f.monthly_interest.value()!=0 || f.covenant_cash_floor.value()!=0 || f.waiver_fee.value()!=0:
             f.term.value()<30 || f.term.value()>1096 || f.acquired.value()!=0 || (f.covenant_cash_floor.value()>0 && f.waiver_fee.value()==0))) invalid("Negotiated finance needs explicit cash, equity/debt and covenant terms");
    }
    if(c.contact_finance) {
        const auto& rules=*c.contact_finance;
        if(rules.retry<Days{7} || rules.retry>Days{365} || rules.validity<Days{7} || rules.validity>rules.retry || rules.offers.empty() || rules.offers.size()>8)
            invalid("Contact funding needs bounded offer, retry and validity windows.");
        std::set<std::string> offers;
        for(const auto& offer:rules.offers) {
            label(offer.person,72);label(offer.terms,64);
            const auto& terms=find(c.financing,offer.terms);
            if(!offer.person.starts_with("opening/") || !offers.insert(offer.terms).second || offer.minimum_reputation>100 ||
                offer.minimum_publications>64 || offer.minimum_evaluations>64 || terms.term!=Days{0} || (c.public_listing && c.public_listing->offer_id==offer.terms))
                invalid("A funding conversation needs unique equity or gift terms and bounded evidence requirements.");
        }
    }
    if (c.public_listing) {
        const auto& rules=*c.public_listing;
        const auto& offer=find(c.financing,rules.offer_id);
        if (offer.donation || offer.term.value()!=0 || !offer.board_seat || rules.performance_days.value()<28 || rules.performance_days.value()>366 ||
            rules.minimum_age<rules.performance_days || rules.minimum_age.value()>36524 ||
            rules.minimum_revenue.value()<=0 || rules.minimum_revenue.value()>100000000000LL ||
            rules.minimum_profit.value()<=0 || rules.minimum_profit>rules.minimum_revenue ||
            rules.director_cash_floor.value()<0 || rules.director_cash_floor.value()>1000000000 ||
            rules.director_maximum_dilution.value()>3000 ||
            std::ranges::count(c.financing,offer.negotiation,&CorporateFinanceTerms::negotiation)!=1)
            invalid("A public listing needs one finite equity offer, settled performance thresholds and disclosed shareholder rights");
    }
    for (const auto& e:c.executives) {
        label(e.id,64);label(e.name);label(e.role,64);label(e.priorities,512);label(e.publication_research,64,true);
        if (!ids.insert(e.id).second || e.signing_cost.value()<0 || e.signing_cost.value()>100000000 || e.cash_salary.value()<=0 ||
            e.cash_salary.value()>10000000 || e.hybrid_salary.value()<=0 || e.hybrid_salary>e.cash_salary || e.hybrid_grant.value()>1000 ||
            e.vesting.value()<30 || e.vesting.value()>1461 || e.cliff.value()<0 || e.cliff>e.vesting ||
            e.technical>100 || e.commercial>100 || e.risk_tolerance>100 ||
            (!e.publication_research.empty() && (e.promise_term.value()<30 || e.promise_term.value()>730)))
            invalid("Executive terms need bounded cash, existing-pool grants, vesting and concrete promises");
    }
    for(const auto& r:c.charters) {
        label(r.id,64);label(r.name);label(r.priorities,512);label(r.employee_expectations,512);
        if(!ids.insert(r.id).second || r.cash_floor.value()<0 || r.cash_floor.value()>1000000000 ||
            r.maximum_dilution.value()>3000 || r.monthly_employee_budget.value()<=0 || r.monthly_employee_budget.value()>10000000)
            invalid("Charter needs bounded board priorities and a funded monthly employee program");
    }
    std::uint32_t seats=0,player_seats=0;
    for (const auto& b:c.board) {
        label(b.id,64);label(b.name);
        if (!ids.insert(b.id).second || b.seats==0 || b.seats>3 || b.cash_floor.value()<0 || b.maximum_dilution.value()>3000) invalid("Board rights require bounded seats and disclosed positions");
        seats+=b.seats;if (b.player) player_seats+=b.seats;
    }
    if (seats>12 || player_seats==0) invalid("The founding board needs player representation within twelve seats");
}
void validate_governance(const SimulationState& s,bool deep) {
    if (!s.management) return;
    const auto& management=*s.management;
    if (management.governance.has_value()!=management.catalog.governance.has_value()) invalid("Corporate rules and saved continuation must be present together");
    if (!management.governance) return;
    const auto& g=*management.governance;const auto& c=catalog(s);
    if (g.charters.size()>129 || g.appointments.size()>8 || g.grants.size()>8 || g.capital.size()>8192 || g.motions.size()>128 ||
        g.disputes.size()>32 || g.opening_holdings.size()>accounts_terms_limit || g.public_filings.size()>public_filing_limit)
        invalid("Corporate history exceeds its bounded records");
    if (!deep) return;
    validate_governance_catalog(c);
    auto opening=*s.accounts;opening.holdings=g.opening_holdings;validate_accounts(opening);
    std::set<std::string> appointed;
    for (const auto& a:g.appointments) {
        const auto& e=find(c.executives,a.candidate);
        if (!appointed.insert(a.candidate).second || a.hired_on<s.accounts->starts_on || a.hired_on>s.date ||
            (a.departed_on && (*a.departed_on<a.hired_on || *a.departed_on>s.date))) invalid("Executive engagement repeats or has invalid dates");
        const auto& addition=find(s.capacity->staff_additions,"executive/"+a.candidate);
        const auto& payroll=find(s.accounts->recurring,"executive/"+a.candidate);
        if (addition.day!=a.hired_on || addition.people!=1 || payroll.starts_on!=a.hired_on || payroll.ends_before!=a.departed_on ||
            payroll.monthly_amount!=(a.hybrid?e.hybrid_salary:e.cash_salary)) invalid("Executive staffing and agreed cash payroll do not reconcile");
        if (e.signing_cost.value()>0) (void)posting(s,"executive/"+e.id,a.hired_on,e.signing_cost,Account::expenses,Account::cash);
        if (a.departed_on) {
            const auto& departure=find(s.capacity->staff_departures,"executive.departure/"+a.candidate);
            if (departure.day!=*a.departed_on || departure.people!=1 || std::none_of(g.disputes.begin(),g.disputes.end(),[&](const auto& d){return d.kind=="publication" && d.subject==a.candidate && d.status==CorporateDisputeStatus::departed && d.resolved_on==a.departed_on;}))
                invalid("Executive departure lacks a concrete warned dispute and dated staffing change");
        }
    }
    auto holdings=g.opening_holdings;std::vector<OptionGrant> grants;std::optional<OrdinalDay> prior;
    std::set<std::string> corporate_posts;
    const auto listing_key=c.public_listing?"corporate/"+c.public_listing->offer_id:std::string{};
    const auto listing_holder=c.public_listing?"investor/"+c.public_listing->offer_id:std::string{};
    for (const auto& e:g.capital) {
        label(e.subject,128);label(e.key,256);
        if (e.day<s.accounts->starts_on || e.day>s.date || (prior && e.day<*prior)) invalid("Capital events are not ordered actual dates");
        prior=e.day;
        if (e.kind=="investment" || e.kind=="asset_equity") {
            if (c.public_listing && e.subject==listing_holder && e.key!=listing_key)
                invalid("Public float must come from the approved listing issue");
            if (e.consideration.value()<=0 || !corporate_posts.insert(e.key).second) invalid("An ownership issue needs distinct real consideration");
            (void)posting(s,e.key,e.day,e.consideration,e.kind=="investment"?Account::cash:Account::assets,Account::equity);
            if (e.key.starts_with("corporate/")) {
                const auto& terms=find(c.financing,e.key.substr(10));
                if (terms.term.value()!=0 || e.shares!=terms.acquired || e.subject!="investor/"+terms.id || e.consideration!=terms.proceeds) invalid("Corporate equity journal changed the accepted counteroffer");
            } else if (e.key.starts_with("financing/")) {
                const auto& terms=find(management.catalog.financing,e.key.substr(10));
                if (terms.term.value()!=0 || e.shares!=terms.acquired || e.subject!="investor/"+terms.id || e.consideration!=terms.proceeds) invalid("Fixed equity journal changed the accepted offer");
            }
        } else if(e.kind=="donation") {
            if(!e.key.starts_with("corporate/") || e.shares.value()!=0 || e.consideration.value()<=0 || !corporate_posts.insert(e.key).second)
                invalid("A nonprofit gift needs distinct non-diluting contribution evidence.");
            const auto& offer=find(c.financing,e.key.substr(10));
            if(!offer.donation || e.subject!="donor/"+offer.id || e.consideration!=offer.proceeds ||
                std::ranges::none_of(g.motions,[&](const auto& motion){return motion.kind==BoardMotionKind::financing && motion.executed && motion.subject==offer.id && motion.resolved_on==e.day;}))
                invalid("A nonprofit gift differs from its approved donor terms.");
            (void)posting(s,e.key,e.day,e.consideration,Account::cash,Account::equity);
        } else if (e.consideration.value()!=0) invalid("Vesting, grants and pool changes cannot create cash or assets");
        if ((e.kind=="grant" || e.kind=="vest" || e.kind=="departure") && e.key!="executive/"+e.subject) invalid("Grant movement lost its actual executive owner");
        if (e.kind=="pool") {
            const auto& m=find(g.motions,e.subject);
            if (m.kind!=BoardMotionKind::pool_expansion || !m.executed || m.resolved_on!=e.day || m.shares!=e.shares) invalid("Pool expansion lacks its approved recorded dilution");
        }
        apply_capital(holdings,grants,g.appointments,c,e);
    }
    if (holdings!=s.accounts->holdings || grants!=g.grants) invalid("Fully diluted ownership differs from conserving capital history");
    for (const auto& p:s.accounts->postings) if (p.credit==Account::equity && p.key!="opening" && !p.key.starts_with("flow.transfer:") &&
        (p.debit==Account::cash || p.debit==Account::assets) && !corporate_posts.contains(p.key)) invalid("An ownership issue bypassed corporate capital history");
    for (const auto& a:g.appointments) if (a.hybrid) {
        const auto& grant=find(g.grants,a.candidate);
        if (grant.departed!=a.departed_on.has_value() || (!grant.departed && vesting_due(holdings,grant,a,find(c.executives,a.candidate),s.date).value()>0)) invalid("Grant departure or cumulative vesting is incomplete");
    }
    std::set<std::string> charter_motions;
    std::set<std::string> expected_charter_bills;
    std::map<std::string_view,const RecurringObligation*> charter_bills;
    for(const auto& bill:s.accounts->recurring) if(bill.id.starts_with("charter/employees/")) charter_bills.emplace(bill.id,&bill);
    std::map<std::string_view,const BoardMotion*> charter_votes;
    for(const auto& m:g.motions) if(m.kind==BoardMotionKind::charter) charter_votes.emplace(m.id,&m);

    const auto formation_day=(s.infrastructure && s.infrastructure->workplace)
        ? s.infrastructure->workplace->starts_on
        : s.accounts->starts_on;
    for(std::size_t i=0;i<g.charters.size();++i) {
        const auto& change=g.charters[i];const auto& charter=find(c.charters,change.charter);
        if(change.day<s.accounts->starts_on || change.day>s.date || (i && (change.day<g.charters[i-1].day || change.charter==g.charters[i-1].charter)))
            invalid("Charter history is unordered or repeats an unchanged charter");
        if(change.motion.empty()) {
            if(i!=0 || change.day!=formation_day) invalid("Only formation can select a charter without a board vote");
        } else {
            const auto it=charter_votes.find(change.motion);
            if(it==charter_votes.end()) invalid("Charter amendment has no board motion");
            const auto& m=*it->second;
            if(!charter_motions.insert(m.id).second || m.kind!=BoardMotionKind::charter || !m.executed || m.status!=BoardMotionStatus::approved ||
                m.subject!=change.charter || m.resolved_on!=change.day || m.charter_cursor!=i) invalid("Charter amendment lacks its actual approved board vote");
        }
        const auto key="charter/employees/"+std::to_string(i+1);
        expected_charter_bills.insert(key);
        const auto bill=charter_bills.find(key);
        if(bill==charter_bills.end()) invalid("Charter has no funded employee program");
        const auto& obligation=*bill->second;
        const auto end=i+1<g.charters.size()?std::optional{g.charters[i+1].day}:std::nullopt;
        if(obligation.monthly_amount!=charter.monthly_employee_budget || obligation.starts_on!=change.day || obligation.ends_before!=end ||
            obligation.cause!="Employee program under "+charter.name) invalid("Charter employee program differs from its funded monthly commitment");
    }
    if(charter_bills.size()!=expected_charter_bills.size()) invalid("Employee program has no approved charter");
    validate_public_filings(s);
    std::set<std::string> finance;
    for (std::size_t index=0;index<g.motions.size();++index) {
        const auto& m=g.motions[index];
        if (m.executed && (!m.resolved_on || m.status!=BoardMotionStatus::approved)) invalid("An executed motion lacks an approved dated vote");
        if (m.id!="board."+std::to_string(index+1) || m.opened_on<s.accounts->starts_on || m.opened_on>s.date ||
            m.deadline!=advance_days(m.opened_on,c.vote_window) || m.deadline>=s.end_date || m.ballots.size()>17 || m.cash.value()<0 || m.asset_value.value()<0 ||
            (m.resolved_on && (*m.resolved_on<m.opened_on || *m.resolved_on>s.date))) invalid("Board motion has invalid identity, response window or terms");
        if (m.kind==BoardMotionKind::financing) {
            const auto& f=find(c.financing,m.subject);
            if (m.cash!=f.proceeds || m.shares!=f.acquired || m.asset_value.value()!=0 || ((m.executed || m.status==BoardMotionStatus::pending) && !finance.insert(f.negotiation).second)) invalid("Negotiated financing terms changed or alternative counteroffers were used twice");
            if (c.public_listing && c.public_listing->offer_id==f.id &&
                (!listing_qualification(s,m.opened_on).qualified ||
                 (m.player_vote && m.resolved_on && !listing_qualification(s,*m.resolved_on).qualified)))
                invalid("The listing motion lacks its dated settled qualification");
            if (m.executed) {
                const auto key="corporate/"+f.id;
                if (f.term.value()==0) (void)posting(s,key,*m.resolved_on,f.proceeds,Account::cash,Account::equity);
                else {
                    (void)posting(s,"debt/"+key,*m.resolved_on,f.proceeds,Account::cash,Account::debt);
                    const auto& debt=find(s.accounts->debts,key);auto due=advance_days(*m.resolved_on,f.term);
                    for (const auto& d:g.disputes) if (d.subject==f.id && d.status==CorporateDisputeStatus::accelerated) due=std::min(due,advance_days(*d.resolved_on,Days{30}));
                    if (debt.drawn_on!=*m.resolved_on || debt.monthly_interest!=f.monthly_interest || debt.due!=due) invalid("Negotiated debt differs from its covenant and actual maturity");
                }
            }
        } else if (m.kind==BoardMotionKind::pool_expansion) {
            if (m.subject!="option_pool" || m.cash.value()!=0 || m.asset_value.value()!=0 || m.shares.value()==0 || m.shares>c.maximum_pool_expansion) invalid("Pool motion has invalid explicit dilution");
        } else if (m.kind==BoardMotionKind::acquisition) {
            if (m.subject.empty() || m.cash.value()<=0 || m.asset_value<m.cash || m.shares>c.acquisition_equity) invalid("Acquisition board terms are malformed");
        } else if(m.kind==BoardMotionKind::charter) {
            (void)find(c.charters,m.subject);
            if(m.cash.value()!=0 || m.asset_value.value()!=0 || m.shares.value()!=0 || (m.executed && !charter_motions.contains(m.id))) invalid("Charter motion contains fabricated consideration or an unrecorded amendment");
        } else invalid("Unsupported board motion kind");
        if (m.status==BoardMotionStatus::pending || m.status==BoardMotionStatus::expired) {
            if (m.player_vote || !m.ballots.empty() || m.posting_cursor!=0 || m.leadership_cursor!=0 || m.charter_cursor!=0 || m.reviewed_cash.value()!=0 || m.executed ||
                (m.status==BoardMotionStatus::pending ? m.resolved_on.has_value() || s.date>=m.deadline:m.resolved_on!=m.deadline || s.date<m.deadline)) invalid("Unvoted board motion contains a fabricated result");
        } else {
            if (!m.player_vote || !m.resolved_on || *m.resolved_on>=m.deadline || m.posting_cursor>s.accounts->postings.size()) invalid("Board decision lacks its actual vote boundary");
            if(s.succession && (m.leadership_cursor==0 || m.leadership_cursor>s.succession->controls.size() ||
                s.succession->controls[m.leadership_cursor-1].day>*m.resolved_on ||
                (m.leadership_cursor<s.succession->controls.size() && s.succession->controls[m.leadership_cursor].day<*m.resolved_on)))
                invalid("Board votes used leadership rights from a different date.");
            if(m.charter_cursor>g.charters.size() ||
                (m.charter_cursor && g.charters[m.charter_cursor-1].day>*m.resolved_on) ||
                (m.charter_cursor<g.charters.size() && g.charters[m.charter_cursor].day<*m.resolved_on) ||
                (m.charter_cursor==0 && !g.charters.empty() && g.charters.front().motion.empty())) invalid("Board decision used charter terms from a different date");
            std::int64_t cash=0;for (std::size_t i=0;i<m.posting_cursor;++i) {const auto& p=s.accounts->postings[i];if (p.day>*m.resolved_on) invalid("Board review includes future postings");if (p.debit==Account::cash) cash=checked_add(cash,p.amount.value());if (p.credit==Account::cash) cash=checked_subtract(cash,p.amount.value());}
            if (m.reviewed_cash!=Cents{cash} || m.ballots!=ballots_for(s,m,*m.player_vote,m.reviewed_cash)) invalid("Board votes differ from actual seats, rights or disclosed positions");
            std::uint32_t yes=0,total=0;for (const auto& b:m.ballots) {total+=b.seats;if (b.support) yes+=b.seats;}
            if ((m.status==BoardMotionStatus::approved)!=(yes>total/2) || (m.status==BoardMotionStatus::rejected && m.executed) ||
                (m.status==BoardMotionStatus::approved && !m.executed && m.kind!=BoardMotionKind::acquisition)) invalid("Board outcome differs from the majority and executed commitment");
        }
    }
    if (c.public_listing && corporate_posts.contains(listing_key)!=public_listing_date(s).has_value())
        invalid("Public equity issue and approved listing vote do not reconcile");
    std::set<std::string> disputes;std::map<std::pair<std::string,std::string>,std::size_t> episodes;
    for (const auto& d:g.disputes) {
        const bool open=d.status==CorporateDisputeStatus::concern || d.status==CorporateDisputeStatus::formal;
        if (!open && !d.resolved_on) invalid("A resolved dispute lacks its actual date");
        const auto episode=++episodes[{d.kind,d.subject}];
        if (d.id!=d.kind+"/"+d.subject+"/"+std::to_string(episode) || !disputes.insert(d.id).second || d.opened_on<s.accounts->starts_on || d.opened_on>s.date ||
            d.deadline!=advance_days(d.opened_on,Days{c.response_window.value()*(d.negotiated_on?2:1)}) ||
            (d.resolved_on && (*d.resolved_on<d.opened_on || *d.resolved_on>s.date))) invalid("Dispute has invalid cause, response window or result");
        if(d.negotiated_on) {
            const auto& review=find(s.capacity->projects,d.review_project);
            const bool interrupted=review.stopped_on && std::ranges::any_of(g.appointments,
                [&](const auto& appointment){return appointment.departed_on==review.stopped_on;});
            if(d.kind!="publication" || *d.negotiated_on<advance_days(d.opened_on,c.concern_period) ||
                *d.negotiated_on>=advance_days(d.opened_on,c.response_window) || *d.negotiated_on>s.date ||
                review.kind!=ProjectKind::research || review.starts_on!=*d.negotiated_on || review.staff==0 ||
                review.total_work!=integrate(review.requested,Days{d.deadline.value()-d.negotiated_on->value()}) ||
                (review.status==ProjectStatus::cancelled && !interrupted &&
                    (!d.resolved_on || !review.stopped_on || *review.stopped_on<*d.resolved_on)))
                invalid("Executive negotiation lost its formal window or actual review work.");
        } else if(!d.review_project.empty()) invalid("An executive review project has no negotiated agreement.");
        if (d.opening_cash!=cash_at(*s.accounts,d.posting_cursor,d.opened_on) || d.publication_cursor>s.lab->publications.size()) invalid("Dispute evidence differs from the actual opening boundary");
        if (d.kind=="publication") {
            const auto& e=find(c.executives,d.subject);const auto& a=find(g.appointments,d.subject);
            if (e.publication_research.empty() || d.opened_on!=advance_days(a.hired_on,e.promise_term) || d.status==CorporateDisputeStatus::waived || d.status==CorporateDisputeStatus::accelerated) invalid("Publication dispute lacks an actual missed commitment");
            for (std::size_t i=0;i<d.publication_cursor;++i) if (s.lab->publications[i].research_id==e.publication_research) invalid("Dispute invents an unmet publication promise");
            if (d.status==CorporateDisputeStatus::resolved && std::none_of(s.lab->publications.begin(),s.lab->publications.end(),[&](const auto& p){return p.research_id==e.publication_research && p.day<=*d.resolved_on;})) invalid("Promise resolution lacks the actual publication");
            if((d.status==CorporateDisputeStatus::departed || open) && std::any_of(s.lab->publications.begin(),s.lab->publications.end(),[&](const auto& p){
                return p.research_id==e.publication_research && (open?p.day<=s.date:p.day<*d.resolved_on);
            })) invalid("The executive dispute invents a breach after the actual publication was already delivered.");
        } else if (d.kind=="covenant") {
            const auto& terms=find(c.financing,d.subject);
            const auto& debt=find(s.accounts->debts,"corporate/"+d.subject);
            std::int64_t principal=0;
            for (std::size_t i=0;i<d.posting_cursor;++i) {const auto& p=s.accounts->postings[i];if (p.document_id==debt.id) {
                if (p.credit==Account::debt) principal=checked_add(principal,p.amount.value());
                if (p.debit==Account::debt) principal=checked_subtract(principal,p.amount.value());
            }}
            if (terms.term.value()==0 || terms.covenant_cash_floor.value()==0 || d.status==CorporateDisputeStatus::departed ||
                d.opening_cash>=terms.covenant_cash_floor || principal<=0) invalid("Cash covenant lacks an actual cash/principal breach");
            if (d.status==CorporateDisputeStatus::waived) (void)posting(s,"covenant.waiver/"+d.id,*d.resolved_on,terms.waiver_fee,Account::expenses,Account::cash);
        } else invalid("Unsupported corporate dispute cause");
        if (open ? d.resolved_on.has_value() || s.date>=d.deadline || (d.status==CorporateDisputeStatus::formal)!=(s.date>=advance_days(d.opened_on,c.concern_period)):
            !d.resolved_on || ((d.status==CorporateDisputeStatus::departed || d.status==CorporateDisputeStatus::accelerated) && *d.resolved_on!=d.deadline)) invalid("Dispute phase skips its warned response dates");
    }
}
}
