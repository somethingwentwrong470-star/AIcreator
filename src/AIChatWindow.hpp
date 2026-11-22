#pragma once
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

class AIChatWindow : public CCLayer {
protected:
    CCPoint m_dragStart;
    bool m_isDragging = false;
    bool m_isResizing = false;

    CCLayerColor* m_bg = nullptr;
    CCLayerColor* m_headerBg = nullptr;
    CCLayerColor* m_inputBg = nullptr;
    CCLabelTTF* m_title = nullptr;
    CCLabelTTF* m_chatLog = nullptr;
    CCTextFieldTTF* m_textField = nullptr;
    CCMenu* m_menu = nullptr;
    CCMenuItemSpriteExtra* m_sendBtn = nullptr;
    CCMenuItemSpriteExtra* m_abortBtn = nullptr;
    ButtonSprite* m_providerSpr = nullptr;
    async::TaskHolder<web::WebResponse> m_webHolder;

public:
    ~AIChatWindow() override;
    static AIChatWindow* create();
    bool init() override;

    void recreateTextField(bool openKeyboard);
    void updateLayoutPositions();
    void onSend(CCObject*);
    void onAbort(CCObject*);
    void executeAIActions(const std::string& rawText);
    void finishAIControl();

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override;
    void ccTouchMoved(CCTouch* touch, CCEvent*) override;
    void ccTouchEnded(CCTouch*, CCEvent*) override;
};
