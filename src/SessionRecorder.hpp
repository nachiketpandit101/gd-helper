#pragma once

#include <Geode/Geode.hpp>
#include <optional>
#include <string>
#include <vector>

class PlayLayer;
class PlayerObject;
class GameObject;
class GJGameLevel;

struct KillerInfo {
    int objectId = 0;
    std::string type = "unknown";
};

struct AttemptRecord {
    int attempt = 0;
    std::string outcome;
    float percent = 0.f;
    float x = 0.f;
    float y = 0.f;
    std::string gamemode = "cube";
    std::optional<KillerInfo> killer;
};

class SessionRecorder {
public:
    static SessionRecorder& get();

    void beginSession(GJGameLevel* level);
    void cancelSession();
    void beginAttempt(PlayLayer* layer);
    void onReset(PlayLayer* layer);
    void recordDeath(PlayLayer* layer, PlayerObject* player, GameObject* object);
    void recordComplete(PlayLayer* layer);
    void endSession();

    bool isActive() const;
    bool isSkipped() const;

private:
    SessionRecorder() = default;

    AttemptRecord capture(PlayLayer* layer, PlayerObject* player) const;
    static float computePercent(PlayLayer* layer, PlayerObject* player);
    matjson::Value toJson() const;
    void persist() const;
    void persistReferenceCopy() const;
    std::string referenceSaveKey() const;
    static std::string gamemodeName(PlayerObject* player);
    static KillerInfo killerFrom(GameObject* object);

    bool m_active = false;
    bool m_skipped = false;
    bool m_attemptOpen = false;
    bool m_reference = false;
    bool m_practice = false;
    bool m_platformer = false;
    bool m_startPos = false;

    int m_levelId = 0;
    int m_levelVersion = 0;
    int m_levelLength = 0;
    int m_attemptNumber = 0;
    std::string m_levelName;
    std::string m_filename;

    std::vector<AttemptRecord> m_attempts;
};
