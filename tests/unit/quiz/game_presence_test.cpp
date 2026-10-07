#include <userver/utest/utest.hpp>
#include "game/service/game_presence.hpp"

using RumpelQuiz::GamePresence;
using namespace std::chrono_literals;

UTEST(GamePresence, ReloadGraceAndMultipleTabs) {
  GamePresence presence;
  const auto now = GamePresence::Clock::now();
  presence.Update("room", "student", "tab1", 1, true, now);
  presence.Update("room", "student", "tab1", 2, false, now + 1s);
  EXPECT_TRUE(presence.Read("room", now + 8s)[0].online);
  EXPECT_FALSE(presence.Read("room", now + 9s)[0].online);
  presence.Update("room", "student", "tab2", 1, true, now + 5s);
  EXPECT_TRUE(presence.Read("room", now + 9s)[0].online);
  EXPECT_FALSE(presence.Read("room", now + 40s)[0].online);
  presence.Update("room", "student", "tab2", 2, true, now + 41s);
  EXPECT_TRUE(presence.Read("room", now + 41s)[0].online);
}

UTEST(GamePresence, LostNetworkAndReorderedRequests) {
  GamePresence presence;
  const auto now = GamePresence::Clock::now();
  presence.Update("room", "student", "tab", 1, true, now);
  EXPECT_TRUE(presence.Read("room", now + 34s)[0].online);
  EXPECT_FALSE(presence.Read("room", now + 35s)[0].online);
  presence.Update("room", "student", "tab", 4, false, now + 36s);
  EXPECT_FALSE(presence.Read("room", now + 36s)[0].online);
  presence.Update("room", "student", "tab", 3, true, now + 37s);
  EXPECT_FALSE(presence.Read("room", now + 37s)[0].online);
  presence.Update("room", "student", "tab", 5, true, now + 38s);
  presence.Update("room", "student", "tab", 4, false, now + 39s);
  EXPECT_TRUE(presence.Read("room", now + 60s)[0].online);
  EXPECT_TRUE(presence.Read("other", now).empty());
  presence.Clear("room");
  EXPECT_TRUE(presence.Read("room", now).empty());
}
