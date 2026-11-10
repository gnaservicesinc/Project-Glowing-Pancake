#pragma once
#include <pgp/core/personal_life.hpp>
#include <type_traits>

namespace pgp::content::personal_life_codec {
template<class T,class F> void fields(T& row,F f) {
    using V=std::remove_cvref_t<T>;
    if constexpr(std::is_same_v<V,core::PersonalLifeCatalog>) {
        f("properties",row.properties);f("decor",row.decor);
        f("events",row.events,true);
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
        f("events",row.events);f("pending_events",row.pending_events);
    } else if constexpr(std::is_same_v<V,core::PersonalProperty>) {
        f("id",row.id);f("offer",row.offer);f("purchased_on",row.purchased_on);f("next_upkeep",row.next_upkeep);f("sold_on",row.sold_on);
    } else if constexpr(std::is_same_v<V,core::PersonalDecor>) {
        f("id",row.id);f("offer",row.offer);f("purchased_on",row.purchased_on);f("delivered_on",row.delivered_on);
        f("property",row.property);f("x",row.x);f("y",row.y);f("rotated",row.rotated);
        f("sold_on",row.sold_on);
    } else if constexpr(std::is_same_v<V,core::PersonalBill>) {
        f("id",row.id);f("property",row.property);f("charged_on",row.charged_on);f("amount_cents",row.amount);f("paid_on",row.paid_on);
    } else if constexpr(std::is_same_v<V,core::PersonalEventRules>) {
        f("offers",row.offers);f("interval_days",row.interval);f("maximum_notice_days",row.maximum_notice);
    } else if constexpr(std::is_same_v<V,core::PersonalEventOffer>) {
        f("id",row.id);f("name",row.name);f("description",row.description);f("venue",row.venue);
        f("booking_cents",row.booking);f("per_guest_cents",row.per_guest);f("guests",row.guests);f("mood",row.mood);
        f("notice_days",row.notice);f("duration_days",row.duration);f("cancellation_refund_bp",row.cancellation_refund);
    } else if constexpr(std::is_same_v<V,core::PersonalEvent>) {
        f("id",row.id);f("property",row.property);f("premises",row.premises);f("offer",row.offer);f("outcome",row.outcome);
        f("booked_on",row.booked_on);f("day",row.day);f("paid_cents",row.paid);f("refund_cents",row.refund);f("resolved_on",row.resolved_on);f("guests",row.guests);
    } else if constexpr(std::is_same_v<V,core::PersonalEventGuest>) {
        f("person",row.person);f("reply",row.reply);f("attended",row.attended);
    }
}
}
