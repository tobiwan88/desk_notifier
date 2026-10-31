#include <string>
#include <vector>

#include "ble/core/provision.hpp"
#include "doctest.h"
#include "screen/core/pair_view.hpp"

using namespace ble;

static_assert(passkey_from(0) == 100000 && passkey_from(899999) == 999999 && passkey_from(900000) == 100000);

TEST_CASE("config parses the wifi, tz and hostname keys") {
    Config c;
    CHECK_EQ(parse_config(R"({"ssid":"Net","pass":"secret123","tz":"CET-1CEST,M3.5.0,M10.5.0/3","hostname":"Desk-1"})", c),
               ParseError::None);
    CHECK(*c.ssid == "Net");
    CHECK(*c.pass == "secret123");
    CHECK(*c.tz == "CET-1CEST,M3.5.0,M10.5.0/3");
    CHECK(*c.hostname == "desk-1");  // folded to lower case
    CHECK_FALSE(c.reboot);
}

TEST_CASE("config handles escapes, unicode and the reboot flag") {
    Config c;
    CHECK_EQ(parse_config(R"( { "ssid" : "é\"1" , "pass" : "a\\b\/c12345" , "reboot" : true } )", c), ParseError::None);
    CHECK(*c.ssid == "\xC3\xA9\"1");
    CHECK(*c.pass == "a\\b/c12345");
    CHECK(c.reboot);
    CHECK_EQ(parse_config(R"({"ssid":"😀"})", c), ParseError::None);
    CHECK(*c.ssid == "\xF0\x9F\x98\x80");
    CHECK_EQ(parse_config(R"({"pass":"\ud83d"})", c), ParseError::Json);  // lone surrogate
    CHECK_EQ(parse_config(R"({"pass":"a\u0000b"})", c), ParseError::Json);
    CHECK_EQ(parse_config(R"({"reboot":true})", c), ParseError::None);  // reboot alone is a valid command
    CHECK(c.reboot);
}

TEST_CASE("config rejects bad input") {
    Config c;
    CHECK_EQ(parse_config("", c), ParseError::Json);
    CHECK_EQ(parse_config("{", c), ParseError::Json);
    CHECK_EQ(parse_config(R"({"ssid":"a"} x)", c), ParseError::Json);
    CHECK_EQ(parse_config(R"({"ssid":"a",})", c), ParseError::Json);
    CHECK_EQ(parse_config(R"({"ssid":5})", c), ParseError::Json);
    CHECK_EQ(parse_config("{}", c), ParseError::Empty);
    CHECK_EQ(parse_config(R"({"ssid":""})", c), ParseError::Ssid);
    CHECK_EQ(parse_config(R"({"ssid":")" + std::string(33, 'x') + R"("})", c), ParseError::Ssid);
    CHECK_EQ(parse_config(R"({"pass":"longenough"})", c), ParseError::Ssid);  // password without a network
    CHECK_EQ(parse_config(R"({"ssid":"a","pass":"short"})", c), ParseError::Pass);
    CHECK_EQ(parse_config(R"({"ssid":"a","pass":")" + std::string(64, 'x') + R"("})", c), ParseError::Pass);
    CHECK_EQ(parse_config(R"({"tz":"bad;tz"})", c), ParseError::Tz);
    CHECK_EQ(parse_config(R"({"hostname":"-x"})", c), ParseError::Hostname);
    CHECK_EQ(parse_config(R"({"hostname":"a_b"})", c), ParseError::Hostname);
    CHECK_EQ(parse_config(std::string(600, ' ') + "{}", c), ParseError::Json);  // too long
}

TEST_CASE("an open network has an empty password") {
    Config c;
    CHECK_EQ(parse_config(R"({"ssid":"Net","pass":""})", c), ParseError::None);
    CHECK(c.pass->empty());
}

TEST_CASE("status json escapes strings") {
    Status s;
    s.version = "abc";
    s.wifi = "connected";
    s.ssid = "m\"n\n";
    s.ip = "203.0.113.5";
    s.hostname = "desk";
    s.tz = "UTC0";
    s.result = "ok";
    s.time_valid = true;
    s.rssi = -55;
    CHECK(status_json(s) ==
          R"({"v":"abc","wifi":"connected","ssid":"m\"n\u000a","ip":"203.0.113.5","host":"desk","tz":"UTC0","result":"ok","time":true,"rssi":-55})");
}

TEST_CASE("scan list is sorted, unique, without hidden networks and short enough") {
    std::vector<Network> n = {{"wk", -80, true}, {"str", -40, false}, {"", -30, true}, {"wk", -70, true}};
    CHECK(scan_json(n) == R"([{"ssid":"str","rssi":-40,"sec":false},{"ssid":"wk","rssi":-70,"sec":true}])");
    std::vector<Network> many;
    for (int i = 0; i < 40; ++i) many.push_back({"network-number-" + std::to_string(i), -30 - i, true});
    const std::string j = scan_json(many);
    CHECK(j.size() <= 480);
    CHECK(j.front() == '[');
    CHECK(j.back() == ']');
    CHECK(j.find("network-number-0\"") != std::string::npos);  // the strongest are kept
    CHECK(scan_json({}) == "[]");
}

TEST_CASE("pair guard locks after five failures and unlocks later") {
    PairGuard g;
    for (int i = 0; i < 4; ++i) g.failed(1000);
    CHECK_FALSE(g.locked(1000));
    g.failed(1000);
    CHECK(g.locked(1000));
    CHECK(g.locked(1000 + PairGuard::kLockMs - 1));
    CHECK_FALSE(g.locked(1000 + PairGuard::kLockMs));
    g.failed(1000 + PairGuard::kLockMs);  // counting starts over after the lock
    CHECK_FALSE(g.locked(1000 + PairGuard::kLockMs));
    g.succeeded();
    for (int i = 0; i < 4; ++i) g.failed(5);
    CHECK_FALSE(g.locked(5));
}

TEST_CASE("pair guard copes with the millisecond counter wrapping") {
    PairGuard g;
    const std::uint32_t t = 0xFFFFFFFFu - 1000u;
    for (int i = 0; i < 5; ++i) g.failed(t);
    CHECK(g.locked(t + 10));
    CHECK(g.locked(5u));  // wrapped, still inside the five minutes
    CHECK_FALSE(g.locked(t + PairGuard::kLockMs + 1));
}

TEST_CASE("pair code overlay draws the digits") {
    std::vector<gfx::Rgb565be> px(gfx::kScreen * gfx::kScreen, gfx::Rgb565be{0});
    screen::render_pair_code(screen::Canvas{px}, "123456");
    bool lit = false;
    for (auto p : px) lit = lit || p != 0;
    CHECK(lit);
}
