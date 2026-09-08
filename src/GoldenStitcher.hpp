#pragma once

#include <Geode/Geode.hpp>
#include <filesystem>
#include <string>
#include <vector>

class PlayLayer;
class PlayerObject;
class GJGameLevel;

struct InputEvent {
    int frame = 0;
    int button = 1;
    bool down = false;
};

struct SeamState {
    float x = 0.f;
    float y = 0.f;
    float yAccel = 0.f;
    bool upsideDown = false;
    int frame = 0;
    float percent = 0.f;
};

enum class StitcherStatus {
    Idle,
    WaitingForSeam,
    RecordingSegment,
};

class GoldenStitcher {
public:
    static constexpr float epsX = 1.5f;
    static constexpr float epsY = 1.5f;
    static constexpr float epsYAccel = 0.08f;
    static constexpr float startPosPassEps = 1.f;
    static constexpr float startPosSpawnEps = 40.f;

    static GoldenStitcher& get();

    void beginLevel(GJGameLevel* level);
    void scanStartPositions(PlayLayer* layer);
    void onAttemptStart(PlayLayer* layer);
    void onPostUpdate(PlayLayer* layer);
    void onClick(PlayLayer* layer, bool down, int button, bool player2);
    void onDeath();
    void onComplete(PlayLayer* layer);
    void onQuit();

    bool commitSegment(PlayLayer* layer, bool continueRecording = false);
    void invalidateSegment();
    void clearGoldenRun();

    StitcherStatus status() const;
    char const* statusLabel() const;
    float coveragePercent() const;
    bool hasSeam() const;
    bool canCommit() const;
    std::vector<InputEvent> const& inputs() const;
    SeamState const& seam() const;
    size_t pendingCount() const;
    size_t startPosCount() const;
    std::string nextStartPosLabel() const;

private:
    GoldenStitcher() = default;

    static SeamState captureSeam(PlayLayer* layer, PlayerObject* player, int frame);
    static float computePercent(PlayLayer* layer, PlayerObject* player);
    static float percentAtX(float x, float length);
    bool matchesSeam(PlayerObject* player) const;
    bool startsAtCommittedStartPos(PlayerObject* player) const;
    void snapOriginToNearbyStartPos(float spawnX);
    int globalFrame() const;
    void resetPending();
    void beginRecordingFromSeam();
    void updateNextStartPos();
    bool tryAutoCommit(PlayLayer* layer);
    void persist() const;
    void loadFromDisk();
    std::filesystem::path goldenPath() const;
    matjson::Value toJson() const;

    StitcherStatus m_status = StitcherStatus::Idle;
    bool m_hasSeam = false;
    bool m_committedAtStartPos = false;
    bool m_hasNextStartPos = false;
    int m_localFrame = 0;
    int m_matchLocalFrame = 0;
    int m_frameOffset = 0;
    int m_levelId = 0;
    int m_levelVersion = 0;
    int m_nextStartPosIndex = 0;
    float m_levelLength = 0.f;
    float m_segmentOriginX = 0.f;
    float m_committedStartPosX = 0.f;
    float m_nextStartPosX = 0.f;
    std::string m_levelName;
    SeamState m_seam;
    std::vector<float> m_startPosXs;
    std::vector<InputEvent> m_golden;
    std::vector<InputEvent> m_pending;
};
