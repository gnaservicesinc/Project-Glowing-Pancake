#include "../../src/bridge/founding_fixture.hpp"
#include "founding_controller.hpp"
#if !PGP_DISTRIBUTION
#include <pgp/runtime/playtest_setup.hpp>
#endif
#include "world_controller.hpp"
#include "management_controller.hpp"
#include <pgp/runtime/campaign_identity.hpp>
#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <iostream>
#include <set>
#include <stdexcept>

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        using namespace pgp;
        const auto check=[](bool okay,const char* why){if(!okay) throw std::runtime_error(why);};
        const auto registry=content::foundation_registry();
        const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_F04_SOURCE,content::maximum_document_bytes),registry),registry);
        QTemporaryDir scratch;
        const auto path=std::filesystem::path{scratch.path().toStdString()}/"founding.json";
        bridge::FoundingController seeded{program.collection,path.parent_path()/"seeds.json"};
        std::set<std::uint64_t> world_seeds, story_seeds;
        for (int attempt=0;attempt<16;++attempt) {
            seeded.reset();
            check(world_seeds.insert(runtime::parse_world_seed(seeded.form().value("seed").toString().toStdString())).second &&
                story_seeds.insert(seeded.selection().story_seed).second,"Each new introduction needs fresh world and story seeds.");
        }
        seeded.setBeat("appearance");
        bridge::FoundingController resumed_seed{program.collection,path.parent_path()/"seeds.json"};
        check(resumed_seed.beat()=="appearance" && resumed_seed.form().value("seed")==seeded.form().value("seed") &&
            resumed_seed.selection().story_seed==seeded.selection().story_seed,"Resuming a draft must retain both seeds.");
        const auto opener_path=path.parent_path()/"opening-history"/"founding-draft.json";
        QString previous_opening;
        for (int attempt=0;attempt<16;++attempt) {
            bridge::FoundingController next{program.collection,opener_path};
            next.reset();
            const auto first="chapter-"+next.chapters().front().toMap().value("id").toString();
            check(first!=previous_opening,"A new playthrough should avoid the last school opening across relaunches.");
            next.setBeat(first);
            check(std::filesystem::exists(opener_path.parent_path()/"founding-history.ini"),
                "The school opening must be remembered on disk.");
            check(std::filesystem::remove(opener_path),"Delete the previous draft while retaining its opening history.");
            previous_opening=first;
        }
        bridge::FoundingController setup{program.collection,path};
        setup.setField("gender","woman");setup.setField("pronouns","she/her");
        setup.setField("gender","arbitrary");setup.setField("pronouns","arbitrary");
        check(setup.form().value("gender")=="woman" && setup.form().value("pronouns")=="she/her","Identity must use structured options.");
        check(!setup.answerText("gender","free text").isEmpty(),"Text entry must not bypass identity choices.");
        setup.setField("pronouns","xe/xem");
        check(setup.form().value("pronouns")=="xe/xem","Additional pronouns must persist as a structured selection.");
        setup.setField("pronouns","name only");
        check(setup.form().value("pronouns")=="xe/xem","A name is not a pronoun option.");
        for(const auto& pronoun:QStringList{"hu/hum","fae/faer","hir/hir","ey/em","ze/zir","xe/xem"}) {
            setup.setField("pronouns",pronoun);check(setup.form().value("pronouns")==pronoun,"Requested neopronouns must be selectable.");
        }
        setup.setField("gender","unspecified");setup.setField("orientation","unspecified");
        check(setup.form().value("gender")!="unspecified" && setup.form().value("orientation")!="unspecified","Nondisclosure is not an identity.");
        const auto before_color=setup.portrait();setup.setAppearance(23,4);
        check(setup.portrait()!=before_color,"Brow color must be editable independently.");
        const auto initial=setup.portrait();
        for(int i=0;i<27;++i) setup.cycleAppearance(i,1);
        check(setup.portrait()!=initial,"Every appearance layer must be editable.");
        setup.setBeat("appearance");
        bridge::FoundingController reopened{program.collection,path};
        check(reopened.portrait()==setup.portrait() && reopened.beat()=="appearance","Current-build drafts must retain the edited portrait.");
        setup.setField("family","rich");setup.setField("route","bootstrap");
        setup.setAdvantage("genius",true);setup.setAdvantage("charisma",true);
        check(setup.backers().empty() && setup.form().value("office")=="home","A self-funded founder must not receive unsolicited backers.");
        pgp::bridge::complete_formative_fixture(setup);
        setup.setField("investment","100000");
        setup.setField("charter","charter.independent_lab");
        check(setup.signingError().isEmpty(),"A reviewed investment within personal savings must be signable.");
        check(setup.preview().contains("employee program $1,500.00/mo"),"The signing preview must disclose optional bylaws' actual recurring cost.");
        setup.setBeat("home");setup.setField("home_app","formation");
        bridge::FoundingController home_draft{program.collection,path};
        check(home_draft.beat()=="home" && home_draft.form().value("investment")=="100000" && home_draft.form().value("home_app")=="formation","Pre-company home and reviewed investment must survive a restart without founding.");
        const auto offer=[](const bridge::FoundingController& story, const QString& id) {
            for (const auto& row:story.homeOffers()) if (row.toMap().value("id")==id) return row.toMap();
            return QVariantMap{};
        };
        bridge::FoundingController applicant{program.collection,path.parent_path()/"applicant.json"};
        applicant.setAdvantage("charisma",true);applicant.setBeat("home");
        check(offer(applicant,"northstar").value("status")=="listed" && applicant.contactHomeOffer("northstar").isEmpty(),"An ordinary founder can apply to the lab.");
        check(offer(applicant,"northstar").value("status")=="rejected" && !applicant.respondHomeOffer("northstar","accept").isEmpty(),"A rejected application must close without an appointment.");
        bridge::FoundingController rejected{program.collection,path.parent_path()/"applicant.json"};
        check(offer(rejected,"northstar").value("status")=="rejected" && rejected.form().value("home_offer")=="northstar","A rejection and its selected thread must survive restart.");
        bridge::FoundingController invited{program.collection,path.parent_path()/"invited.json"};
        invited.setField("family","poor");invited.setAdvantage("genius",true);invited.setBeat("home");
        check(offer(invited,"northstar").value("source")=="recruiter" && offer(invited,"frontier").value("source")=="introduction","A genius without family wealth should receive both approaches.");
        check(!invited.respondHomeOffer("northstar","accept").isEmpty(),"Review a message before responding.");
        check(invited.contactHomeOffer("northstar").isEmpty() && invited.respondHomeOffer("northstar","later").isEmpty(),"An invitation can be kept for later.");
        check(invited.contactHomeOffer("frontier").isEmpty() && invited.respondHomeOffer("frontier","decline").isEmpty(),"An investor can be turned down.");
        bridge::FoundingController inbox{program.collection,path.parent_path()/"invited.json"};
        check(offer(inbox,"northstar").value("status")=="considering" && offer(inbox,"frontier").value("status")=="declined" &&
              inbox.form().value("home_offer")=="frontier" && !inbox.respondHomeOffer("frontier","accept").isEmpty(),"Held and declined replies must resume with their original choices.");
        bridge::FoundingController unwritable{program.collection,path/"unwritable.json"};
        unwritable.setBeat("home");
        const auto before_failure=unwritable.form();
        check(!unwritable.contactHomeOffer("northstar").isEmpty() && unwritable.form()==before_failure,
            "A failed draft write must report the error and keep the previous conversation state.");
        runtime::ScenarioOptions options;
        options.founding=setup.selection();
        options.charter=setup.form().value("charter").toString().toStdString();
        options.identity=runtime::CampaignIdentity{"Test lab",options.founding->first_name+" "+options.founding->last_name,"she/her"};
        const auto checkpoint=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
        const auto& catalog=*std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data).founding;
        const auto terms=content::founding_terms(catalog,*options.founding);
        check(core::active_charter(checkpoint.world.simulation) && core::active_charter(checkpoint.world.simulation)->id==options.charter,"Founding bylaws must become the actual active charter.");
        check(terms.personal_contribution_cents==10000000 && checkpoint.world.simulation.succession->personal_cash==core::Cents{terms.personal_remaining_cents},"Only the chosen investment enters company funds; remaining savings stay personal.");
        check(persistence::decode_save(program,persistence::encode_save(program,checkpoint)).checkpoint==checkpoint,"Personal savings and appearance styling must reload exactly.");
        bridge::WorldController world{std::filesystem::path{scratch.path().toStdString()}/"world"};
        world.update(std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{1,checkpoint,{},true,false}),program,true);
        check(world.founder().value("portrait")==setup.portraitForAge(18),"The world profile must show the customized portrait.");
        const auto biography=world.founder().value("bio").toString();
        check(biography.contains("Actual Genius") && biography.contains("Extremely Charismatic") && biography.contains("Wealthy Elite"),
            "Every applicable advantage must appear in the founder's profile.");
        const auto workspaces=[](QVariantList rooms){
            rooms.erase(std::remove_if(rooms.begin(),rooms.end(),[](const auto& room){return room.toMap().value("passage").toBool();}),rooms.end());
            return rooms;
        };
        const auto hqRooms=workspaces(world.headquarters().value("rooms").toList());
        const auto familyRooms=workspaces(world.familyHome().value("rooms").toList());
        const QStringList finishes{"concrete","concrete","concrete","carpet","oak","oak"};
        check(hqRooms.size()==finishes.size(),"The rich family home must keep its six distinct rooms.");
        for(qsizetype i=0;i<finishes.size();++i) {
            check(hqRooms[i].toMap().value("flooring")==finishes[i],"Home finishes must follow room use, not family wealth.");
            check(familyRooms[i].toMap().value("flooring")==finishes[i],"Family and HQ views must agree on finishes.");
        }
        const auto listings=world.hqListings(world.headquarters().value("market").toString());
        for(const auto& listing:listings) {
            const auto row=listing.toMap();
            check(row.value("flooring")== (row.value("plan")=="floor"?"oak":"carpet"),"Commercial listing finishes must follow the property plan.");
            check(row.value("detail").toString().contains(row.value("flooringLabel").toString()),"Lease terms must disclose the finish shown in the preview.");
        }
        bridge::ManagementController management;
        management.update(std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{1,checkpoint,{},true,false}),program,true);
        check(management.recommended().value("trainingRate").toString()=="0" && management.recommended().value("testingRate").toString()=="0","The lab must open without inventing or dereferencing missing compute.");
        check(setup.contactHomeOffer("commercial").isEmpty() && setup.respondHomeOffer("commercial","accept").isEmpty(),"The investment route must solicit and accept a named offer.");
        const auto funded=setup.selection();
        const auto funded_terms=content::founding_terms(catalog,funded);
        check(offer(setup,"commercial").value("status")=="selected" && funded_terms.backer_shares.at("commercial")==2000 &&
            funded_terms.contributed_cents>terms.contributed_cents && setup.signingError().isEmpty(),"The selected offer must move capital and equity into the actual founding terms.");
        runtime::ScenarioOptions funded_options;
        funded_options.founding=funded;
        funded_options.opening_contacts=setup.openingContacts();
        funded_options.identity=runtime::CampaignIdentity{"Test lab",funded_options.founding->first_name+" "+funded_options.founding->last_name,"xe/xem"};
        auto financed=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},funded_options);
        financed.world.enabled_flows=std::vector<std::string>{};
        runtime::Runtime conversation{program,financed};
        check(conversation.prepare_opening(conversation.current_state().world.simulation.revision).accepted(),"The contact's company must prepare its first morning.");
        const auto send=[&](std::string id,std::string topic,std::string device="laptop",std::uint32_t room=0) {
            return conversation.submit({conversation.current_state().world.simulation.revision,
                core::ManagePeopleContact{core::MessagePeopleContact{std::move(id),std::move(topic),std::move(device),room}}});
        };
        const auto refuse=[&](std::string id,std::string topic,std::string device="laptop",std::uint32_t room=0) {
            const auto before=conversation.current_state();
            check(!send(std::move(id),std::move(topic),std::move(device),room).accepted() && conversation.current_state()==before,
                "Unsupported contact topics, devices and duplicate replies must refuse atomically.");
        };
        refuse("opening/commercial","opening_terms","tablet");
        refuse("opening/commercial","opening_terms","",1);
        refuse("opening/unknown","opening_terms");
        refuse("opening/commercial","workstation");
        check(send("opening/commercial","opening_terms").accepted() && send("opening/commercial","board_role").accepted(),
            "The signed investor must answer about the real agreement and board.");
        const auto commercial_contact = std::ranges::find(
            conversation.current_state().world.simulation.management->people->contacts,
            std::string{"opening/commercial"}, &core::PeopleContact::person);
        check(commercial_contact != conversation.current_state().world.simulation.management->people->contacts.end() &&
              !commercial_contact->messages.empty() && commercial_contact->messages.back().reply == "has_seat",
            "The investor's board reply must reflect the actual seat created by their signed agreement.");
        refuse("opening/commercial","opening_terms");
        refuse("opening/northstar","board_role");
        check(send("opening/northstar","opening_terms").accepted(),"An unsigned invitation must remain accessible with its recorded outcome.");
        const auto update=[&](const runtime::Checkpoint& saved) {
            world.update(std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{2,saved,{},true,false}),program,true);
        };
        update(conversation.current_state());
        const auto commercial=world.contactThread("opening/commercial");
        check(world.contacts().size()==static_cast<qsizetype>(funded_options.opening_contacts.size()) && commercial.value("status")=="Signed at opening" &&
            commercial.value("messages").toList().back().toMap().value("reply").toString().contains("seat on the current board"),
            "The company app must list the original contact, signed terms and real board reply.");
        const auto continued=persistence::decode_save(program,persistence::encode_save(program,conversation.current_state()));
        check(continued.checkpoint==conversation.current_state(),"Opening conversations and replies must reload exactly.");
        update(continued.checkpoint);
        check(world.contactThread("opening/commercial")==commercial,"Reload must preserve the projected conversation and topic states.");
        auto forged=financed;
        const auto unsigned_contact = std::ranges::find(
            forged.world.simulation.management->people->contacts,
            std::string{"opening/northstar"}, &core::PeopleContact::person);
        check(unsigned_contact != forged.world.simulation.management->people->contacts.end(),
              "Expected unsigned opening/northstar contact in funded fixture.");
        unsigned_contact->opening->outcome="signed";
        bool rejected_forgery=false;
        try {runtime::validate_checkpoint(program,forged);}catch(const std::invalid_argument&){rejected_forgery=true;}
        check(rejected_forgery,"A saved contact must not invent a signed agreement absent from the founding selection.");
        setup.setField("route","hired_ceo");
        check(setup.selection().route=="investment","A home form cannot bypass the recruiter's reply.");
        const auto companyDraft = setup.form().value("company");
        check(setup.contactHomeOffer("northstar").isEmpty() && setup.respondHomeOffer("northstar","accept").isEmpty(),"An active invitation can open the appointment paperwork.");
        check(setup.form().value("charter").toString()=="charter.independent_lab","A CEO appointment must inherit the company's existing bylaws.");
        setup.setField("company","Hijacked Name");
        setup.setField("mission","profit");
        check(setup.form().value("company")=="Northstar Applied Labs" && setup.form().value("mission")=="survival",
            "An appointment letter must refuse edits to the existing company's name and mission.");
        check(setup.backers().empty() && setup.form().value("backers").toStringList().empty() && setup.form().value("company")=="Northstar Applied Labs","Changing route must discard incompatible funding and identify the existing company.");
        const auto appointment=content::founding_terms(catalog,setup.selection());
        check(setup.signingError().isEmpty() && appointment.contributed_cents==65000000 && appointment.founder_bp==500 &&
            appointment.personal_contribution_cents==0,"Accepting the appointment must use the existing lab's capitalization and leave personal cash alone.");
        runtime::ScenarioOptions appointment_options;
        appointment_options.founding=setup.selection();
        appointment_options.opening_contacts=setup.openingContacts();
        appointment_options.identity=runtime::CampaignIdentity{"Northstar Applied Labs",appointment_options.founding->first_name+" "+appointment_options.founding->last_name,"xe/xem"};
        const auto appointed=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},appointment_options);
        const auto& lab=appointed.world.simulation;
        check(lab.management->people->employees.size()==appointment.opening_staff && lab.cash==core::Cents{appointment.contributed_cents} &&
            lab.succession->personal_cash==core::Cents{appointment.personal_remaining_cents} &&
            persistence::decode_save(program,persistence::encode_save(program,appointed)).checkpoint==appointed,
            "The appointment from the inbox must open and reload an existing lab with its staff and separate personal savings.");
        // F-08A: Inherited mission authority, economic-vs-voting control distinction, and survivable mission-reversal scandal.
        {
            auto auth_check = core::company_authority(appointed.world.simulation);
            check(!auth_check.can_change_mission && !auth_check.has_economic_control && !auth_check.has_voting_control &&
                  auth_check.player_voting_seats == 1 && auth_check.total_voting_seats == 4,
                  "An appointed CEO must inherit 1 of 4 board votes and lack authority to rewrite mission.");
            auto ceo_start = appointed;
            ceo_start.world.developer_playtest = true;
            ceo_start.world.enabled_flows = std::vector<std::string>{};
            // Give personal cash to test buyout path (via playtest cash for exact personal cash conservation)
            ceo_start.world.simulation.succession->playtest_cash = core::Cents{5000000000LL};
            ceo_start.world.simulation.succession->personal_cash = core::checked_unit_add(
                ceo_start.world.simulation.succession->personal_cash, core::Cents{5000000000LL});
            runtime::Runtime ceo_rt{program, ceo_start};
            check(ceo_rt.prepare_opening(ceo_rt.current_state().world.simulation.revision).accepted(), "Prepare CEO first morning.");
            // Refuses mission rewrite before control
            check(!ceo_rt.submit({ceo_rt.current_state().world.simulation.revision, core::ChangeCompanyMission{"commercial"}}).accepted(),
                  "Appointed CEO without voting control must be refused from rewriting company mission.");
            // Buy economic stake (>50% equity) — must NOT grant voting control
            check(ceo_rt.submit({ceo_rt.current_state().world.simulation.revision, core::AcquireCompanyControl{"buy_economic_stake"}}).accepted(),
                  "Economic stake buyout should succeed with sufficient personal funds.");
            const auto after_econ = core::company_authority(ceo_rt.current_state().world.simulation);
            check(after_econ.has_economic_control && !after_econ.has_voting_control && !after_econ.can_change_mission,
                  "Economic majority alone must NOT grant board voting control or mission-change authority.");
            check(!ceo_rt.submit({ceo_rt.current_state().world.simulation.revision, core::ChangeCompanyMission{"commercial"}}).accepted(),
                  "Mission rewrite must still be refused until board voting control is acquired.");
            // Acquire voting control
            check(ceo_rt.submit({ceo_rt.current_state().world.simulation.revision, core::AcquireCompanyControl{"acquire_voting_control"}}).accepted(),
                  "Board voting control buyout should succeed after acquiring controlling economic stake.");
            const auto after_vote = core::company_authority(ceo_rt.current_state().world.simulation);
            check(after_vote.has_voting_control && after_vote.can_change_mission,
                  "Acquiring board voting control must unlock fundamental mission change authority.");
            // Reversing inherited purpose succeeds and triggers survivable scandal
            check(ceo_rt.submit({ceo_rt.current_state().world.simulation.revision, core::ChangeCompanyMission{"commercial"}}).accepted(),
                  "Mission change should succeed once controlling voting seats are held.");
            const auto* scandal = core::mission_scandal(ceo_rt.current_state().world.simulation);
            check(scandal && !scandal->responded_on && scandal->employee_morale_hit < 0 && scandal->donor_customer_reaction_bp < 0,
                  "Reversing inherited company purpose must trigger a visible scandal with morale and demand backlash.");
            check(ceo_rt.submit({ceo_rt.current_state().world.simulation.revision, core::RespondToMissionScandal{"transition_reserve"}}).accepted(),
                  "Survivable scandal response must be accepted.");
            check(core::mission_scandal(ceo_rt.current_state().world.simulation)->responded_on.has_value() &&
                  persistence::decode_save(program, persistence::encode_save(program, ceo_rt.checkpoint())).checkpoint == ceo_rt.checkpoint(),
                  "Control acquisition, scandal, and scandal response must reload identically.");
        }
        update(appointed);
        check(world.contactThread("opening/northstar").value("status")=="Signed at opening" &&
            world.contactThread("opening/commercial").value("status")=="Expired at opening",
            "Accepting Northstar must preserve the signed appointment and the unsigned investor conversation.");
        check(setup.respondHomeOffer("northstar","later").isEmpty() && setup.selection().route=="investment" &&
            setup.form().value("company")==companyDraft && offer(setup,"commercial").value("status")=="considering", "Setting a selected appointment aside must restore the player's own company draft and preserve conversations.");
        {
            const auto find_opt=[](const QVariantMap& chapter,const QString& id) {
                for(const auto& v:chapter.value("choices").toList()) if(v.toMap().value("id")==id) return v.toMap();
                return QVariantMap{};
            };
            const auto find_chapter=[](const bridge::FoundingController& ctrl,const QString& id) {
                for(const auto& v:ctrl.chapters()) if(v.toMap().value("id")==id) return v.toMap();
                return QVariantMap{};
            };
            const auto advantage_shown=[](const bridge::FoundingController& ctrl,const QString& id) {
                for(const auto& v:ctrl.advantageOptions()) if(v.toMap().value("id")==id) return true;
                return false;
            };
            bridge::FoundingController school{program.collection,path.parent_path()/"school.json"};
            school.setField("family","poor");
            school.setAdvantage("wealthy",true);school.setAdvantage("political",true);
            check(school.selection().advantages.empty() && !school.selection().has_advantage("wealthy") &&
                !advantage_shown(school,"wealthy") && !advantage_shown(school,"political"),
                "A poor founder can continue without any advantages, and cannot select elite backgrounds.");
            school.setField("family","rich");
            check(school.selection().has_advantage("wealthy") && school.personalFunds().value("dollars")=="7000000" &&
                !advantage_shown(school,"wealthy") && advantage_shown(school,"political"),
                "Choosing the wealthy household must immediately apply its savings and unlock the political option.");
            school.setAdvantage("genius",true);school.setAdvantage("charisma",true);school.setAdvantage("political",true);
            school.setBeat("advantages");
            bridge::FoundingController resumed_school{program.collection,path.parent_path()/"school.json"};
            check(resumed_school.beat()=="advantages" && resumed_school.selection()==school.selection() &&
                resumed_school.selection().advantages.size()==3 && resumed_school.personalFunds().value("dollars")=="7100000",
                "All three optional advantages must coexist, affect savings and resume exactly.");
            school.setField("family","poor");
            check(!school.selection().has_advantage("wealthy") && !school.selection().has_advantage("political") &&
                school.selection().has_advantage("genius") && school.selection().has_advantage("charisma"),
                "Changing family background must clear only the now-ineligible elite advantages.");

            school.setField("family","rich");school.setAdvantage("charisma",false);
            school.choose("ms_sport","no_sport");
            school.choose("ms_clubs","robotics");school.choose("ms_clubs","coding");school.choose("ms_clubs","debate");
            check(find_chapter(school,"ms_clubs").value("maxSelections").toInt()==3 &&
                  school.form().value("choices").toMap().value("ms_clubs")=="robotics,coding,debate",
                  "Middle school clubs must allow up to 3 multi-selections without a sport.");
            school.choose("ms_clubs","drama");
            check(school.form().value("choices").toMap().value("ms_clubs")=="robotics,coding,debate",
                  "Middle school clubs must not exceed 3 selections.");
            school.choose("ms_sport","basketball_thrive");
            check(find_chapter(school,"ms_clubs").value("maxSelections").toInt()==2 &&
                  school.form().value("choices").toMap().value("ms_clubs").toString().split(',',Qt::SkipEmptyParts).size()<=2,
                  "Picking a middle school sport must reduce club cap to 2 and clamp existing selections.");

            for(int seed=1;seed<4096;++seed) {
                school.setField("story_seed",seed);
                if(!find_chapter(school,"ms_bully").isEmpty()) break;
            }
            check(!find_chapter(school,"ms_bully").isEmpty(),"Must find a story_seed revealing ms_bully in bridge.");
            check(!find_opt(find_chapter(school,"ms_bully"),"befriend").value("available").toBool(),
                  "Befriend bullies option must be unavailable without Charisma.");
            school.setAdvantage("charisma",true);
            check(find_opt(find_chapter(school,"ms_bully"),"befriend").value("available").toBool(),
                  "A founder can unlock the Charisma option while keeping Genius.");

            for(int seed=1;seed<4096;++seed) {
                school.setField("story_seed",seed);
                if(!find_chapter(school,"hs_drugs").isEmpty()) break;
            }
            check(!find_chapter(school,"hs_drugs").isEmpty(),"Must find a story_seed revealing hs_drugs in bridge.");
            school.choose("hs_drugs","liked_a_lot");
            school.choose("hs_drugs_aftermath","did_not_stop");
            const auto blocked_college=find_opt(find_chapter(school,"grad_choice"),"college");
            check(!blocked_college.value("available").toBool() &&
                  blocked_college.value("reason").toString().contains("Addict"),
                  "Addict trait must close the university track with an explicit reason.");

            school.choose("hs_drugs","pass");
            school.setAdvantage("genius",false);school.setAdvantage("political",true);school.setField("route","bootstrap");
            school.setField("office","home");school.setField("researcher","founder");
            school.choose("grad_choice","apprentice");
            check(school.education().value("entryDelayDays").toInt()==180 &&
                school.education().value("launchDate")=="2015-06-30" &&
                find_opt(find_chapter(school,"grad_choice"),"apprentice").value("detail").toString().contains("180 day"),
                "A short career route must disclose its exact delay and date before financing.");
            school.choose("grad_choice","contract");
            check(school.education().value("entryDelayDays").toInt()==90 &&
                school.education().value("launchDate")=="2015-04-01",
                "Software contracts must open the founder timeline 90 days after the market.");
            pgp::bridge::complete_formative_fixture(school);
            const auto& founder_scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
            const auto contract_selection=school.selection();
            const auto opening=content::scenario_opening(founder_scenario,"","company.player","",&contract_selection);
            check(QString::fromStdString(core::iso_date(opening.launch_day))==school.education().value("launchDate").toString(),
                "The home date must match the actual start of the selected campaign.");
            school.choose("grad_choice","college");school.choose("ug_funding","loans");school.choose("grad_school","phd_ml");
            check(school.education().value("doctorateCount").toInt()==1 && school.education().value("hasStudentDebt").toBool(),
                "The profile and student debt must be visible before the formation review.");
            pgp::bridge::complete_formative_fixture(school);
            school.setField("investment",QString::number(catalog.formation_cents+500000));
            const auto prev=school.preview();
            check(prev.contains("Personal student debt at entry") && prev.contains("Delayed start") &&
                  prev.contains("research effort needed") && !prev.contains("Research speed"),
                  "Preview must disclose student debt, delayed launch years, and unambiguous research effort.");
            runtime::ScenarioOptions dr_options;
            dr_options.founding=school.selection();
            dr_options.identity=runtime::CampaignIdentity{"Doctorate Labs",dr_options.founding->first_name+" "+dr_options.founding->last_name,school.form().value("pronouns").toString().toStdString()};
            const auto dr_cp=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},dr_options);
            update(dr_cp);
            check(world.founder().value("name").toString().startsWith("Dr. "),
                  "Doctoral founders must display the Dr. title prefix in WorldController::founder().");

            for(int seed=1;seed<4096;++seed) {
                school.setField("story_seed",seed);
                if(!find_chapter(school,"ms_bully").isEmpty()) break;
            }
            school.choose("ms_bully","ignore");
            school.choose("ms_bully_aftermath","accept");
            school.choose("hs_bully_continuation","endure_hs");
            pgp::bridge::complete_formative_fixture(school);
            runtime::ScenarioOptions bully_options;
            bully_options.founding=school.selection();
            bully_options.identity=runtime::CampaignIdentity{"Resilient Labs",bully_options.founding->first_name+" "+bully_options.founding->last_name,school.form().value("pronouns").toString().toStdString()};
            const auto bully_cp=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},bully_options);
            update(bully_cp);
            const auto rel=world.relationships();
            int bully_cards=0,bully_edges=0;
            for(const auto& row:rel.value("people").toList()) {
                const auto m=row.toMap();
                if(m.value("rival").toBool() && m.value("relationship")=="Rivals" &&
                   m.value("memory").toString().contains("Targeted you in school")) {
                    ++bully_cards;
                }
            }
            for(const auto& row:rel.value("edges").toList()) {
                if(row.toMap().value("label")=="School bully · Rivals") ++bully_edges;
            }
            check(bully_cards>=4 && bully_cards<=8 && bully_edges==bully_cards,
                  "Unresolved school bullies must appear in both SocialPanel people cards and relationship graph edges.");
        }
#if !PGP_DISTRIBUTION
        // F-08A: Playtester one-screen freely configurable setup & start (two contrasting configs + invalid refusal + exact reload).
        {
            auto invalid_cfg = runtime::default_playtest_setup(program);
            invalid_cfg.birth_year = invalid_cfg.launch_year - 12; // underage
            invalid_cfg.legal_form = "nonprofit";
            invalid_cfg.objective = "profit";
            const auto errs = runtime::validate_playtest_setup(program, invalid_cfg);
            check(errs.size() >= 2, "Playtest setup validation must report underage start and nonprofit profit conflict.");

            auto cfg_a = runtime::default_playtest_setup(program);
            cfg_a.first_name = "Sora";
            cfg_a.last_name = "Lin";
            cfg_a.company_name = "Sora Garage Labs";
            cfg_a.headquarters = "US-TX";
            cfg_a.office = "home";
            cfg_a.company_cash = core::Cents{25000000};
            cfg_a.personal_cash = core::Cents{5000000};
            cfg_a.company_debt = core::Cents{1500000};
            cfg_a.industry_stage = 2;
            cfg_a.family = "workshop";
            cfg_a.launch_year += 1;
            cfg_a.personal_debt = core::Cents{340000};
            cfg_a.extra_staff_count = 0;
            cfg_a.completed_research_ids.clear();
            cfg_a.owned_dataset_ids.clear();
            const auto cp_a = runtime::begin_playtest_scenario(program, cfg_a);
            check(cp_a.world.developer_playtest && cp_a.world.playtest_custom && cp_a.world.simulation.paused, "Direct starts must open paused.");
            check(core::headquarters_home(cp_a.world.simulation).plan.id == "home.workshop" &&
                  cp_a.world.founding->family == "workshop" && cp_a.world.founding->advantages.empty(), "Keep the repair-shop home and requested advantages.");
            check(core::from_ordinal(cp_a.world.simulation.date).year == cfg_a.launch_year, "Start on the chosen launch year.");
            check(runtime::industry_activity(cp_a.world).stage == 2, "Apply the selected initial industry conditions.");
            check(cp_a.world.state_id == "state.us_tx" && cp_a.world.simulation.cash == cfg_a.company_cash, "Preserve selected state and company cash after debt.");
            check(persistence::decode_save(program, persistence::encode_save(program, cp_a)).checkpoint == cp_a, "Reload the custom founder and term balances exactly.");

            auto cfg_b = runtime::default_playtest_setup(program);
            cfg_b.first_name = "Elena";
            cfg_b.last_name = "Vance";
            cfg_b.company_name = "Cascade Frontier Institute";
            cfg_b.headquarters = "US-WA";
            cfg_b.office = "leased";
            cfg_b.company_cash = core::Cents{450000000};
            cfg_b.personal_cash = core::Cents{120000000};
            cfg_b.extra_staff_count = 3;
            cfg_b.extra_laptops = 2;
            cfg_b.extra_desktops = 2;
            cfg_b.starting_compute = core::MilliCUPerDay{250000};
            cfg_b.include_starter_model = true;
            cfg_b.include_starter_product = true;
            cfg_b.completed_research_ids = {"research.held_out_evaluation", "research.reproducible_tooling"};
            cfg_b.owned_dataset_ids = {"data.licensed_documents", "data.claims_redacted"};
            const auto cp_b = runtime::begin_playtest_scenario(program, cfg_b);
            check(cp_b.world.developer_playtest && cp_b.world.playtest_custom && cp_b.world.simulation.paused &&
                  cp_b.world.state_id == "state.us_wa" && cp_b.world.simulation.cash == cfg_b.company_cash &&
                  !cp_b.world.simulation.lab->models.empty() && !cp_b.world.simulation.deployments->records.empty() &&
                  persistence::decode_save(program, persistence::encode_save(program, cp_b)).checkpoint == cp_b,
                  "Playtest custom setup B (funded WA lab with starter model) must start paused and reload identically.");
        }
#endif
        std::cout<<"Structured identity, appearance editing, current drafts, founder profile and route dependencies passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
