#include "game_events.hpp"

#include <algorithm>
#include <boost/uuid/uuid_io.hpp>

namespace RumpelQuiz {

std::shared_ptr<GameEvents::Subscriber> GameEvents::Subscribe(
    const boost::uuids::uuid& session, bool is_host) {
  auto subscriber = std::make_shared<Subscriber>(is_host);
  std::lock_guard lock(mutex_);
  auto& room = rooms_[boost::uuids::to_string(session)];
  room.erase(std::remove_if(room.begin(), room.end(),
                            [](const auto& weak) { return weak.expired(); }),
             room.end());
  room.emplace_back(subscriber);
  return subscriber;
}

void GameEvents::Publish(const boost::uuids::uuid& session, Audience audience,
                         std::string_view type, std::string_view json) {
  std::vector<std::shared_ptr<Subscriber>> recipients;
  {
    std::lock_guard lock(mutex_);
    auto it = rooms_.find(boost::uuids::to_string(session));
    if (it == rooms_.end()) return;
    auto& room = it->second;
    for (auto weak = room.begin(); weak != room.end();) {
      if (auto subscriber = weak->lock()) {
        if (audience == Audience::kAll ||
            (audience == Audience::kHost) == subscriber->is_host)
          recipients.push_back(std::move(subscriber));
        ++weak;
      } else {
        weak = room.erase(weak);
      }
    }
    if (room.empty()) rooms_.erase(it);
  }
  const std::string message = "event: " + std::string(type) + "\ndata: " +
                              std::string(json) + "\n\n";
  for (const auto& subscriber : recipients) {
    {
      std::lock_guard lock(subscriber->mutex);
      if (type == "resync" && !subscriber->pending.empty() &&
          subscriber->pending.back() == message)
        continue;
      if (subscriber->pending.size() >= 16) {
        subscriber->pending.clear();
        subscriber->pending.emplace_back("event: resync\ndata: {}\n\n");
      } else {
        subscriber->pending.push_back(message);
      }
    }
    subscriber->wake.Send();
  }
}

std::string GameEvents::Pop(const std::shared_ptr<Subscriber>& subscriber) {
  std::lock_guard lock(subscriber->mutex);
  if (subscriber->pending.empty()) return {};
  auto message = std::move(subscriber->pending.front());
  subscriber->pending.pop_front();
  return message;
}

}  // namespace RumpelQuiz
