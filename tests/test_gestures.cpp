#include <iostream>

#include "engine/events/event_bus.hpp"
#include "engine/events/gesture_events.hpp"
#include "vision/gesture_engine/gesture_classifier.hpp"
#include "vision/gesture_engine/gesture_engine.hpp"

namespace {

bool expect_true(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

arx::vision::FrameLandmarks pinch_frame() {
    arx::vision::FrameLandmarks frame;
    arx::vision::HandLandmarks hand;
    hand.points[0] = {0.50f, 0.72f, 0.0f};
    hand.points[4] = {0.47f, 0.38f, 0.0f};
    hand.points[5] = {0.49f, 0.58f, 0.0f};
    hand.points[6] = {0.50f, 0.46f, 0.0f};
    hand.points[8] = {0.48f, 0.39f, 0.0f};
    hand.points[9] = {0.54f, 0.60f, 0.0f};
    hand.points[10] = {0.54f, 0.48f, 0.0f};
    hand.points[12] = {0.54f, 0.30f, 0.0f};
    hand.points[14] = {0.58f, 0.52f, 0.0f};
    hand.points[16] = {0.58f, 0.38f, 0.0f};
    hand.points[18] = {0.62f, 0.55f, 0.0f};
    hand.points[20] = {0.63f, 0.43f, 0.0f};
    frame.hands.push_back(hand);
    return frame;
}

}

int main() {
    using namespace arx::vision;
    using namespace arx::engine;

    GestureClassifier classifier;
    auto pinch = classifier.classify(pinch_frame());

    EventBus bus;
    int events = 0;
    bus.subscribe<GestureRuntimeEvent>([&](const GestureRuntimeEvent&) { ++events; });
    GestureEngine engine(&bus);

    auto out1 = engine.process(pinch_frame(), 0.0);
    auto out2 = engine.process(pinch_frame(), 0.5);
    auto swipe = classifier.classify(pinch_frame(), VelocityFrame{2.0f, 0.1f, 2.1f});

    bool ok = true;
    ok &= expect_true(pinch.gesture == GestureType::kPinch, "Pinch frame should classify as pinch");
    ok &= expect_true(out1.event.has_value(), "First valid gesture should emit an event");
    ok &= expect_true(out2.event.has_value(), "Held gesture should emit a hold event");
    ok &= expect_true(events >= 2, "Gesture engine should publish events to the event bus");
    ok &= expect_true(swipe.gesture == GestureType::kSwipeRight, "High horizontal velocity should classify as swipe right");

    return ok ? 0 : 1;
}
