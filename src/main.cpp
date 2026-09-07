#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/Notification.hpp>

#include "GoldenRunPopup.hpp"
#include "GoldenStitcher.hpp"
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
        GoldenStitcher::get().beginLevel(level);
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
        GoldenStitcher::get().onAttemptStart(this);
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        SessionRecorder::get().samplePath(this);
        GoldenStitcher::get().onPostUpdate(this);
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
        GoldenStitcher::get().onDeath();
    }

    void levelComplete() {
        SessionRecorder::get().recordComplete(this);
        GoldenStitcher::get().onComplete(this);
        PlayLayer::levelComplete();
    }

    void onQuit() {
        SessionRecorder::get().onReset(this);
        SessionRecorder::get().endSession();
        GoldenStitcher::get().onQuit();
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
        GoldenStitcher::get().onClick(playLayer, down, button, player2);
    }
};

class $modify(GDHelperPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto* menu = CCMenu::create();
        menu->setID("golden-run-menu"_spr);
        menu->setPosition({ 0.f, 0.f });

        auto* spr = ButtonSprite::create("Golden Run", "bigFont.fnt", "GJ_button_02.png", 0.8f);
        spr->setScale(0.55f);
        auto* btn = CCMenuItemSpriteExtra::create(
            spr,
            this,
            menu_selector(GDHelperPauseLayer::onGoldenRun)
        );
        btn->setID("golden-run-button"_spr);
        btn->setPosition({ 70.f, 28.f });
        menu->addChild(btn);

        this->addChild(menu, 100);
    }

    void onGoldenRun(CCObject*) {
        GoldenRunPopup::create()->show();
    }
};
