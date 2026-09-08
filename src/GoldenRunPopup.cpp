#include "GoldenRunPopup.hpp"

#include "GoldenStitcher.hpp"
#include "SessionRecorder.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <algorithm>

using namespace geode::prelude;

bool GoldenRunPopup::init() {
    if (!Popup::init(320.f, 270.f)) {
        return false;
    }

    this->setTitle("Golden Run");

    m_coverageLabel = CCLabelBMFont::create("Mapped 0%", "bigFont.fnt");
    m_coverageLabel->setScale(0.4f);
    m_mainLayer->addChildAtPosition(m_coverageLabel, Anchor::Top, { 0.f, -48.f });

    m_barWidth = 240.f;
    auto* barHost = CCNode::create();
    barHost->setContentSize({ m_barWidth, 14.f });
    barHost->setAnchorPoint({ 0.5f, 0.5f });

    auto* barBG = CCLayerColor::create({ 40, 40, 40, 255 }, m_barWidth, 14.f);
    barBG->ignoreAnchorPointForPosition(false);
    barBG->setAnchorPoint({ 0.f, 0.f });
    barBG->setPosition({ 0.f, 0.f });
    barHost->addChild(barBG);

    m_barFill = CCLayerColor::create({ 90, 200, 90, 255 }, m_barWidth, 14.f);
    m_barFill->ignoreAnchorPointForPosition(false);
    m_barFill->setAnchorPoint({ 0.f, 0.f });
    m_barFill->setPosition({ 0.f, 0.f });
    m_barFill->setScaleX(0.f);
    barHost->addChild(m_barFill);
    m_mainLayer->addChildAtPosition(barHost, Anchor::Top, { 0.f, -72.f });

    m_statusLabel = CCLabelBMFont::create("Idle", "goldFont.fnt");
    m_statusLabel->setScale(0.4f);
    m_mainLayer->addChildAtPosition(m_statusLabel, Anchor::Center, { 0.f, 28.f });

    m_inputsLabel = CCLabelBMFont::create("0 inputs", "bigFont.fnt");
    m_inputsLabel->setScale(0.3f);
    m_mainLayer->addChildAtPosition(m_inputsLabel, Anchor::Center, { 0.f, 12.f });

    m_nextStartPosLabel = CCLabelBMFont::create("No StartPos", "bigFont.fnt");
    m_nextStartPosLabel->setScale(0.28f);
    m_mainLayer->addChildAtPosition(m_nextStartPosLabel, Anchor::Center, { 0.f, -6.f });

    auto* recordLabel = CCLabelBMFont::create("Record Session", "bigFont.fnt");
    recordLabel->setScale(0.35f);
    recordLabel->setAnchorPoint({ 1.f, 0.5f });
    m_mainLayer->addChildAtPosition(recordLabel, Anchor::Center, { -8.f, -32.f });

    m_recordStateLabel = CCLabelBMFont::create("Off", "bigFont.fnt");
    m_recordStateLabel->setScale(0.3f);
    m_recordStateLabel->setAnchorPoint({ 0.f, 0.5f });
    m_mainLayer->addChildAtPosition(m_recordStateLabel, Anchor::Center, { 36.f, -32.f });

    m_recordToggle = CCMenuItemToggler::createWithStandardSprites(
        this,
        menu_selector(GoldenRunPopup::onToggleRecording),
        0.6f
    );
    m_recordToggle->m_notClickable = true;
    m_buttonMenu->addChildAtPosition(m_recordToggle, Anchor::Center, { 16.f, -32.f });

    auto* commitSpr = ButtonSprite::create("Commit Segment", "bigFont.fnt", "GJ_button_01.png", 0.8f);
    commitSpr->setScale(0.7f);
    m_commitBtn = CCMenuItemSpriteExtra::create(
        commitSpr,
        this,
        menu_selector(GoldenRunPopup::onCommit)
    );
    m_buttonMenu->addChildAtPosition(m_commitBtn, Anchor::Bottom, { 0.f, 48.f });

    auto* clearSpr = ButtonSprite::create("Clear", "bigFont.fnt", "GJ_button_06.png", 0.8f);
    clearSpr->setScale(0.6f);
    auto* clearBtn = CCMenuItemSpriteExtra::create(
        clearSpr,
        this,
        menu_selector(GoldenRunPopup::onClear)
    );
    m_buttonMenu->addChildAtPosition(clearBtn, Anchor::Bottom, { 0.f, 18.f });

    this->refresh();
    return true;
}

void GoldenRunPopup::refresh() {
    auto& stitcher = GoldenStitcher::get();
    auto const coverage = std::clamp(stitcher.coveragePercent(), 0.f, 100.f);

    m_coverageLabel->setString(fmt::format("Mapped {:.1f}%", coverage).c_str());
    m_barFill->setScaleX(coverage / 100.f);

    m_statusLabel->setString(stitcher.statusLabel());
    switch (stitcher.status()) {
        case StitcherStatus::WaitingForSeam:
            m_statusLabel->setColor({ 255, 220, 80 });
            break;
        case StitcherStatus::RecordingSegment:
            m_statusLabel->setColor({ 120, 255, 120 });
            break;
        case StitcherStatus::Idle:
        default:
            m_statusLabel->setColor({ 255, 255, 255 });
            break;
    }

    m_inputsLabel->setString(fmt::format(
        "{} committed  |  {} pending",
        stitcher.inputs().size(),
        stitcher.pendingCount()
    ).c_str());
    m_nextStartPosLabel->setString(stitcher.nextStartPosLabel().c_str());
    m_nextStartPosLabel->setColor(
        stitcher.startPosCount() == 0 ? ccColor3B { 200, 200, 200 } : ccColor3B { 180, 220, 255 }
    );

    auto const recording = SessionRecorder::isRecordingEnabled();
    m_recordToggle->toggle(recording);
    m_recordStateLabel->setString(recording ? "On" : "Off");
    m_recordStateLabel->setColor(recording ? ccColor3B { 120, 255, 120 } : ccColor3B { 200, 200, 200 });
    m_commitBtn->setEnabled(stitcher.canCommit());
    m_commitBtn->setColor(stitcher.canCommit() ? ccWHITE : ccGRAY);
}

void GoldenRunPopup::onToggleRecording(CCObject*) {
    auto const enabled = !m_recordToggle->isToggled();
    Mod::get()->setSettingValue<bool>(SessionRecorder::recordSettingKey, enabled);
    m_recordToggle->toggle(enabled);
    m_recordStateLabel->setString(enabled ? "On" : "Off");
    m_recordStateLabel->setColor(enabled ? ccColor3B { 120, 255, 120 } : ccColor3B { 200, 200, 200 });
    Notification::create(
        enabled ? "GD Helper recording on" : "GD Helper recording off",
        enabled ? NotificationIcon::Success : NotificationIcon::None
    )->show();
}

void GoldenRunPopup::onCommit(CCObject*) {
    auto* layer = PlayLayer::get();
    if (!GoldenStitcher::get().commitSegment(layer)) {
        Notification::create("Nothing to commit", NotificationIcon::Warning)->show();
        this->refresh();
        return;
    }
    Notification::create(
        fmt::format("Segment committed ({:.1f}% mapped)", GoldenStitcher::get().coveragePercent()),
        NotificationIcon::Success
    )->show();
    this->refresh();
}

void GoldenRunPopup::onClear(CCObject*) {
    createQuickPopup(
        "Clear Golden Run",
        "Delete the stitched click sequence for this level?",
        "Cancel",
        "Clear",
        [this](auto, bool confirm) {
            if (!confirm) {
                return;
            }
            GoldenStitcher::get().clearGoldenRun();
            Notification::create("Golden run cleared", NotificationIcon::None)->show();
            this->refresh();
        }
    );
}

GoldenRunPopup* GoldenRunPopup::create() {
    auto* ret = new GoldenRunPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}
