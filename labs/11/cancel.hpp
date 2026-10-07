#pragma once
#include <stdexcept>
#include <stop_token>
struct cancelled : std::runtime_error { cancelled() : std::runtime_error("cancelled") {} };
inline void check_stop(std::stop_token token) { if (token.stop_requested()) throw cancelled{}; }
