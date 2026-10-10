#include "doctest.h"
#include "engine/core/parse.hpp"

using namespace engine;

#define OK(x) CHECK(static_cast<bool>(x))
#define BAD(x, c) do { auto r_ = (x); CHECK(!r_); if (!r_) CHECK(r_.error().code == (c)); } while (0)

TEST_CASE("parse_eye basics") {
    auto r = parse_eye(R"({"mood":"happy","look":[0.5,-1],"blink":"left","pupil":0.5,"brightness":40,"transition_ms":300})");
    OK(r); if (!r) return;
    CHECK(*r->mood == eyes::Mood::Happy);
    CHECK((*r->look_q8)[0] == 128); CHECK((*r->look_q8)[1] == -256);
    CHECK(*r->blink == EyeSel::Left);
    CHECK(*r->pupil == 128); CHECK(*r->brightness == 40); CHECK(*r->transition_ms == 300);
}
TEST_CASE("parse_eye blink forms") {
    auto a = parse_eye(R"({"blink":true})"); OK(a); if (a) CHECK(*a->blink == EyeSel::Both);
    auto b = parse_eye(R"({"blink":false})"); OK(b); if (b) CHECK(!b->blink);
    BAD(parse_eye(R"({"blink":"up"})"), Err::BadJson);
}
TEST_CASE("parse_eye lids and style") {
    auto r = parse_eye(R"({"lids":{"top":1,"tilt":-1},"style":{"preset":"robo","iris":{"on":true,"r":40,"color":"#ff8800"},"pupil":{"shape":"slit","slit_w":6}},"save_style":true})");
    OK(r); if (!r) return;
    CHECK(*r->lids->top == 255); CHECK(*r->lids->tilt == -127); CHECK(!r->lids->bottom);
    CHECK(*r->style->preset == 1); CHECK(*r->style->iris_r == 40);
    CHECK(*r->style->iris_color == gfx::rgb565(0xFF8800));
    CHECK(*r->style->pupil_shape == eyes::PupilShape::Slit);
    CHECK(r->save_style);
}
TEST_CASE("parse_eye rejects bad input") {
    BAD(parse_eye("not json"), Err::BadJson);
    BAD(parse_eye("[]"), Err::BadJson);
    BAD(parse_eye(R"({"foo":1})"), Err::BadJson);
    BAD(parse_eye(R"({"mood":"furious"})"), Err::BadJson);
    BAD(parse_eye(R"({"look":[1]})"), Err::BadJson);
    BAD(parse_eye(R"({"look":[2,0]})"), Err::BadJson);
    BAD(parse_eye(R"({"pupil":0.1})"), Err::BadJson);
    BAD(parse_eye(R"({"brightness":101})"), Err::BadJson);
    BAD(parse_eye(R"({"brightness":1.5})"), Err::BadJson);
    BAD(parse_eye(R"({"transition_ms":6000})"), Err::BadJson);
    BAD(parse_eye(R"({"style":{"bg":"red"}})"), Err::BadJson);
    BAD(parse_eye(R"({"style":{"sclera":{"w":5}}})"), Err::BadJson);
    BAD(parse_eye(R"({"style":{"zzz":1}})"), Err::BadJson);
    BAD(parse_eye(R"({"lids":{"top":2}})"), Err::BadJson);
    BAD(parse_eye(std::string(2000, ' ')), Err::TooLarge);
}
TEST_CASE("parse_eye error message") {
    auto r = parse_eye(R"({"look":[1]})");
    CHECK(!r);
    if (!r) CHECK(r.error().message.view() == "look must have 2 items");
}
TEST_CASE("parse_play") {
    auto a = parse_play(R"({"clip":"wink"})"); OK(a);
    if (a) { CHECK(a->clip[0].view() == "wink"); CHECK(a->clip[1].view() == "wink"); CHECK(a->then_eyes); CHECK(!a->loop); }
    auto b = parse_play(R"({"left":"a","loop":true,"then":"hold"})"); OK(b);
    if (b) { CHECK(b->clip[0].view() == "a"); CHECK(b->clip[1].empty()); CHECK(b->loop); CHECK(!b->then_eyes); }
    BAD(parse_play(R"({})"), Err::BadJson);
    BAD(parse_play(R"({"clip":"A B"})"), Err::BadName);
    BAD(parse_play(R"({"clip":"a","left":"b"})"), Err::BadJson);
    BAD(parse_play(R"({"clip":"a","then":"x"})"), Err::BadJson);
    BAD(parse_play(R"({"clip":3})"), Err::BadJson);
}
TEST_CASE("clip_name_valid") {
    CHECK(clip_name_valid("a-b_9")); CHECK(!clip_name_valid("")); CHECK(!clip_name_valid("A"));
    CHECK(!clip_name_valid("../x")); CHECK(!clip_name_valid(std::string(32, 'a'))); CHECK(clip_name_valid(std::string(31, 'a')));
}

#include "engine/core/errors.hpp"
TEST_CASE("errors and auth helpers") {
    CHECK(describe(Err::Conflict).http == 409);
    CHECK(describe(Err::BadJson).code == "E_BAD_JSON");
    static_assert(ct_equal("abc", "abc") && !ct_equal("abc", "abd") && !ct_equal("abc", "ab"));
    CHECK(bearer_token("Bearer xyz") == "xyz");
    CHECK(bearer_token("Basic xyz").empty());
    CHECK(bearer_token("Bearer ").empty());
}
