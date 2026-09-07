#pragma once

#include <Geode/ui/Popup.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/binding/ButtonSprite.hpp>

class GoldenRunPopup : public geode::Popup {
protected:
    cocos2d::CCLabelBMFont* m_coverageLabel = nullptr;
    cocos2d::CCLabelBMFont* m_statusLabel = nullptr;
    cocos2d::CCLabelBMFont* m_inputsLabel = nullptr;
    cocos2d::CCLayerColor* m_barFill = nullptr;
    CCMenuItemToggler* m_recordToggle = nullptr;
    cocos2d::CCLabelBMFont* m_recordStateLabel = nullptr;
    CCMenuItemSpriteExtra* m_commitBtn = nullptr;
    float m_barWidth = 240.f;

    bool init();
    void refresh();
    void onToggleRecording(CCObject* sender);
    void onCommit(CCObject* sender);
    void onClear(CCObject* sender);

public:
    static GoldenRunPopup* create();
};
