#pragma once
#include <pgp/core/personal_life.hpp>
#include <type_traits>

namespace pgp::content::personal_life_codec {
template<class T,class F> void fields(T& row,F f) {
    using V=std::remove_cvref_t<T>;
    if constexpr(std::is_same_v<V,core::PersonalLifeCatalog>) {
        f("properties",row.properties);f("decor",row.decor);
    } else if constexpr(std::is_same_v<V,core::PersonalPropertyOffer>) {
        f("id",row.id);f("name",row.name);f("agent",row.agent);f("description",row.description);f("plan",row.plan);
        f("price_cents",row.price);f("monthly_upkeep_cents",row.monthly_upkeep);f("resale_bp",row.resale);
    } else if constexpr(std::is_same_v<V,core::HQPlan>) {
        f("id",row.id);f("name",row.name);f("width",row.width);f("depth",row.depth);f("rooms",row.rooms);
    } else if constexpr(std::is_same_v<V,core::HQRoom>) {
        f("x",row.x);f("y",row.y);f("width",row.width);f("depth",row.depth);
    } else if constexpr(std::is_same_v<V,core::PersonalDecorOffer>) {
        f("id",row.id);f("name",row.name);f("supplier",row.supplier);f("kind",row.kind);f("price_cents",row.price);
        f("delivery_days",row.delivery);f("width",row.width);f("depth",row.depth);f("resale_bp",row.resale);
    } else if constexpr(std::is_same_v<V,core::PersonalLifeState>) {
        f("properties",row.properties);f("decor",row.decor);f("bills",row.bills);f("spent_cents",row.spent);f("unpaid_cents",row.unpaid);
    } else if constexpr(std::is_same_v<V,core::PersonalProperty>) {
        f("id",row.id);f("offer",row.offer);f("purchased_on",row.purchased_on);f("next_upkeep",row.next_upkeep);f("sold_on",row.sold_on);
    } else if constexpr(std::is_same_v<V,core::PersonalDecor>) {
        f("id",row.id);f("offer",row.offer);f("purchased_on",row.purchased_on);f("delivered_on",row.delivered_on);
        f("property",row.property);f("x",row.x);f("y",row.y);f("rotated",row.rotated);
        f("sold_on",row.sold_on);
    } else if constexpr(std::is_same_v<V,core::PersonalBill>) {
        f("id",row.id);f("property",row.property);f("charged_on",row.charged_on);f("amount_cents",row.amount);f("paid_on",row.paid_on);
    }
}
}
