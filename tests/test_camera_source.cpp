#include <iostream>

#include "vision/camera/frame_source.hpp"

namespace {

bool expect_true(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

class MockCameraFrameSource final : public arx::vision::camera::CameraFrameSource {
public:
    bool open() override {
        opened_ = true;
        return true;
    }

    bool read(arx::vision::camera::CameraFrame& frame) override {
        ++attempts_;
        if (attempts_ == 1) {
            last_error_ = "decode failure";
            healthy_ = false;
            return false;
        }
        frame.frame_id = 2;
        frame.timestamp_ns = 200;
        frame.width = 640;
        frame.height = 480;
        frame.source_id = "mock";
        healthy_ = true;
        return true;
    }

    void close() override {
        opened_ = false;
        healthy_ = false;
    }

    bool healthy() const noexcept override { return healthy_; }
    const std::string& last_error() const noexcept override { return last_error_; }
    const char* name() const noexcept override { return "mock-camera"; }

private:
    int attempts_{0};
    bool opened_{false};
    bool healthy_{false};
    std::string last_error_;
};

}

int main() {
    bool ok = true;
    MockCameraFrameSource source;
    ok &= expect_true(source.open(), "Mock camera source should open");
    arx::vision::camera::CameraFrame frame;
    ok &= expect_true(!source.read(frame), "Mock camera source should simulate first read failure");
    ok &= expect_true(!source.healthy(), "Mock camera source should report unhealthy state after failure");
    ok &= expect_true(source.read(frame), "Mock camera source should recover on second read");
    ok &= expect_true(source.healthy(), "Mock camera source should report healthy state after recovery");
    ok &= expect_true(frame.frame_id == 2, "Mock camera source should preserve frame sequence after reconnect");
    return ok ? 0 : 1;
}
