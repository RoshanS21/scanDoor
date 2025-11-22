#include <gpiod.hpp>

// This test checks if libgpiod v2.x API is available
// In v2.x, gpiod::line is a namespace, and line_request_builder exists
// In v1.x, gpiod::line is a class, and there's no builder

int main() {
    // Try to use v2.x specific API
    gpiod::line_request_builder builder(gpiod::line());
    return 0;
}
