#pragma once

#include "civic_codec.hpp"
#include <pgp/core/corporate_tax.hpp>

namespace pgp::content::civic_codec {
template<> struct Extension<core::CorporateTaxPolicy> {
    static void fields(auto& row,auto field) {
        field("rate_bp",row.rate);
        field("payment_delay_days",row.payment_delay);
        field("loss_carry_quarters",row.loss_carry_quarters);
        field("loss_carry_cap_cents",row.loss_carry_cap);
        field("loss_offset_bp",row.loss_offset);
        field("research_credit_bp",row.research_credit);
        field("research_credit_cap_cents",row.research_credit_cap);
        field("research_credit_quarters",row.research_credit_quarters);
    }
};
template<> struct Extension<core::TaxLoss> {
    static constexpr std::size_t maximum_rows=core::tax_loss_limit;
    static void fields(auto& row,auto field) {
        field("originated_on",row.originated_on);
        field("remaining_cents",row.remaining);
    }
};
template<> struct Extension<core::TaxBase> {
    static void fields(auto& row,auto field) {
        field("income_cents",row.income);
        field("deductions_cents",row.deductions);
        field("research_fees_cents",row.research_fees);
    }
};
template<> struct Extension<core::TaxAssessment> {
    static void fields(auto& row,auto field) {
        field("first",row.first);
        field("through",row.through);
        field("due",row.due);
        field("base",row.base);
        field("loss_used_cents",row.loss_used);
        field("loss_added_cents",row.loss_added);
        field("loss_expired_cents",row.loss_expired);
        field("taxable_cents",row.taxable);
        field("gross_tax_cents",row.gross_tax);
        field("credit_cents",row.credit);
        field("bill_cents",row.bill);
    }
};
} // namespace pgp::content::civic_codec
