#include <catch2/catch_test_macros.hpp>

#include "../src/paper_oms.hpp"
#include "../src/order_types.hpp"

namespace {

OrderIntent make_intent(
    uint64_t intent_id = 1,
    uint32_t market_id = 42,
    TradeSide side = TradeSide::Buy,
    uint32_t price = 5200,
    uint32_t quantity = 100,
    TimeInForce tif = TimeInForce::IOC,
    uint64_t timestamp_ns = 1000
) {
    OrderIntent intent{};
    intent.intent_id = intent_id;
    intent.created_timestamp_ns = timestamp_ns;
    intent.internal_market_id = market_id;
    intent.side = side;
    intent.price = price;
    intent.quantity = quantity;
    intent.time_in_force = tif;
    return intent;
}

} // namespace

TEST_CASE("PaperOMS submits valid order", "[paper_oms]") {
    PaperOMS oms;

    OrderIntent intent = make_intent();

    SubmitResult result = oms.submit_order(intent);

    REQUIRE(result.status == SubmitStatus::Accepted);
    REQUIRE(result.order_id == 1);
    REQUIRE(oms.order_count() == 1);
    REQUIRE(oms.next_order_id() == 2);

    const PaperOrder* order = oms.get_order(result.order_id);
    REQUIRE(order != nullptr);

    REQUIRE(order->order_id == 1);
    REQUIRE(order->intent_id == intent.intent_id);
    REQUIRE(order->created_timestamp_ns == intent.created_timestamp_ns);
    REQUIRE(order->updated_timestamp_ns == intent.created_timestamp_ns);

    REQUIRE(order->price == intent.price);
    REQUIRE(order->original_quantity == intent.quantity);
    REQUIRE(order->remaining_quantity == intent.quantity);
    REQUIRE(order->filled_quantity == 0);

    REQUIRE(order->side == intent.side);
    REQUIRE(order->time_in_force == intent.time_in_force);
    REQUIRE(order->status == OrderStatus::Accepted);
}

TEST_CASE("PaperOMS rejects zero quantity order", "[paper_oms]") {
    PaperOMS oms;

    OrderIntent intent = make_intent();
    intent.quantity = 0;

    SubmitResult result = oms.submit_order(intent);

    REQUIRE(result.status == SubmitStatus::InvalidQuantity);
    REQUIRE(result.order_id == 0);
    REQUIRE(oms.order_count() == 0);
    REQUIRE(oms.next_order_id() == 1);
}

TEST_CASE("PaperOMS assigns increasing order ids", "[paper_oms]") {
    PaperOMS oms;

    SubmitResult first = oms.submit_order(make_intent(1));
    SubmitResult second = oms.submit_order(make_intent(2));
    SubmitResult third = oms.submit_order(make_intent(3));

    REQUIRE(first.status == SubmitStatus::Accepted);
    REQUIRE(second.status == SubmitStatus::Accepted);
    REQUIRE(third.status == SubmitStatus::Accepted);

    REQUIRE(first.order_id == 1);
    REQUIRE(second.order_id == 2);
    REQUIRE(third.order_id == 3);

    REQUIRE(oms.order_count() == 3);
    REQUIRE(oms.next_order_id() == 4);
}

TEST_CASE("PaperOMS rejects submit when capacity is exceeded", "[paper_oms]") {
    PaperOMS oms;

    for (uint32_t i = 0; i < MAX_PAPER_ORDERS; ++i) {
        SubmitResult result = oms.submit_order(make_intent(i + 1));
        REQUIRE(result.status == SubmitStatus::Accepted);
        REQUIRE(result.order_id == i + 1);
    }

    REQUIRE(oms.order_count() == MAX_PAPER_ORDERS);

    SubmitResult overflow = oms.submit_order(make_intent(999999));

    REQUIRE(overflow.status == SubmitStatus::CapacityExceeded);
    REQUIRE(overflow.order_id == 0);
    REQUIRE(oms.order_count() == MAX_PAPER_ORDERS);
}

TEST_CASE("PaperOMS cancels accepted order", "[paper_oms]") {
    PaperOMS oms;

    SubmitResult submit = oms.submit_order(make_intent());
    REQUIRE(submit.status == SubmitStatus::Accepted);

    CancelStatus cancel = oms.cancel_order(submit.order_id, 2000);

    REQUIRE(cancel == CancelStatus::Cancelled);

    const PaperOrder* order = oms.get_order(submit.order_id);
    REQUIRE(order != nullptr);

    REQUIRE(order->status == OrderStatus::Cancelled);
    REQUIRE(order->updated_timestamp_ns == 2000);
    REQUIRE(order->remaining_quantity == order->original_quantity);
    REQUIRE(order->filled_quantity == 0);
}

TEST_CASE("PaperOMS cancel returns NotFound for unknown order", "[paper_oms]") {
    PaperOMS oms;

    CancelStatus cancel = oms.cancel_order(999, 2000);

    REQUIRE(cancel == CancelStatus::NotFound);
}

TEST_CASE("PaperOMS cannot cancel filled order", "[paper_oms]") {
    PaperOMS oms;

    SubmitResult submit = oms.submit_order(make_intent());
    REQUIRE(submit.status == SubmitStatus::Accepted);

    FillStatus fill = oms.apply_fill(submit.order_id, 100, 2000);
    REQUIRE(fill == FillStatus::Filled);

    CancelStatus cancel = oms.cancel_order(submit.order_id, 3000);

    REQUIRE(cancel == CancelStatus::AlreadyTerminal);

    const PaperOrder* order = oms.get_order(submit.order_id);
    REQUIRE(order != nullptr);
    REQUIRE(order->status == OrderStatus::Filled);
}

TEST_CASE("PaperOMS cannot cancel cancelled order twice", "[paper_oms]") {
    PaperOMS oms;

    SubmitResult submit = oms.submit_order(make_intent());
    REQUIRE(submit.status == SubmitStatus::Accepted);

    CancelStatus first = oms.cancel_order(submit.order_id, 2000);
    CancelStatus second = oms.cancel_order(submit.order_id, 3000);

    REQUIRE(first == CancelStatus::Cancelled);
    REQUIRE(second == CancelStatus::AlreadyTerminal);

    const PaperOrder* order = oms.get_order(submit.order_id);
    REQUIRE(order != nullptr);
    REQUIRE(order->status == OrderStatus::Cancelled);
    REQUIRE(order->updated_timestamp_ns == 2000);
}

TEST_CASE("PaperOMS applies partial fill", "[paper_oms]") {
    PaperOMS oms;

    SubmitResult submit = oms.submit_order(make_intent());
    REQUIRE(submit.status == SubmitStatus::Accepted);

    FillStatus fill = oms.apply_fill(submit.order_id, 40, 2000);

    REQUIRE(fill == FillStatus::PartiallyFilled);

    const PaperOrder* order = oms.get_order(submit.order_id);
    REQUIRE(order != nullptr);

    REQUIRE(order->status == OrderStatus::PartiallyFilled);
    REQUIRE(order->original_quantity == 100);
    REQUIRE(order->filled_quantity == 40);
    REQUIRE(order->remaining_quantity == 60);
    REQUIRE(order->updated_timestamp_ns == 2000);
}

TEST_CASE("PaperOMS applies full fill", "[paper_oms]") {
    PaperOMS oms;

    SubmitResult submit = oms.submit_order(make_intent());
    REQUIRE(submit.status == SubmitStatus::Accepted);

    FillStatus fill = oms.apply_fill(submit.order_id, 100, 2000);

    REQUIRE(fill == FillStatus::Filled);

    const PaperOrder* order = oms.get_order(submit.order_id);
    REQUIRE(order != nullptr);

    REQUIRE(order->status == OrderStatus::Filled);
    REQUIRE(order->original_quantity == 100);
    REQUIRE(order->filled_quantity == 100);
    REQUIRE(order->remaining_quantity == 0);
    REQUIRE(order->updated_timestamp_ns == 2000);
}

TEST_CASE("PaperOMS transitions partial fill to full fill", "[paper_oms]") {
    PaperOMS oms;

    SubmitResult submit = oms.submit_order(make_intent());
    REQUIRE(submit.status == SubmitStatus::Accepted);

    FillStatus first = oms.apply_fill(submit.order_id, 40, 2000);
    FillStatus second = oms.apply_fill(submit.order_id, 60, 3000);

    REQUIRE(first == FillStatus::PartiallyFilled);
    REQUIRE(second == FillStatus::Filled);

    const PaperOrder* order = oms.get_order(submit.order_id);
    REQUIRE(order != nullptr);

    REQUIRE(order->status == OrderStatus::Filled);
    REQUIRE(order->filled_quantity == 100);
    REQUIRE(order->remaining_quantity == 0);
    REQUIRE(order->updated_timestamp_ns == 3000);
}

TEST_CASE("PaperOMS rejects zero fill quantity", "[paper_oms]") {
    PaperOMS oms;

    SubmitResult submit = oms.submit_order(make_intent());
    REQUIRE(submit.status == SubmitStatus::Accepted);

    FillStatus fill = oms.apply_fill(submit.order_id, 0, 2000);

    REQUIRE(fill == FillStatus::InvalidFillQuantity);

    const PaperOrder* order = oms.get_order(submit.order_id);
    REQUIRE(order != nullptr);

    REQUIRE(order->status == OrderStatus::Accepted);
    REQUIRE(order->filled_quantity == 0);
    REQUIRE(order->remaining_quantity == 100);
}

TEST_CASE("PaperOMS rejects overfill", "[paper_oms]") {
    PaperOMS oms;

    SubmitResult submit = oms.submit_order(make_intent());
    REQUIRE(submit.status == SubmitStatus::Accepted);

    FillStatus fill = oms.apply_fill(submit.order_id, 101, 2000);

    REQUIRE(fill == FillStatus::FillQuantityTooLarge);

    const PaperOrder* order = oms.get_order(submit.order_id);
    REQUIRE(order != nullptr);

    REQUIRE(order->status == OrderStatus::Accepted);
    REQUIRE(order->filled_quantity == 0);
    REQUIRE(order->remaining_quantity == 100);
}

TEST_CASE("PaperOMS fill returns NotFound for unknown order", "[paper_oms]") {
    PaperOMS oms;

    FillStatus fill = oms.apply_fill(999, 10, 2000);

    REQUIRE(fill == FillStatus::NotFound);
}

TEST_CASE("PaperOMS cannot fill cancelled order", "[paper_oms]") {
    PaperOMS oms;

    SubmitResult submit = oms.submit_order(make_intent());
    REQUIRE(submit.status == SubmitStatus::Accepted);

    CancelStatus cancel = oms.cancel_order(submit.order_id, 2000);
    REQUIRE(cancel == CancelStatus::Cancelled);

    FillStatus fill = oms.apply_fill(submit.order_id, 10, 3000);

    REQUIRE(fill == FillStatus::AlreadyTerminal);

    const PaperOrder* order = oms.get_order(submit.order_id);
    REQUIRE(order != nullptr);

    REQUIRE(order->status == OrderStatus::Cancelled);
    REQUIRE(order->filled_quantity == 0);
    REQUIRE(order->remaining_quantity == 100);
}

TEST_CASE("PaperOMS cannot fill already filled order", "[paper_oms]") {
    PaperOMS oms;

    SubmitResult submit = oms.submit_order(make_intent());
    REQUIRE(submit.status == SubmitStatus::Accepted);

    FillStatus first = oms.apply_fill(submit.order_id, 100, 2000);
    FillStatus second = oms.apply_fill(submit.order_id, 1, 3000);

    REQUIRE(first == FillStatus::Filled);
    REQUIRE(second == FillStatus::AlreadyTerminal);

    const PaperOrder* order = oms.get_order(submit.order_id);
    REQUIRE(order != nullptr);

    REQUIRE(order->status == OrderStatus::Filled);
    REQUIRE(order->filled_quantity == 100);
    REQUIRE(order->remaining_quantity == 0);
}

TEST_CASE("PaperOMS get_order returns nullptr for missing order", "[paper_oms]") {
    PaperOMS oms;

    const PaperOrder* order = oms.get_order(12345);

    REQUIRE(order == nullptr);
}

TEST_CASE("PaperOMS reset clears active orders", "[paper_oms]") {
    PaperOMS oms;

    OrderIntent intent{};
    intent.intent_id = 1;
    intent.internal_market_id = 0;
    intent.price = 5000;
    intent.quantity = 10;
    intent.side = TradeSide::Buy;

    SubmitResult result = oms.submit_order(intent);
    REQUIRE(result.status == SubmitStatus::Accepted);
    REQUIRE(oms.order_count() == 1);

    oms.reset();

    REQUIRE(oms.order_count() == 0);

    SubmitResult next = oms.submit_order(intent);
    REQUIRE(next.status == SubmitStatus::Accepted);
    REQUIRE(next.order_id == 1);
}