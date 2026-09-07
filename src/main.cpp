#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/Notification.hpp>

#include "SessionRecorder.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    log::info("GD Helper loaded");
    listenForSettingChanges<bool>(SessionRecorder::recordSettingKey, [](bool enabled) {
        SessionRecorder::get().applyEnabled(enabled);
    });
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

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        SessionRecorder::get().samplePath(this);
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

class $modify(GDHelperBaseLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool player2) {
        GJBaseGameLayer::handleButton(down, button, player2);
        auto* playLayer = PlayLayer::get();
        if (!playLayer || static_cast<void*>(playLayer) != static_cast<void*>(this)) {
            return;
        }
        SessionRecorder::get().recordClick(playLayer, down, button, player2);
    }
};

class $modify(GDHelperPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto const enabled = SessionRecorder::isRecordingEnabled();

        auto* menu = CCMenu::create();
        menu->setID("record-menu"_spr);
        menu->setPosition({ 0.f, 0.f });

        auto* toggle = CCMenuItemToggler::createWithStandardSprites(
            this,
            menu_selector(GDHelperPauseLayer::onToggleRecording),
            0.7f
        );
        toggle->setID("record-toggle"_spr);
        toggle->toggle(enabled);
        toggle->setPosition({ 36.f, 36.f });
        menu->addChild(toggle);

        this->addChild(menu, 100);

        auto* label = CCLabelBMFont::create("Record", "bigFont.fnt");
        label->setID("record-label"_spr);
        label->setScale(0.35f);
        label->setAnchorPoint({ 0.f, 0.5f });
        label->setPosition({ 58.f, 36.f });
        this->addChild(label, 100);
    }

    void onToggleRecording(CCObject*) {
        auto const enabled = !SessionRecorder::isRecordingEnabled();
        Mod::get()->setSettingValue<bool>(SessionRecorder::recordSettingKey, enabled);
        Notification::create(
            enabled ? "GD Helper recording on" : "GD Helper recording off",
            enabled ? NotificationIcon::Success : NotificationIcon::None
        )->show();
    }
};
