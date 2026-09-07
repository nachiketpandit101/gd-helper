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

struct PathSample {
    int frame = 0;
    float percent = 0.f;
    float x = 0.f;
    float y = 0.f;
    std::string gamemode = "cube";
};

struct ClickEvent {
    int frame = 0;
    float percent = 0.f;
    float x = 0.f;
    float y = 0.f;
    bool down = false;
    std::string button = "jump";
    bool player2 = false;
};

struct AttemptRecord {
    int attempt = 0;
    std::string outcome;
    float percent = 0.f;
    float x = 0.f;
    float y = 0.f;
    std::string gamemode = "cube";
    std::optional<KillerInfo> killer;
    std::vector<PathSample> path;
    std::vector<ClickEvent> clicks;
};

class SessionRecorder {
public:
    static constexpr int pathSampleInterval = 8;
    static constexpr char const* recordSettingKey = "record-sessions";

    static SessionRecorder& get();
    static bool isRecordingEnabled();

    void beginSession(GJGameLevel* level);
    void cancelSession();
    void beginAttempt(PlayLayer* layer);
    void onReset(PlayLayer* layer);
    void recordDeath(PlayLayer* layer, PlayerObject* player, GameObject* object);
    void recordComplete(PlayLayer* layer);
    void samplePath(PlayLayer* layer);
    void recordClick(PlayLayer* layer, bool down, int button, bool player2);
    void applyEnabled(bool enabled);
    void endSession();

    bool isActive() const;
    bool isSkipped() const;

private:
    SessionRecorder() = default;

    AttemptRecord capture(PlayLayer* layer, PlayerObject* player);
    void resetAttemptBuffers();
    PathSample makePathSample(PlayLayer* layer, PlayerObject* player) const;
    static float computePercent(PlayLayer* layer, PlayerObject* player);
    matjson::Value toJson() const;
    void persist() const;
    void persistReferenceCopy() const;
    std::string referenceSaveKey() const;
    static std::string gamemodeName(PlayerObject* player);
    static std::string buttonName(int button);
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
    int m_frame = 0;
    std::string m_levelName;
    std::string m_filename;

    std::vector<PathSample> m_path;
    std::vector<ClickEvent> m_clicks;
    std::vector<AttemptRecord> m_attempts;
};
