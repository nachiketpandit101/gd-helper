#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "SessionRecorder.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    log::info("GD Helper loaded");
}

class $modify(GDHelperPlayLayer, PlayLayer) {
    struct Fields {
        bool deathRecorded = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        SessionRecorder::get().beginSession(level);
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            SessionRecorder::get().cancelSession();
            return false;
        }
        return true;
    }

    void resetLevel() {
        SessionRecorder::get().onReset(this);
        PlayLayer::resetLevel();
        m_fields->deathRecorded = false;
        SessionRecorder::get().beginAttempt(this);
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        auto const isRealPlayer = player == m_player1 || player == m_player2;
        auto const wasDead = player && player->m_isDead;

        PlayLayer::destroyPlayer(player, object);

        // Anticheat dummy / ignore-damage calls hit this hook but do not kill.
        // Only record the frame the player actually dies.
        if (!isRealPlayer || wasDead || m_fields->deathRecorded) {
            return;
        }
        if (!player || !player->m_isDead) {
            return;
        }

        m_fields->deathRecorded = true;
        SessionRecorder::get().recordDeath(this, player, object);
    }

    void levelComplete() {
        SessionRecorder::get().recordComplete(this);
        PlayLayer::levelComplete();
    }

    void onQuit() {
        SessionRecorder::get().onReset(this);
        SessionRecorder::get().endSession();
        PlayLayer::onQuit();
    }
};
