#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <iostream>
#include <stdexcept>
#include <algorithm>

namespace {
using namespace pgp;
void require(bool value,const char* why) {if(!value) throw std::runtime_error(why);}
template<class F> void refuses(F f) {try{f();}catch(const std::exception&){return;}throw std::runtime_error("Invalid founder terms were accepted.");}
}
int main() {
    try {
        const auto registry=content::foundation_registry();
        const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_FOUNDING_SOURCE,content::maximum_document_bytes),registry),registry);
        const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
        const auto& catalog=*scenario.founding;
        content::FoundingSelection selected;
        for(const auto& scene:catalog.scenes) selected.choices.emplace(scene.id,scene.choices.front().id);
        const auto make=[&](content::FoundingSelection s,std::optional<core::ObjectiveSelection> mission=core::ObjectiveSelection{core::ObjectiveKind::survival,{}}) {
            runtime::ScenarioOptions options;
            options.identity=runtime::CampaignIdentity{"Test research",s.first_name+" "+s.last_name,"she/her"};
            options.founding=std::move(s);
            return runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",mission,options);
        };
        content::FoundingSelection revised;
        revised.choices=selected.choices;revised.appearance=core::make_portrait(9,"founder");
        revised.family="rich";revised.route="bootstrap";revised.office="home";
        revised.personal_investment_cents=1'000'000;
        auto solo=make(revised);
        require(solo.world.simulation.succession->personal_cash==core::Cents{699'000'000} &&
            content::founding_terms(catalog,revised).personal_contribution_cents==1'000'000,
            "A wealthy founder automatically receives $7 million before the chosen personal investment.");
        auto political=revised;
        political.advantages={"genius","charisma","political"};
        political.choices["elem_campaign_trail"]="briefing";
        const auto combined=content::founding_terms(catalog,political);
        const auto ordinary_wealth=content::founding_terms(catalog,revised);
        require(combined.personal_remaining_cents==709'000'000 &&
            combined.buzz==ordinary_wealth.buzz+35 && combined.founder_researcher,
            "Wealth, Genius, Charisma and political effects must combine.");
        const auto combined_opening=make(political);
        require(combined_opening.world.simulation.management->people->talent->founder_genius &&
            persistence::decode_save(program,persistence::encode_save(program,combined_opening)).checkpoint==combined_opening,
            "All chosen advantages and the derived wealthy background must survive a fresh save.");
        political.family="poor";
        refuses([&]{(void)make(political);});
        political.family="rich";political.advantages.push_back("wealthy");
        refuses([&]{(void)make(political);});
        revised.personal_investment_cents=-1;
        require(solo.world.difficulty.empty() && solo.world.simulation.capacity->sources.empty(),"New openings must not inherit difficulty or a compute lease.");
        require(solo.world.simulation.management->people->employees.size()==1,"A genius starts alone, without a forced first hire.");
        require(std::ranges::none_of(solo.world.simulation.accounts->recurring,[](const auto& r){return r.id=="office_and_admin" || r.id=="compute_lease" || r.id=="payroll_with_benefits";}),"The parents' house has no rent, employee payroll or compute subscription.");
        require(solo.world.simulation.management->people->employees.front().terms.profile.portrait==revised.appearance,"The research-capable founder must keep the customized portrait.");
        require(persistence::decode_save(program,persistence::encode_save(program,solo)).checkpoint==solo,"Revised biography and appearance must reload exactly.");
        revised.advantages={"charisma"};revised.researcher="none";
        const auto nontechnical=make(revised);
        require(nontechnical.world.simulation.capacity->available_staff==0 && nontechnical.world.simulation.management->people->employees.empty(),"A nontechnical sole founder must not invent a researcher.");
        auto no_optional_advantages=revised;
        no_optional_advantages.family="poor";
        no_optional_advantages.advantages.clear();
        const auto ordinary_opening=make(no_optional_advantages);
        require(ordinary_opening.world.founding->advantages.empty() && ordinary_opening.world.simulation.cash>core::Cents{0},
            "A founder with a modest upbringing can launch without selecting any advantage.");
        runtime::Runtime quiet{program,nontechnical};
        require(quiet.prepare_opening(quiet.current_state().world.simulation.revision).accepted(),"A company without research staff must open normally.");
        const auto quiet_day=quiet.submit({quiet.current_state().world.simulation.revision,core::AdvanceDay{}});
        require(quiet_day.accepted(),quiet_day.explanation.c_str());
        revised.family="poor";revised.personal_investment_cents=0;
        refuses([&]{(void)make(revised);});
        revised.personal_investment_cents=catalog.formation_cents+500000;
        const auto small_bootstrap=make(revised);
        require(small_bootstrap.world.simulation.cash==core::Cents{500000},"A founder with a modest background can self-fund with their chosen contribution.");
        {
            require(catalog.startup_loan.has_value(),"The founder campaign must offer startup credit.");
            const auto& loan=*catalog.startup_loan;
            auto application=revised;
            application.route="bank_loan";
            application.personal_investment_cents=loan.minimum_investment_cents-1;
            refuses([&]{(void)make(application);});
            application.personal_investment_cents=loan.minimum_investment_cents;
            auto funded=make(application);
            const auto& opening=funded.world.simulation;
            require(opening.cash==core::Cents{loan.principal_cents+loan.minimum_investment_cents-catalog.formation_cents},"The loan and personal contribution must both fund the new company.");
            require(opening.accounts->postings.front().amount==core::Cents{loan.minimum_investment_cents},"Borrowed cash must enter through the debt ledger rather than an equity posting.");
            require(opening.accounts->debts.size()==1 && opening.accounts->debts.front().principal==core::Cents{loan.principal_cents},"Signing must create one real principal balance.");
            require(opening.succession->personal_cash==core::Cents{content::founding_terms(catalog,application).personal_remaining_cents},"The loan must never enter the founder's personal account.");
            funded.world.enabled_flows=std::vector<std::string>{};
            runtime::Runtime borrower{program,funded};
            require(borrower.prepare_opening(opening.revision).accepted(),"The funded opening must prepare.");
            const auto command=[&](core::CommandAction action) {
                const auto result=borrower.submit({borrower.current_state().world.simulation.revision,std::move(action)});
                require(result.accepted(),result.explanation.c_str());
            };
            for(int day=0;day<31;++day) command(core::AdvanceDay{});
            const auto& accounts=*borrower.current_state().world.simulation.accounts;
            const auto interest=accounts.invoices.find("interest/founding/startup_loan/2015-01-01");
            require(interest.has_value() && accounts.invoices[*interest].amount==core::Cents{loan.principal_cents*loan.monthly_interest_bp/10000},"A full month must accrue the disclosed interest on actual principal.");
            command(core::RepayDebt{std::string{content::startup_debt_id},core::Cents{50000},"startup-test-repayment"});
            const auto& repaid=borrower.current_state();
            require(repaid.world.simulation.accounts->debts.front().principal==core::Cents{loan.principal_cents-50000},"Repayment must reduce the outstanding principal.");
            require(persistence::decode_save(program,persistence::encode_save(program,repaid)).checkpoint==repaid,"The loan, accrued interest and partial repayment must reload exactly.");
        }
        revised.route="investment";
        refuses([&]{(void)make(revised);});
        revised.backers={"commercial"};
        const auto financed=make(revised);
        require(financed.world.simulation.management->people->employees.empty(),"Investment does not hire staff.");
        auto baseline=revised;
        baseline.gender="woman";baseline.birth_sex="female";baseline.orientation="straight";
        const auto ordinary=content::founding_terms(catalog,baseline);
        for(const auto& identity:std::vector<std::pair<std::string,std::string>>{{"woman","bisexual"},{"woman","queer"},{"nonbinary","straight"},{"man","gay_or_lesbian"}}) {
            auto community=baseline;community.gender=identity.first;community.orientation=identity.second;
            const auto supported=make(community);
            const auto support=content::founding_terms(catalog,community);
            require(content::queer_founder(community) && support.buzz==ordinary.buzz+catalog.queer_community_buzz &&
                support.prestige==ordinary.prestige+catalog.queer_community_prestige,
                "Queer gender or orientation must receive the single disclosed company and public reaction.");
            require(supported.world.simulation.management->people->community_prestige==catalog.queer_community_prestige && supported.world.simulation.management->people->community_buzz==catalog.queer_community_buzz,"Company and public reactions must reach persistent gameplay state.");
            require(persistence::decode_save(program,persistence::encode_save(program,supported)).checkpoint==supported,
                "Birth record, adult identity, orientation and every portrait color must reload exactly.");
        }
        auto community=baseline;community.birth_sex="male";
        require(content::transgender_founder(community) && content::queer_founder(community),"Transgender recognition must include straight founders.");
        community.orientation="bisexual";
        require(content::founding_terms(catalog,community).buzz==ordinary.buzz+catalog.queer_community_buzz,"Gender and orientation must not stack the same audience reaction.");
        baseline.birth_sex="indeterminate";
        require(!content::transgender_founder(baseline),"An indeterminate birth record cannot establish a mismatch.");
        community.orientation="unspecified";refuses([&]{(void)make(community);});
        community.orientation="straight";community.gender="unspecified";refuses([&]{(void)make(community);});
        require(scenario.management->people->founder_audiences.empty(),"The campaign must not assign employee pay or mood by founder gender.");
        require(std::ranges::none_of(catalog.scenes,[](const auto& scene){return scene.id=="arrival" || scene.id=="threshold";}),
            "The intro must not ask an infant to move or choose a company mission.");
        revised.route="hired_ceo";revised.office="shared";revised.backers.clear();
        refuses([&]{(void)make(revised);});
        revised.advantages={"political"};
        revised.family="rich";
        const auto employed=make(revised);
        const auto employment=content::founding_terms(catalog,revised);
        require(employed.world.simulation.management->people->employees.size()==catalog.opening->inherited_staff && employment.founder_bp==catalog.opening->ceo_equity_bp,"A hired CEO inherits a small roster and the disclosed minority grant.");
        require(employment.contributed_cents==content::founding_option(catalog.routes,"hired_ceo").capital_cents,"CEO opening cash is the existing company's cash, not family money.");
        require(employed.world.simulation.lab->research.empty() && employed.world.simulation.lab->models.empty(),"An existing office does not force a research or product choice.");
        require(persistence::decode_save(program,persistence::encode_save(program,employed)).checkpoint==employed,"Inherited company terms must reload exactly.");
        revised.backers={"commercial"};refuses([&]{(void)make(revised);});revised.backers.clear();
        runtime::ScenarioOptions with_difficulty;with_difficulty.founding=revised;with_difficulty.difficulty="difficulty.forgiving";
        refuses([&]{(void)runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",{},with_difficulty);});
        revised.route="partnership";
        refuses([&]{(void)make(revised,std::nullopt);});
        require(catalog.partners.size()==3,"The founder campaign must offer three distinct working cofounders.");
        for(const auto& partner:catalog.partners) {
            auto agreement=revised;
            agreement.partner=partner.id;
            agreement.family="poor";agreement.advantages={"charisma"};
            refuses([&]{(void)make(agreement,std::nullopt);});
            agreement.advantages={partner.access=="technical"?"genius":"political"};
            if(partner.access=="connected") agreement.family="rich";
            agreement.researcher=partner.access=="technical"?"founder":"none";
            agreement.personal_investment_cents=0;
            refuses([&]{(void)make(agreement);});
            auto signed_opening=make(agreement,std::nullopt);
            const auto& world=signed_opening.world;
            const auto& state=world.simulation;
            const auto terms=content::founding_terms(catalog,agreement);
            require(state.cash==core::Cents{partner.capital_cents-terms.paid_setup_cents} &&
                state.succession->personal_cash==core::Cents{terms.personal_remaining_cents},"Cofounder capital must fund the company and leave personal savings intact.");
            const auto holding=std::ranges::find(state.accounts->holdings,"cofounder",&core::EquityHolding::holder);
            require(holding!=state.accounts->holdings.end() && holding->shares==core::BasisPoints{partner.shares_bp},"The actual equity ledger must use the agreement.");
            require(core::objective_key(world.objectives->selections.front().selection.primary)==partner.objective,"Signing must set the cofounder's opening mission.");
            const auto& people=*state.management->people;
            const auto employee=std::ranges::find_if(people.employees,[&](const auto& row){return row.terms.profile.name==partner.name;});
            require(people.employees.size()==terms.opening_staff && employee!=people.employees.end() &&
                core::employee_monthly_salary(state,*employee)==core::Cents{partner.monthly_salary_cents},"The signed cofounder must join the roster at the quoted salary.");
            require(people.contacts.size()==1 && people.contacts.front().person=="opening/"+partner.id &&
                people.contacts.front().opening->outcome=="signed","The conversation must carry into the company inbox as signed.");
            const auto board=core::current_board(state);
            const auto seat=std::ranges::find(board,"board."+partner.id,&core::BoardMember::id);
            require(seat!=board.end() && seat->name==partner.name && seat->seats==partner.seats &&
                seat->cash_floor==core::Cents{partner.cash_floor_cents} && seat->maximum_dilution==core::BasisPoints{partner.maximum_dilution_bp},"The actual board must use the disclosed cofounder terms.");
            auto forged=signed_opening;
            forged.world.simulation.management->people->contacts.front().opening->outcome="unsigned";
            refuses([&]{runtime::validate_checkpoint(program,forged);});
            signed_opening.world.enabled_flows=std::vector<std::string>{};
            runtime::Runtime partnered{program,signed_opening};
            require(partnered.prepare_opening(state.revision).accepted(),"The cofounder opening must prepare.");
            const auto command=[&](core::CommandAction action) {
                const auto result=partnered.submit({partnered.current_state().world.simulation.revision,std::move(action)});
                require(result.accepted(),result.explanation.c_str());
            };
            command(core::ProposePoolExpansion{core::BasisPoints{800}});
            const auto& governance=*partnered.current_state().world.simulation.management->governance;
            command(core::VoteBoardMotion{governance.motions.back().id,true});
            const auto& saved=partnered.current_state();
            const auto& ballots=saved.world.simulation.management->governance->motions.back().ballots;
            const auto ballot=std::ranges::find(ballots,"board."+partner.id,&core::BoardBallot::member);
            require(ballot!=ballots.end() && ballot->seats==partner.seats && ballot->support==(partner.maximum_dilution_bp>=800),"Cofounders must actually vote according to their own dilution limits.");
            require(persistence::decode_save(program,persistence::encode_save(program,saved)).checkpoint==saved,"Signed terms and the actual cofounder ballot must reload exactly.");
        }
        revised.route="mission_director";revised.legal_form="nonprofit";revised.office="home";
        refuses([&]{(void)make(revised);});
        revised.backers={"safety"};
        const auto donated=make(revised);
        require(donated.world.simulation.capacity->available_staff==0,"A negotiated donor does not hire a researcher.");
        selected.mode="sandbox";selected.choices.clear();
        refuses([&]{ (void)make(selected); });
        {
            const auto has_scene=[&](const content::FoundingSelection& s,std::string_view id) {
                return std::ranges::any_of(content::formative_scenes(catalog,s),[&](const auto* sc){return sc->id==id;});
            };
            const auto school_defaults=[&](std::string leg,std::string family,std::uint64_t seed) {
                content::FoundingSelection s;
                s.advantages={std::move(leg)};s.family=std::move(family);s.route="bootstrap";s.office="home";s.researcher="founder";
                s.personal_investment_cents=catalog.formation_cents+500000;
                s.story_seed=seed;
                for(const auto& sc:catalog.scenes) {
                    if(sc.id=="ms_sport") s.choices[sc.id]="no_sport";
                    else if(sc.id=="ms_clubs") s.choices[sc.id]="robotics";
                    else if(sc.id=="ms_bully") s.choices[sc.id]="report";
                    else if(sc.id=="ms_bully_aftermath") s.choices[sc.id]="report_later";
                    else if(sc.id=="hs_bully_continuation") s.choices[sc.id]="report_hs";
                    else if(sc.id=="hs_drugs") s.choices[sc.id]="pass";
                    else if(sc.id=="hs_drugs_aftermath") s.choices[sc.id]="rehab";
                    else if(sc.id=="grad_choice") s.choices[sc.id]="workforce";
                    else if(sc.id=="ug_funding") s.choices[sc.id]=(s.family=="rich"?"family_paid":"loans");
                    else if(sc.id=="grad_school") s.choices[sc.id]="enter_market";
                    else if(sc.id=="genius_second_phd") s.choices[sc.id]="decline_second";
                    else s.choices[sc.id]=sc.choices.front().id;
                }
                return s;
            };
            std::uint64_t seed_elem=0;
            for(std::uint64_t seed=1;seed<4096;++seed) {
                auto probe=school_defaults("genius","rich",seed);
                if(has_scene(probe,"elem_best_friend") && has_scene(probe,"elem_sharing") && has_scene(probe,"elem_rule_blame")) {
                    seed_elem=seed;break;
                }
            }
            require(seed_elem!=0,"Must find a story_seed exercising all 3 friend-stacking elementary events.");
            auto elem=school_defaults("genius","rich",seed_elem);
            elem.choices["elem_best_friend"]="accept";
            elem.choices["elem_sharing"]="share";
            elem.choices["elem_rule_blame"]="truth";
            const auto elem_terms=content::founding_terms(catalog,elem);
            require(elem_terms.best_friends>=3,"Friend sources in elementary school must stack.");
            const auto elem_cp=make(elem);
            const auto childhood_friend_count=[&](const runtime::Checkpoint& cp) {
                return std::ranges::count_if(cp.world.simulation.management->people->social->links,
                    [](const auto& l){return l.memory=="Best friends since childhood";});
            };
            require(childhood_friend_count(elem_cp)>=3,"Stacked childhood best friends must become runtime SocialLinks.");
            auto gen_lie=elem;gen_lie.choices["elem_rule_blame"]="lie";
            auto gen_truth=elem;
            auto char_lie=gen_lie;char_lie.advantages={"charisma"};char_lie.researcher="none";
            auto char_truth=gen_truth;char_truth.advantages={"charisma"};char_truth.researcher="none";
            const int gen_diff=static_cast<int>(content::founding_terms(catalog,gen_lie).prestige)-static_cast<int>(content::founding_terms(catalog,gen_truth).prestige);
            const int char_diff=static_cast<int>(content::founding_terms(catalog,char_lie).prestige)-static_cast<int>(content::founding_terms(catalog,char_truth).prestige);
            require(char_diff>gen_diff,"Charismatic founders must earn the special prestige combo when bluffing blame.");

            auto clubs=school_defaults("genius","rich",seed_elem);
            clubs.choices["ms_sport"]="no_sport";clubs.choices["ms_clubs"]="robotics,coding,debate";
            (void)content::founding_terms(catalog,clubs);
            clubs.choices["ms_sport"]="basketball_thrive";
            refuses([&]{ (void)content::founding_terms(catalog,clubs); });
            clubs.choices["ms_clubs"]="robotics,coding";
            (void)content::founding_terms(catalog,clubs);

            std::uint64_t seed_ms=0;
            for(std::uint64_t seed=1;seed<4096;++seed) {
                auto probe=school_defaults("genius","rich",seed);
                if(has_scene(probe,"ms_bully")) {seed_ms=seed;break;}
            }
            require(seed_ms!=0,"Must find a story_seed activating ms_bully.");
            auto bully_gen=school_defaults("genius","rich",seed_ms);
            bully_gen.choices["ms_bully"]="befriend";
            refuses([&]{ (void)content::founding_terms(catalog,bully_gen); });
            auto bully_char=school_defaults("charisma","rich",seed_ms);
            bully_char.researcher="none";
            const auto base_friends=content::founding_terms(catalog,bully_char).best_friends;
            bully_char.choices["ms_bully"]="befriend";
            const auto befriend_terms=content::founding_terms(catalog,bully_char);
            require(befriend_terms.best_friends==base_friends+6,"Befriending middle school bullies must add 6 childhood best friends.");
            const auto befriend_cp=make(bully_char);
            require(childhood_friend_count(befriend_cp)>=6,"Befriended middle school bullies must persist into runtime best friends.");

            auto tol=school_defaults("genius","rich",seed_ms);
            tol.choices["ms_bully"]="ignore";
            tol.choices["ms_bully_aftermath"]="accept";
            tol.choices["hs_bully_continuation"]="report_hs";
            const auto tol_cp=make(tol);
            require(core::founder_has_trait(tol_cp.world.simulation,"Tolerant") &&
                    tol_cp.world.simulation.management->people->social->hostile_peers.empty(),
                    "Reporting bullying in high school must resolve hostile peers while retaining Tolerant.");

            auto endure=tol;
            endure.choices["hs_bully_continuation"]="endure_hs";
            const auto endure_cp=make(endure);
            const auto& hostile=endure_cp.world.simulation.management->people->social->hostile_peers;
            require(hostile.size()>=4 && hostile.size()<=8,"Unresolved bullying must create 4-8 hostile industry peers.");
            require(persistence::decode_save(program,persistence::encode_save(program,endure_cp)).checkpoint==endure_cp,
                    "Hostile peer assignments must survive save/reload.");

            std::uint64_t seed_both=0;
            for(std::uint64_t seed=1;seed<4096;++seed) {
                auto probe=school_defaults("genius","rich",seed);
                if(has_scene(probe,"elem_best_friend") && has_scene(probe,"ms_bully")) {seed_both=seed;break;}
            }
            require(seed_both!=0,"Must find a story_seed activating both elem_best_friend and ms_bully.");
            auto both=school_defaults("genius","rich",seed_both);
            both.choices["elem_best_friend"]="accept";
            both.choices["ms_bully"]="ignore";
            both.choices["ms_bully_aftermath"]="accept";
            both.choices["hs_bully_continuation"]="endure_hs";
            const auto both_cp=make(both);
            const auto& both_social=*both_cp.world.simulation.management->people->social;
            const auto& both_industry=both_cp.world.simulation.management->people->industry;
            bool found_ceo_friend=false;
            for(const auto& link:both_social.links) {
                if(link.memory!="Best friends since childhood") continue;
                const auto& peer_id=link.first=="founder"?link.second:link.first;
                require(std::ranges::find(both_social.hostile_peers,peer_id)==both_social.hostile_peers.end(),
                        "School bullies must never displace or overlap with childhood best friends.");
                if(const auto it=both_industry.find(peer_id);it!=both_industry.end()) {
                    if(peer_id.ends_with("/employee/1") || it->second.profile.role.find("CEO")!=std::string::npos ||
                       it->second.profile.role.find("Founder")!=std::string::npos) {
                        found_ceo_friend=true;
                    }
                }
            }
            require(found_ceo_friend && both_social.hostile_peers.size()>=4 && both_social.hostile_peers.size()<=8,
                    "Childhood CEO best friends and 4-8 same-age school bullies must coexist cleanly.");

            std::uint64_t seed_hs=0;
            for(std::uint64_t seed=1;seed<4096;++seed) {
                auto probe=school_defaults("genius","rich",seed);
                if(has_scene(probe,"hs_drugs")) {seed_hs=seed;break;}
            }
            require(seed_hs!=0,"Must find a story_seed activating hs_drugs.");
            auto calc=school_defaults("genius","rich",seed_hs);
            calc.choices["hs_drugs"]="calculated";
            const auto calc_cp=make(calc);
            require(core::founder_has_trait(calc_cp.world.simulation,"Calculated Risk Taker") &&
                    core::founder_risk_scale_bp(calc_cp.world.simulation)==core::BasisPoints{9000} &&
                    calc_cp.world.simulation.lab &&
                    calc_cp.world.simulation.lab->setback_risk_scale_bp==core::BasisPoints{9000} &&
                    core::scale_risk_bp(core::BasisPoints{2000},core::founder_risk_scale_bp(calc_cp.world.simulation))==core::BasisPoints{1800},
                    "Calculated Risk Taker must scale negative event and lab setback risk down by 10%.");

            auto addict=school_defaults("genius","rich",seed_hs);
            addict.choices["hs_drugs"]="liked_a_lot";
            addict.choices["hs_drugs_aftermath"]="did_not_stop";
            addict.choices["grad_choice"]="college";
            refuses([&]{ (void)content::founding_terms(catalog,addict); });
            addict.choices["grad_choice"]="workforce";
            const auto addict_cp=make(addict);
            require(core::founder_has_trait(addict_cp.world.simulation,"Addict"),"Dropping out must grant Addict on the workforce track.");

            auto paid=school_defaults("charisma","rich",seed_elem);
            paid.researcher="none";paid.choices["grad_choice"]="college";paid.choices["ug_funding"]="family_paid";paid.choices["grad_school"]="enter_market";
            auto loan_s=paid;loan_s.choices["ug_funding"]="loans";
            const auto paid_cp=make(paid);
            const auto loan_cp=make(loan_s);
            require(loan_cp.world.simulation.accounts->debts.size()==1 &&
                    loan_cp.world.simulation.accounts->debts.front().id==content::student_debt_id &&
                    loan_cp.world.simulation.accounts->debts.front().principal==core::Cents{8'500'000} &&
                    loan_cp.world.simulation.cash==paid_cp.world.simulation.cash,
                    "Student loans must create $85,000 in debt with zero extra startup cash.");
            require(persistence::decode_save(program,persistence::encode_save(program,loan_cp)).checkpoint==loan_cp,
                    "Student debt checkpoint must reload identically.");

            auto phd=school_defaults("political","rich",seed_elem);
            phd.researcher="founder";phd.choices["grad_choice"]="college";phd.choices["ug_funding"]="family_paid";phd.choices["grad_school"]="phd_ml";
            const auto phd_terms=content::founding_terms(catalog,phd);
            require(phd_terms.doctorate_count==1 && phd_terms.entry_delay_days>=1460 && phd_terms.research_work_scale_bp<10000,
                    "Non-genius Ph.D. must delay launch, confer a doctorate, and boost research speed.");
            const auto phd_cp=make(phd);
            require(phd_cp.world.campaign_start==scenario.starts_on &&
                    phd_cp.world.scope_created>phd_cp.world.campaign_start &&
                    phd_cp.world.simulation.date==phd_cp.world.scope_created &&
                    phd_cp.world.simulation.accounts->invoices.size()==0 &&
                    core::founder_has_trait(phd_cp.world.simulation,"Title: Dr.") &&
                    phd_cp.world.simulation.lab->research_work_scale_bp==core::BasisPoints{phd_terms.research_work_scale_bp},
                    "Delayed Ph.D. launch must preserve 2015 campaign_start, advance to launch date without pre-launch invoices, and grant Title: Dr.");
            require(persistence::decode_save(program,persistence::encode_save(program,phd_cp)).checkpoint==phd_cp,
                    "Delayed doctorate launch must reload cleanly.");
            {
                auto delayed_play=phd_cp;delayed_play.world.enabled_flows=std::vector<std::string>{};
                runtime::Runtime delayed_rt{program,std::move(delayed_play)};
                require(delayed_rt.prepare_opening(delayed_rt.current_state().world.simulation.revision).accepted(),
                        "Delayed doctorate launch must prepare its opening morning.");
                const auto first_day=delayed_rt.submit({delayed_rt.current_state().world.simulation.revision,core::AdvanceDay{}});
                require(first_day.accepted(),first_day.explanation.c_str());
            }

            auto dual=school_defaults("genius","rich",seed_elem);
            dual.choices["grad_choice"]="college";dual.choices["ug_funding"]="family_paid";
            dual.choices["grad_school"]="phd_ml";dual.choices["genius_second_phd"]="phd2_math";
            const auto dual_terms=content::founding_terms(catalog,dual);
            require(dual_terms.doctorate_count==2 && dual_terms.entry_delay_days==0,
                    "Actual Genius must compress dual Ph.D. study with zero launch delay unless serving in the army.");
            const auto dual_cp=make(dual);
            require(dual_cp.world.scope_created==dual_cp.world.campaign_start &&
                    dual_cp.world.simulation.lab->research_work_scale_bp<phd_cp.world.simulation.lab->research_work_scale_bp,
                    "Genius dual Ph.D. must open on campaign_start with compounded research speed.");

            auto army=dual;army.choices["ug_funding"]="army";
            const auto army_terms=content::founding_terms(catalog,army);
            const auto army_cp=make(army);
            require(army_terms.entry_delay_days>=1460 && army_cp.world.scope_created>army_cp.world.campaign_start,
                    "Army service must still delay launch by 4 years even for an Actual Genius.");
        }
        std::cout<<"Founder biography, calendar, costs, eligibility, nonprofit ownership and exact reload passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
