#include "SessionRecorder.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>

using namespace geode::prelude;

namespace {
    std::string sanitize(std::string_view input) {
        std::string out;
        out.reserve(input.size());
        for (char ch : input) {
            if (std::isalnum(static_cast<unsigned char>(ch))) {
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            }
            else if (ch == ' ' || ch == '-' || ch == '_') {
                if (out.empty() || out.back() != '-') {
                    out.push_back('-');
                }
            }
        }
        if (out.empty()) {
            out = "level";
        }
        return out;
    }

    char const* objectTypeName(GameObjectType type) {
        switch (type) {
            case GameObjectType::Solid: return "Solid";
            case GameObjectType::Hazard: return "Hazard";
            case GameObjectType::InverseGravityPortal: return "InverseGravityPortal";
            case GameObjectType::NormalGravityPortal: return "NormalGravityPortal";
            case GameObjectType::ShipPortal: return "ShipPortal";
            case GameObjectType::CubePortal: return "CubePortal";
            case GameObjectType::Decoration: return "Decoration";
            case GameObjectType::YellowJumpPad: return "YellowJumpPad";
            case GameObjectType::PinkJumpPad: return "PinkJumpPad";
            case GameObjectType::GravityPad: return "GravityPad";
            case GameObjectType::YellowJumpRing: return "YellowJumpRing";
            case GameObjectType::PinkJumpRing: return "PinkJumpRing";
            case GameObjectType::GravityRing: return "GravityRing";
            case GameObjectType::BallPortal: return "BallPortal";
            case GameObjectType::UfoPortal: return "UfoPortal";
            case GameObjectType::Breakable: return "Breakable";
            case GameObjectType::Slope: return "Slope";
            case GameObjectType::WavePortal: return "WavePortal";
            case GameObjectType::RobotPortal: return "RobotPortal";
            case GameObjectType::SpiderPortal: return "SpiderPortal";
            case GameObjectType::SwingPortal: return "SwingPortal";
            case GameObjectType::AnimatedHazard: return "AnimatedHazard";
            case GameObjectType::CollisionObject: return "CollisionObject";
            case GameObjectType::Special: return "Special";
            default: return "Unknown";
        }
    }
}

SessionRecorder& SessionRecorder::get() {
    static SessionRecorder instance;
    return instance;
}

bool SessionRecorder::isActive() const {
    return m_active;
}

bool SessionRecorder::isSkipped() const {
    return m_skipped;
}

void SessionRecorder::beginSession(GJGameLevel* level) {
    endSession();

    m_skipped = false;
    m_attemptOpen = false;
    m_reference = false;
    m_practice = false;
    m_platformer = false;
    m_startPos = false;
    m_attemptNumber = 0;
    m_attempts.clear();
    m_filename.clear();
    resetAttemptBuffers();

    if (level && level->isPlatformer()) {
        m_skipped = true;
        log::info("GD Helper: skipping platformer level '{}'", std::string(level->m_levelName));
        return;
    }

    m_active = true;
    m_levelId = level ? static_cast<int>(level->m_levelID) : 0;
    m_levelVersion = level ? level->m_levelVersion : 0;
    m_levelLength = level ? level->m_levelLength : 0;
    m_levelName = level ? std::string(level->m_levelName) : "Unknown";
    m_platformer = false;

    auto const timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    m_filename = fmt::format(
        "session-{}-{}-{}.json",
        m_levelId,
        sanitize(m_levelName),
        timestamp
    );

    log::info("GD Helper: started session for '{}' (id {})", m_levelName, m_levelId);
}

void SessionRecorder::cancelSession() {
    m_active = false;
    m_skipped = false;
    m_attemptOpen = false;
    m_attempts.clear();
    m_filename.clear();
    resetAttemptBuffers();
}

void SessionRecorder::onReset(PlayLayer* layer) {
    if (!m_active || !m_attemptOpen) {
        return;
    }

    auto* player = layer ? layer->m_player1 : nullptr;
    auto rec = capture(layer, player);
    rec.outcome = "reset";
    m_attempts.push_back(std::move(rec));
    m_attemptOpen = false;
    persist();
}

void SessionRecorder::beginAttempt(PlayLayer* layer) {
    if (!m_active || m_skipped) {
        return;
    }

    ++m_attemptNumber;
    m_attemptOpen = true;
    resetAttemptBuffers();

    if (layer && m_attemptNumber == 1) {
        m_practice = layer->m_isPracticeMode;
        m_startPos = layer->m_isTestMode;
    }
}

void SessionRecorder::recordDeath(PlayLayer* layer, PlayerObject* player, GameObject* object) {
    if (!m_active || !m_attemptOpen) {
        return;
    }

    auto rec = capture(layer, player);
    rec.outcome = "death";
    rec.killer = killerFrom(object);
    m_attempts.push_back(std::move(rec));
    m_attemptOpen = false;
    persist();
}

void SessionRecorder::recordComplete(PlayLayer* layer) {
    if (!m_active || !m_attemptOpen) {
        return;
    }

    auto* player = layer ? layer->m_player1 : nullptr;
    auto rec = capture(layer, player);
    rec.outcome = "complete";
    rec.percent = 100.f;
    m_attempts.push_back(std::move(rec));
    m_attemptOpen = false;

    auto const practice = layer && layer->m_isPracticeMode;
    if (!practice && !m_startPos) {
        auto const key = referenceSaveKey();
        if (!Mod::get()->getSavedValue<bool>(key, false)) {
            m_reference = true;
            Mod::get()->setSavedValue(key, true);
            persistReferenceCopy();
            log::info("GD Helper: tagged first completion of '{}' as the reference run", m_levelName);
        }
    }

    persist();
}

void SessionRecorder::endSession() {
    if (!m_active) {
        m_skipped = false;
        return;
    }

    if (m_attemptOpen) {
        m_attemptOpen = false;
    }

    persist();
    log::info("GD Helper: ended session ({} attempts) -> {}", m_attempts.size(), m_filename);

    m_active = false;
    m_skipped = false;
    m_attempts.clear();
    m_filename.clear();
    resetAttemptBuffers();
}

float SessionRecorder::computePercent(PlayLayer* layer, PlayerObject* player) {
    if (!layer || !player) {
        return 0.f;
    }

    // Position in the level, not getCurrentPercent() — that value is often still
    // ~0 on the destroyPlayer frame (and would also be 0 for the anticheat dummy).
    float length = layer->m_levelLength;
    if (length <= 0.f && layer->m_endPortal) {
        length = layer->m_endPortal->getPositionX();
    }
    if (length <= 0.f) {
        return 0.f;
    }

    float percent = player->getPositionX() / length * 100.f;
    percent = std::clamp(percent, 0.f, 100.f);
    return std::round(percent * 100.f) / 100.f;
}

void SessionRecorder::resetAttemptBuffers() {
    m_frame = 0;
    m_path.clear();
    m_clicks.clear();
    m_path.reserve(256);
    m_clicks.reserve(64);
}

PathSample SessionRecorder::makePathSample(PlayLayer* layer, PlayerObject* player) const {
    PathSample sample;
    sample.frame = m_frame;
    sample.percent = computePercent(layer, player);
    sample.gamemode = gamemodeName(player);
    if (player) {
        auto const pos = player->getPosition();
        sample.x = pos.x;
        sample.y = pos.y;
    }
    return sample;
}

void SessionRecorder::samplePath(PlayLayer* layer) {
    if (!m_active || !m_attemptOpen || !layer || layer->m_isPaused) {
        return;
    }

    auto* player = layer->m_player1;
    if (!player || player->m_isDead) {
        return;
    }

    if (m_frame % pathSampleInterval == 0) {
        m_path.push_back(makePathSample(layer, player));
    }
    ++m_frame;
}

void SessionRecorder::recordClick(PlayLayer* layer, bool down, int button, bool player2) {
    if (!m_active || !m_attemptOpen || !layer || layer->m_isPaused) {
        return;
    }

    auto* player = player2 ? layer->m_player2 : layer->m_player1;
    if (!player || player->m_isDead) {
        return;
    }

    auto const pos = player->getPosition();
    m_clicks.push_back(ClickEvent {
        .frame = m_frame,
        .percent = computePercent(layer, player),
        .x = pos.x,
        .y = pos.y,
        .down = down,
        .button = buttonName(button),
        .player2 = player2,
    });
}

AttemptRecord SessionRecorder::capture(PlayLayer* layer, PlayerObject* player) {
    AttemptRecord rec;
    rec.attempt = m_attemptNumber;
    rec.percent = computePercent(layer, player);
    rec.gamemode = gamemodeName(player);
    if (player) {
        auto const pos = player->getPosition();
        rec.x = pos.x;
        rec.y = pos.y;
        if (m_path.empty() || m_path.back().frame != m_frame) {
            m_path.push_back(makePathSample(layer, player));
        }
    }
    rec.path = std::move(m_path);
    rec.clicks = std::move(m_clicks);
    resetAttemptBuffers();
    return rec;
}

std::string SessionRecorder::gamemodeName(PlayerObject* player) {
    if (!player) {
        return "unknown";
    }
    if (player->m_isShip) {
        return "ship";
    }
    if (player->m_isBird) {
        return "ufo";
    }
    if (player->m_isBall) {
        return "ball";
    }
    if (player->m_isDart) {
        return "wave";
    }
    if (player->m_isRobot) {
        return "robot";
    }
    if (player->m_isSpider) {
        return "spider";
    }
    if (player->m_isSwing) {
        return "swing";
    }
    return "cube";
}

std::string SessionRecorder::buttonName(int button) {
    switch (button) {
        case 1: return "jump";
        case 2: return "left";
        case 3: return "right";
        default: return "unknown";
    }
}

KillerInfo SessionRecorder::killerFrom(GameObject* object) {
    if (!object) {
        return KillerInfo { .objectId = 0, .type = "unknown" };
    }
    return KillerInfo {
        .objectId = object->m_objectID,
        .type = objectTypeName(object->m_objectType),
    };
}

std::string SessionRecorder::referenceSaveKey() const {
    return fmt::format("has-reference:{}:{}:{}", m_levelId, m_levelVersion, m_levelName);
}

matjson::Value SessionRecorder::toJson() const {
    std::vector<matjson::Value> attempts;
    attempts.reserve(m_attempts.size());
    for (auto const& attempt : m_attempts) {
        auto obj = matjson::makeObject({
            { "attempt", attempt.attempt },
            { "outcome", attempt.outcome },
            { "percent", attempt.percent },
            { "x", attempt.x },
            { "y", attempt.y },
            { "gamemode", attempt.gamemode },
        });
        if (attempt.killer) {
            obj["killer"] = matjson::makeObject({
                { "objectId", attempt.killer->objectId },
                { "type", attempt.killer->type },
            });
        }

        std::vector<matjson::Value> path;
        path.reserve(attempt.path.size());
        for (auto const& sample : attempt.path) {
            path.push_back(matjson::makeObject({
                { "frame", sample.frame },
                { "percent", sample.percent },
                { "x", sample.x },
                { "y", sample.y },
                { "gamemode", sample.gamemode },
            }));
        }
        obj["path"] = matjson::Value(std::move(path));

        std::vector<matjson::Value> clicks;
        clicks.reserve(attempt.clicks.size());
        for (auto const& click : attempt.clicks) {
            clicks.push_back(matjson::makeObject({
                { "frame", click.frame },
                { "down", click.down },
                { "button", click.button },
                { "player2", click.player2 },
                { "percent", click.percent },
                { "x", click.x },
                { "y", click.y },
            }));
        }
        obj["clicks"] = matjson::Value(std::move(clicks));

        attempts.push_back(std::move(obj));
    }

    return matjson::makeObject({
        { "schemaVersion", 1 },
        { "pathSampleInterval", pathSampleInterval },
        { "level", matjson::makeObject({
            { "id", m_levelId },
            { "name", m_levelName },
            { "version", m_levelVersion },
            { "length", m_levelLength },
        }) },
        { "mode", matjson::makeObject({
            { "practice", m_practice },
            { "platformer", m_platformer },
            { "startPos", m_startPos },
        }) },
        { "reference", m_reference },
        { "attempts", matjson::Value(std::move(attempts)) },
    });
}

void SessionRecorder::persist() const {
    if (m_filename.empty() || m_attempts.empty()) {
        return;
    }

    auto const dir = Mod::get()->getSaveDir() / "sessions";
    if (auto created = file::createDirectoryAll(dir); created.isErr()) {
        log::error("GD Helper: could not create sessions dir: {}", created.unwrapErr());
        return;
    }

    auto const path = dir / m_filename;
    if (auto written = file::writeToJson(path, toJson()); written.isErr()) {
        log::error("GD Helper: failed to write {}: {}", path.string(), written.unwrapErr());
        return;
    }

    log::debug("GD Helper: wrote {}", path.string());
}

void SessionRecorder::persistReferenceCopy() const {
    auto const dir = Mod::get()->getSaveDir() / "references";
    if (auto created = file::createDirectoryAll(dir); created.isErr()) {
        log::error("GD Helper: could not create references dir: {}", created.unwrapErr());
        return;
    }

    auto const path = dir / fmt::format("{}-v{}-{}.json", m_levelId, m_levelVersion, sanitize(m_levelName));
    if (auto written = file::writeToJson(path, toJson()); written.isErr()) {
        log::error("GD Helper: failed to write reference {}: {}", path.string(), written.unwrapErr());
        return;
    }

    log::info("GD Helper: saved reference run to {}", path.string());
}
