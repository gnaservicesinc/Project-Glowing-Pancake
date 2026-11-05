#include "competition_history_json.hpp"
#include <pgp/core/simulation.hpp>
#include <map>

namespace pgp::persistence::detail {
namespace {
struct Dictionary {
    Json strings=Json::array();
    std::map<std::string,std::size_t,std::less<>> indices;
    CompetitionHistoryBudget& budget;
    bool intern=true;
    void count(const Json& value) {
        if (!value.is_string()) invalid("competition/history/string","Expected a string column");
        const auto size=value.get_ref<const std::string&>().size();
        if (size>512 || budget.string_bytes>64U*1024U*1024U-size) invalid("competition/history","Expanded history string budget exceeded");
        budget.string_bytes+=size;
    }
    void convert(Json& value,bool expand) {
        if (!intern) {count(value);return;}
        if (expand) {
            const auto index=u64(value,"competition/history/string_index");
            if (index>=strings.size()) invalid("competition/history/string_index","String reference is outside its bounded table");
            const auto& source=strings.at(static_cast<std::size_t>(index));count(source);value=source;
        } else {
            count(value);const auto& key=value.get_ref<const std::string&>();
            const auto [it,inserted]=indices.emplace(key,strings.size());
            if (inserted) {if (strings.size()>=100000) invalid("competition/history","String table exceeds its bound");strings.push_back(key);}
            value=it->second;
        }
    }
};
void rows(Json& values,std::size_t maximum,std::initializer_list<std::string_view> columns,bool expand,Dictionary& dictionary,
    std::initializer_list<std::string_view> strings={}, std::initializer_list<std::string_view> tail={}) {
    array(values,"competition/history",maximum);
    for (auto& row:values) {
        if (dictionary.budget.cells>4'000'000-columns.size()-tail.size()) invalid("competition/history","Expanded history cell budget exceeded");
        dictionary.budget.cells+=columns.size()+tail.size();
        Json result=expand?Json::object():Json::array();
        if (expand) {
            array(row,"competition/history/row",columns.size()+tail.size());
            if (row.size()<columns.size()) invalid("competition/history/row","Missing required history columns");
            std::size_t index=0;for (const auto column:columns) {
                auto value=std::move(row.at(index++));
                if (std::find(strings.begin(),strings.end(),column)!=strings.end()) dictionary.convert(value,true);
                result[column]=std::move(value);
            }
            for (const auto column:tail) {
                if (index==row.size()) break;
                result[column]=std::move(row.at(index++));
            }
        } else {
            fields(row,"competition/history/row",columns,tail);
            for (const auto column:columns) {
                auto value=std::move(row.at(column));
                if (std::find(strings.begin(),strings.end(),column)!=strings.end()) dictionary.convert(value,false);
                result.push_back(std::move(value));
            }
            bool ended=false;
            for (const auto column:tail) {
                if (!row.contains(column)) { ended=true;continue; }
                if (ended) invalid("competition/history/row","Optional history columns must be a suffix");
                result.push_back(std::move(row.at(column)));
            }
        }
        row=std::move(result);
    }
}
Json convert(Json simulation,bool expand,CompetitionHistoryBudget& budget,bool dictionary_format) {
    if (!simulation.is_object()) invalid("competition/simulation","Expected named current state");
    Dictionary dictionary{Json::array(),{},budget,dictionary_format};
    if (expand && dictionary_format) {
        if (!simulation.contains("history_strings")) invalid("competition/history","The explicit string table is missing");
        dictionary.strings=std::move(simulation.at("history_strings"));simulation.erase("history_strings");
        array(dictionary.strings,"competition/history/strings",100000);std::set<std::string> unique;
        for (const auto& row:dictionary.strings) if (!unique.insert(text(row,"competition/history/string",512,true)).second) invalid("competition/history","Repeated string-table entry");
    } else if (simulation.contains("history_strings")) invalid("competition/history","Unknown string-table storage format");
    rows(simulation.at("receipts"),core::simulation_receipt_limit,{"id","revision","day","cause","reservation_id","cash_delta_cents"},expand,dictionary,{"cause"});
    if (auto& accounts=simulation.at("accounts");!accounts.is_null()) {
        rows(accounts.at("postings"),core::accounts_posting_limit,{"id","day","debit","credit","amount_cents","cause","owner","key","document_id"},expand,dictionary,{"debit","credit","cause","owner","key","document_id"});
        rows(accounts.at("invoices"),core::accounts_document_limit,{"id","kind","due","amount_cents","settled_cents","cause"},expand,dictionary,{"id","kind","cause"});
    }
    if (auto& capacity=simulation.at("capacity");!capacity.is_null()) {
        auto& days=capacity.at("days");array(days,"competition/capacity/days",core::capacity_day_limit);
        if (expand) rows(days,core::capacity_day_limit,{"day","sources","projects"},true,dictionary);
        for (auto& day:days) {
            rows(day.at("sources"),core::capacity_source_limit,{"source_id","effective_milli_CU_per_day","used_milli_CU"},expand,dictionary,{"source_id"},
                {"wholesale_available_milli_CU_per_day","wholesale_used_milli_CU"});
            rows(day.at("projects"),core::capacity_project_limit,{"project_id","work_milli_CU","useful_tasks","assigned_staff","protected_tasks"},expand,dictionary,{"project_id"});
        }
        if (!expand) rows(days,core::capacity_day_limit,{"day","sources","projects"},false,dictionary);
    }
    if (auto& market=simulation.at("market");!market.is_null()) {
        auto& days=market.at("days");array(days,"competition/market/days",core::market_day_limit);
        if (expand) rows(days,core::market_day_limit,{"day","contracts","discretionary","segments","allocation_rounds"},true,dictionary,{},{"total_demand"});
        for (auto& day:days) {
            rows(day.at("contracts"),core::market_contract_limit,{"contract_id","promised_tasks","delivered_tasks","gross_fee_cents","service_credit_cents","usage_charge_cents","penalty_cents"},expand,dictionary,{"contract_id"});
            rows(day.at("discretionary"),core::market_segment_limit*core::market_product_limit,{"segment_id","product_id","tasks","charged_cents"},expand,dictionary,{"segment_id","product_id"},
                {"customer_compute_milli_CU"});
            rows(day.at("segments"),core::market_segment_limit,{"segment_id","demand","budget_cents","contract_tasks","discretionary_tasks","outside_tasks","charged_cents","unspent_cents","outside_reason"},expand,dictionary,{"segment_id","outside_reason"});
        }
        if (!expand) rows(days,core::market_day_limit,{"day","contracts","discretionary","segments","allocation_rounds"},false,dictionary,{},{"total_demand"});
    }
    if (!expand && dictionary_format) simulation["history_strings"]=std::move(dictionary.strings);
    return simulation;
}
}
Json compact_competition_history(Json simulation,CompetitionHistoryBudget& budget) {return convert(std::move(simulation),false,budget,true);}
Json expand_competition_history(Json simulation,CompetitionHistoryBudget& budget,bool dictionary) {return convert(std::move(simulation),true,budget,dictionary);}
}
