#pragma once

#include <chrono>
#include <algorithm>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace RumpelQuiz {

// Short leases detect a lost network even if the SSE socket stays open.
// All tabs of one participant are aggregated into a single presence status.
class GamePresence {
 public:
  using Clock = std::chrono::steady_clock;
  struct Status {
    std::string user_id;
    bool online;
  };

  void Update(const std::string& session, const std::string& user,
              const std::string& client, std::int64_t sequence, bool online,
              Clock::time_point now = Clock::now()) {
    std::lock_guard lock(mutex_);
    // Bound abandoned rooms without background tasks or persistent history.
    for (auto it = rooms_.begin(); it != rooms_.end();) {
      if (now - it->second.updated > std::chrono::hours{24})
        it = rooms_.erase(it);
      else
        ++it;
    }
    auto& room = rooms_[session];
    room.updated = now;
    auto& clients = room.users[user];
    auto found = clients.find(client);
    if (found != clients.end() && sequence <= found->second.sequence) return;
    // Keep recent expired entries as tombstones against reordered requests.
    for (auto it = clients.begin(); it != clients.end();) {
      if (now - it->second.deadline > std::chrono::minutes{5})
        it = clients.erase(it);
      else
        ++it;
    }
    if (!clients.contains(client) && clients.size() >= 32) return;
    auto& lease = clients[client];
    lease.sequence = sequence;
    // A brief grace period hides reloads and token renewal from the teacher.
    lease.deadline = online ? now + std::chrono::seconds{35}
        : std::min(lease.deadline, now + std::chrono::seconds{8});
  }

  std::vector<Status> Read(const std::string& session,
                         Clock::time_point now = Clock::now()) const {
    std::lock_guard lock(mutex_);
    std::vector<Status> result;
    const auto room = rooms_.find(session);
    if (room == rooms_.end()) return result;
    for (const auto& [user, clients] : room->second.users) {
      bool online = false;
      for (const auto& [id, lease] : clients)
        online = online || lease.deadline > now;
      result.push_back({user, online});
    }
    return result;
  }

  void Clear(const std::string& session) {
    std::lock_guard lock(mutex_);
    rooms_.erase(session);
  }

 private:
  struct Lease {
    std::int64_t sequence = -1;
    Clock::time_point deadline{};
  };
  struct Room {
    Clock::time_point updated{};
    std::map<std::string, std::map<std::string, Lease>> users;
  };
  mutable std::mutex mutex_;
  std::map<std::string, Room> rooms_;
};

}  // namespace RumpelQuiz
