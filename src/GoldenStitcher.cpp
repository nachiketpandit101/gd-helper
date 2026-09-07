#include "GoldenStitcher.hpp"

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

float GoldenStitcher::coveragePercent() const {
    if (!m_hasSeam) {
        return 0.f;
    }
    return std::clamp(m_seam.percent, 0.f, 100.f);
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

int GoldenStitcher::globalFrame() const {
    return m_frameOffset + (m_localFrame - m_matchLocalFrame);
}

void GoldenStitcher::resetPending() {
    m_pending.clear();
    m_localFrame = 0;
    m_matchLocalFrame = 0;
    m_frameOffset = 0;
}

void GoldenStitcher::beginLevel(GJGameLevel* level) {
    persist();
    m_status = StitcherStatus::Idle;
    resetPending();
    m_hasSeam = false;
    m_seam = {};
    m_golden.clear();
    m_levelId = level ? static_cast<int>(level->m_levelID) : 0;
    m_levelVersion = level ? level->m_levelVersion : 0;
    m_levelLength = level ? static_cast<float>(level->m_levelLength) : 0.f;
    m_levelName = level ? std::string(level->m_levelName) : "Unknown";
    loadFromDisk();
}

void GoldenStitcher::onAttemptStart(PlayLayer* layer) {
    resetPending();
    m_status = StitcherStatus::Idle;
    if (!layer || !layer->m_player1) {
        return;
    }

    if (!m_hasSeam) {
        m_status = StitcherStatus::RecordingSegment;
        m_frameOffset = 0;
        m_matchLocalFrame = 0;
        log::info("GD Helper: golden first segment — recording");
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

    m_status = StitcherStatus::RecordingSegment;
    m_frameOffset = m_seam.frame;
    m_matchLocalFrame = 0;
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

bool GoldenStitcher::commitSegment(PlayLayer* layer) {
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
    resetPending();
    m_status = StitcherStatus::Idle;
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
    m_seam = {};
    resetPending();
    m_status = StitcherStatus::Idle;
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
        json["seam"] = matjson::makeObject({
            { "x", m_seam.x },
            { "y", m_seam.y },
            { "yAccel", m_seam.yAccel },
            { "upsideDown", m_seam.upsideDown },
            { "frame", m_seam.frame },
            { "percent", m_seam.percent },
        });
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

    if (json.contains("seam") && json["seam"].isObject()) {
        auto const& obj = json["seam"];
        m_seam.x = static_cast<float>(obj["x"].asDouble().unwrapOr(0.0));
        m_seam.y = static_cast<float>(obj["y"].asDouble().unwrapOr(0.0));
        m_seam.yAccel = static_cast<float>(obj["yAccel"].asDouble().unwrapOr(0.0));
        m_seam.upsideDown = obj["upsideDown"].asBool().unwrapOr(false);
        m_seam.frame = static_cast<int>(obj["frame"].asInt().unwrapOr(0));
        m_seam.percent = static_cast<float>(obj["percent"].asDouble().unwrapOr(0.0));
        m_hasSeam = true;
    }

    log::info(
        "GD Helper: loaded golden run ({} inputs, mapped {:.2f}%)",
        m_golden.size(),
        coveragePercent()
    );
}
