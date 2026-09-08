#include "GoldenStitcher.hpp"

#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/cocos.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>

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
            return "level";
        }
        return out;
    }
}

GoldenStitcher& GoldenStitcher::get() {
    static GoldenStitcher instance;
    return instance;
}

StitcherStatus GoldenStitcher::status() const {
    return m_status;
}

bool GoldenStitcher::hasSeam() const {
    return m_hasSeam;
}

bool GoldenStitcher::canCommit() const {
    return m_status == StitcherStatus::RecordingSegment;
}

std::vector<InputEvent> const& GoldenStitcher::inputs() const {
    return m_golden;
}

SeamState const& GoldenStitcher::seam() const {
    return m_seam;
}

size_t GoldenStitcher::pendingCount() const {
    return m_pending.size();
}

size_t GoldenStitcher::startPosCount() const {
    return m_startPosXs.size();
}

char const* GoldenStitcher::statusLabel() const {
    switch (m_status) {
        case StitcherStatus::WaitingForSeam:
            return "Waiting for Seam Alignment";
        case StitcherStatus::RecordingSegment:
            return "Recording Segment";
        case StitcherStatus::Idle:
        default:
            return "Idle";
    }
}

std::string GoldenStitcher::nextStartPosLabel() const {
    if (m_startPosXs.empty()) {
        return "No StartPos - use Commit Segment";
    }
    if (!m_hasNextStartPos) {
        return "Last section - commit or finish";
    }
    return fmt::format(
        "Auto-save: SP {}/{} ({:.1f}%)",
        m_nextStartPosIndex,
        m_startPosXs.size(),
        percentAtX(m_nextStartPosX, m_levelLength)
    );
}

float GoldenStitcher::coveragePercent() const {
    if (!m_hasSeam) {
        return 0.f;
    }
    return std::clamp(m_seam.percent, 0.f, 100.f);
}

float GoldenStitcher::percentAtX(float x, float length) {
    if (length <= 0.f) {
        return 0.f;
    }
    return std::round(std::clamp(x / length * 100.f, 0.f, 100.f) * 100.f) / 100.f;
}

float GoldenStitcher::computePercent(PlayLayer* layer, PlayerObject* player) {
    if (!layer || !player) {
        return 0.f;
    }

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

SeamState GoldenStitcher::captureSeam(PlayLayer* layer, PlayerObject* player, int frame) {
    SeamState state;
    state.frame = frame;
    if (!player) {
        return state;
    }
    auto const pos = player->getPosition();
    state.x = pos.x;
    state.y = pos.y;
    state.yAccel = static_cast<float>(player->m_yVelocity);
    state.upsideDown = player->m_isUpsideDown;
    state.percent = computePercent(layer, player);
    return state;
}

bool GoldenStitcher::matchesSeam(PlayerObject* player) const {
    if (!player || !m_hasSeam) {
        return false;
    }
    auto const pos = player->getPosition();
    return std::abs(pos.x - m_seam.x) < epsX
        && std::abs(pos.y - m_seam.y) < epsY
        && std::abs(static_cast<float>(player->m_yVelocity) - m_seam.yAccel) < epsYAccel
        && player->m_isUpsideDown == m_seam.upsideDown;
}

bool GoldenStitcher::startsAtCommittedStartPos(PlayerObject* player) const {
    if (!player || !m_hasSeam || !m_committedAtStartPos) {
        return false;
    }
    auto const x = player->getPositionX();
    return std::abs(x - m_committedStartPosX) <= startPosSpawnEps
        && x <= m_seam.x + startPosSpawnEps;
}

void GoldenStitcher::snapOriginToNearbyStartPos(float spawnX) {
    m_segmentOriginX = spawnX;
    float bestDist = startPosSpawnEps;
    bool found = false;
    for (auto const x : m_startPosXs) {
        auto const dist = std::abs(x - spawnX);
        if (dist <= startPosSpawnEps && (!found || dist < bestDist)) {
            m_segmentOriginX = x;
            bestDist = dist;
            found = true;
        }
    }
}

int GoldenStitcher::globalFrame() const {
    return m_frameOffset + (m_localFrame - m_matchLocalFrame);
}

void GoldenStitcher::resetPending() {
    m_pending.clear();
    m_localFrame = 0;
    m_matchLocalFrame = 0;
    m_frameOffset = 0;
}

void GoldenStitcher::beginRecordingFromSeam() {
    m_status = StitcherStatus::RecordingSegment;
    m_frameOffset = m_seam.frame;
    m_matchLocalFrame = 0;
}

void GoldenStitcher::updateNextStartPos() {
    m_hasNextStartPos = false;
    m_nextStartPosX = 0.f;
    m_nextStartPosIndex = 0;

    for (size_t i = 0; i < m_startPosXs.size(); ++i) {
        auto const x = m_startPosXs[i];
        if (x <= m_segmentOriginX + startPosPassEps) {
            continue;
        }
        if (m_hasSeam && x <= m_seam.x + epsX) {
            continue;
        }
        m_hasNextStartPos = true;
        m_nextStartPosX = x;
        m_nextStartPosIndex = static_cast<int>(i + 1);
        return;
    }
}

void GoldenStitcher::beginLevel(GJGameLevel* level) {
    persist();
    m_status = StitcherStatus::Idle;
    resetPending();
    m_hasSeam = false;
    m_committedAtStartPos = false;
    m_hasNextStartPos = false;
    m_seam = {};
    m_golden.clear();
    m_startPosXs.clear();
    m_segmentOriginX = 0.f;
    m_committedStartPosX = 0.f;
    m_nextStartPosX = 0.f;
    m_nextStartPosIndex = 0;
    m_levelId = level ? static_cast<int>(level->m_levelID) : 0;
    m_levelVersion = level ? level->m_levelVersion : 0;
    m_levelLength = level ? static_cast<float>(level->m_levelLength) : 0.f;
    m_levelName = level ? std::string(level->m_levelName) : "Unknown";
    loadFromDisk();
}

void GoldenStitcher::scanStartPositions(PlayLayer* layer) {
    m_startPosXs.clear();
    if (!layer) {
        return;
    }
    if (layer->m_levelLength > 0.f) {
        m_levelLength = layer->m_levelLength;
    }

    if (layer->m_objects) {
        for (auto* obj : CCArrayExt<GameObject*>(layer->m_objects)) {
            if (obj && obj->m_objectID == 31) {
                m_startPosXs.push_back(obj->getPositionX());
            }
        }
    }

    std::sort(m_startPosXs.begin(), m_startPosXs.end());
    m_startPosXs.erase(
        std::unique(m_startPosXs.begin(), m_startPosXs.end(), [](float a, float b) {
            return std::abs(a - b) < 1.f;
        }),
        m_startPosXs.end()
    );

    if (layer->m_player1) {
        snapOriginToNearbyStartPos(layer->m_player1->getPositionX());
    }
    updateNextStartPos();
    log::info("GD Helper: found {} StartPos object(s)", m_startPosXs.size());
}

void GoldenStitcher::onAttemptStart(PlayLayer* layer) {
    resetPending();
    m_status = StitcherStatus::Idle;
    if (!layer || !layer->m_player1) {
        return;
    }

    if (m_startPosXs.empty()) {
        scanStartPositions(layer);
    }

    snapOriginToNearbyStartPos(layer->m_player1->getPositionX());
    updateNextStartPos();

    if (!m_hasSeam) {
        m_status = StitcherStatus::RecordingSegment;
        m_frameOffset = 0;
        m_matchLocalFrame = 0;
        log::info("GD Helper: golden first segment — recording");
        return;
    }

    if (startsAtCommittedStartPos(layer->m_player1)) {
        m_segmentOriginX = m_committedStartPosX;
        updateNextStartPos();
        beginRecordingFromSeam();
        log::info(
            "GD Helper: recording from saved StartPos at x={:.1f}",
            m_committedStartPosX
        );
        return;
    }

    auto const pos = layer->m_player1->getPosition();
    if (pos.x + epsX < m_seam.x || !matchesSeam(layer->m_player1)) {
        m_status = StitcherStatus::WaitingForSeam;
        log::info(
            "GD Helper: waiting for seam alignment at x={:.1f} y={:.1f}",
            m_seam.x,
            m_seam.y
        );
        return;
    }

    beginRecordingFromSeam();
}

void GoldenStitcher::onPostUpdate(PlayLayer* layer) {
    if (!layer || layer->m_isPaused) {
        return;
    }
    auto* player = layer->m_player1;
    if (!player || player->m_isDead) {
        return;
    }

    if (m_status == StitcherStatus::WaitingForSeam && matchesSeam(player)) {
        m_status = StitcherStatus::RecordingSegment;
        m_matchLocalFrame = m_localFrame;
        m_frameOffset = m_seam.frame;
        log::info("GD Helper: seam aligned at local frame {}", m_localFrame);
    }

    tryAutoCommit(layer);
    ++m_localFrame;
}

void GoldenStitcher::onClick(PlayLayer* layer, bool down, int button, bool player2) {
    if (player2 || m_status != StitcherStatus::RecordingSegment || !layer) {
        return;
    }
    auto* player = layer->m_player1;
    if (!player || player->m_isDead || layer->m_isPaused) {
        return;
    }

    m_pending.push_back(InputEvent {
        .frame = globalFrame(),
        .button = button,
        .down = down,
    });
}

void GoldenStitcher::onDeath() {
    if (m_status == StitcherStatus::Idle) {
        return;
    }
    invalidateSegment();
}

void GoldenStitcher::onComplete(PlayLayer* layer) {
    if (m_status == StitcherStatus::RecordingSegment) {
        commitSegment(layer);
        if (m_hasSeam) {
            m_seam.percent = 100.f;
            persist();
        }
    }
}

void GoldenStitcher::onQuit() {
    if (m_status != StitcherStatus::Idle) {
        invalidateSegment();
    }
    persist();
    m_status = StitcherStatus::Idle;
}

void GoldenStitcher::invalidateSegment() {
    auto const wasWaiting = m_status == StitcherStatus::WaitingForSeam;
    resetPending();
    m_status = StitcherStatus::Idle;
    log::info(
        "GD Helper: golden segment invalidated ({})",
        wasWaiting ? "died before seam" : "died before commit"
    );
}

bool GoldenStitcher::tryAutoCommit(PlayLayer* layer) {
    if (m_status != StitcherStatus::RecordingSegment || !m_hasNextStartPos || !layer) {
        return false;
    }
    auto* player = layer->m_player1;
    if (!player || player->getPositionX() + 0.01f < m_nextStartPosX) {
        return false;
    }

    auto const savedX = m_nextStartPosX;
    auto const index = m_nextStartPosIndex;
    m_committedAtStartPos = true;
    m_committedStartPosX = savedX;

    if (!commitSegment(layer, true)) {
        m_committedAtStartPos = false;
        m_committedStartPosX = 0.f;
        return false;
    }

    m_segmentOriginX = savedX;
    updateNextStartPos();
    Notification::create(
        fmt::format("Saved at StartPos {} ({:.1f}% mapped)", index, coveragePercent()),
        NotificationIcon::Success
    )->show();
    log::info(
        "GD Helper: auto-saved golden segment at StartPos {} (x={:.1f})",
        index,
        savedX
    );
    return true;
}

bool GoldenStitcher::commitSegment(PlayLayer* layer, bool continueRecording) {
    if (m_status != StitcherStatus::RecordingSegment || !layer) {
        return false;
    }

    auto* player = layer->m_player1;
    if (!player) {
        return false;
    }

    m_pending.erase(
        std::remove_if(m_pending.begin(), m_pending.end(), [this](InputEvent const& ev) {
            return m_hasSeam && ev.frame <= m_seam.frame;
        }),
        m_pending.end()
    );

    m_golden.insert(m_golden.end(), m_pending.begin(), m_pending.end());
    m_seam = captureSeam(layer, player, globalFrame());
    m_hasSeam = true;
    if (!continueRecording) {
        m_committedAtStartPos = false;
        m_committedStartPosX = 0.f;
    }
    resetPending();
    if (continueRecording) {
        beginRecordingFromSeam();
    }
    else {
        m_status = StitcherStatus::Idle;
    }
    persist();

    log::info(
        "GD Helper: committed golden segment ({} inputs, mapped {:.2f}%)",
        m_golden.size(),
        m_seam.percent
    );
    return true;
}

void GoldenStitcher::clearGoldenRun() {
    m_golden.clear();
    m_hasSeam = false;
    m_committedAtStartPos = false;
    m_seam = {};
    m_committedStartPosX = 0.f;
    resetPending();
    m_status = StitcherStatus::Idle;
    updateNextStartPos();
    persist();
    log::info("GD Helper: cleared golden run");
}

std::filesystem::path GoldenStitcher::goldenPath() const {
    auto const dir = Mod::get()->getSaveDir() / "golden";
    return dir / fmt::format(
        "{}-v{}-{}.json",
        m_levelId,
        m_levelVersion,
        sanitize(m_levelName)
    );
}

matjson::Value GoldenStitcher::toJson() const {
    std::vector<matjson::Value> inputs;
    inputs.reserve(m_golden.size());
    for (auto const& ev : m_golden) {
        inputs.push_back(matjson::makeObject({
            { "frame", ev.frame },
            { "button", ev.button },
            { "down", ev.down },
        }));
    }

    auto json = matjson::makeObject({
        { "schemaVersion", 1 },
        { "level", matjson::makeObject({
            { "id", m_levelId },
            { "name", m_levelName },
            { "version", m_levelVersion },
            { "length", m_levelLength },
        }) },
        { "coveragePercent", coveragePercent() },
        { "inputs", matjson::Value(std::move(inputs)) },
    });

    if (m_hasSeam) {
        auto seam = matjson::makeObject({
            { "x", m_seam.x },
            { "y", m_seam.y },
            { "yAccel", m_seam.yAccel },
            { "upsideDown", m_seam.upsideDown },
            { "frame", m_seam.frame },
            { "percent", m_seam.percent },
        });
        if (m_committedAtStartPos) {
            seam["startPosX"] = m_committedStartPosX;
        }
        json["seam"] = std::move(seam);
    }

    return json;
}

void GoldenStitcher::persist() const {
    if (m_levelName.empty() && m_levelId == 0) {
        return;
    }
    if (!m_hasSeam && m_golden.empty()) {
        auto const path = goldenPath();
        if (std::filesystem::exists(path)) {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
        return;
    }

    auto const dir = Mod::get()->getSaveDir() / "golden";
    if (auto created = file::createDirectoryAll(dir); created.isErr()) {
        log::error("GD Helper: could not create golden dir: {}", created.unwrapErr());
        return;
    }

    auto const path = goldenPath();
    if (auto written = file::writeToJson(path, toJson()); written.isErr()) {
        log::error("GD Helper: failed to write {}: {}", path.string(), written.unwrapErr());
        return;
    }
}

void GoldenStitcher::loadFromDisk() {
    auto const path = goldenPath();
    auto parsed = file::readJson(path);
    if (parsed.isErr()) {
        return;
    }

    auto json = parsed.unwrap();
    m_golden.clear();
    if (json.contains("inputs") && json["inputs"].isArray()) {
        for (auto const& value : json["inputs"]) {
            m_golden.push_back(InputEvent {
                .frame = static_cast<int>(value["frame"].asInt().unwrapOr(0)),
                .button = static_cast<int>(value["button"].asInt().unwrapOr(1)),
                .down = value["down"].asBool().unwrapOr(false),
            });
        }
    }

    m_committedAtStartPos = false;
    m_committedStartPosX = 0.f;
    if (json.contains("seam") && json["seam"].isObject()) {
        auto const& obj = json["seam"];
        m_seam.x = static_cast<float>(obj["x"].asDouble().unwrapOr(0.0));
        m_seam.y = static_cast<float>(obj["y"].asDouble().unwrapOr(0.0));
        m_seam.yAccel = static_cast<float>(obj["yAccel"].asDouble().unwrapOr(0.0));
        m_seam.upsideDown = obj["upsideDown"].asBool().unwrapOr(false);
        m_seam.frame = static_cast<int>(obj["frame"].asInt().unwrapOr(0));
        m_seam.percent = static_cast<float>(obj["percent"].asDouble().unwrapOr(0.0));
        m_hasSeam = true;
        if (obj.contains("startPosX")) {
            m_committedStartPosX = static_cast<float>(obj["startPosX"].asDouble().unwrapOr(0.0));
            m_committedAtStartPos = true;
        }
    }

    log::info(
        "GD Helper: loaded golden run ({} inputs, mapped {:.2f}%)",
        m_golden.size(),
        coveragePercent()
    );
}
