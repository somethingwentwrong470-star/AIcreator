#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include "Config.hpp"
#include "AIChatWindow.hpp"

using namespace geode::prelude;

class $modify(AIEditorLayerHook, LevelEditorLayer) {
    bool ccTouchBegan(CCTouch* touch, CCEvent* event) {
        if (g_aiControlling) return false;
        return LevelEditorLayer::ccTouchBegan(touch, event);
    }
};

class $modify(AIEditorUIHook, EditorUI) {
    struct Fields {
        AIChatWindow* chatWindow = nullptr;
        CCSprite* crossSprite = nullptr;
        CCMenu* btnMenu = nullptr;
    };

    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;
        g_aiControlling = false;
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto brainSpr = CCSprite::create("brain.png"_spr);
        if (brainSpr) {
            auto dummyNode = CCNode::create();
            dummyNode->setContentSize({30.f, 30.f});
            auto circleBtnSpr = CircleButtonSprite::create(dummyNode, CircleBaseColor::Green, CircleBaseSize::Small);
            circleBtnSpr->setScale(0.85f);
            brainSpr->setPosition(circleBtnSpr->getContentSize() / 2.f);
            if (brainSpr->getContentSize().width > 0) {
                brainSpr->setScale(22.f / brainSpr->getContentSize().width);
            }
            circleBtnSpr->addChild(brainSpr, 5);

            m_fields->crossSprite = CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png");
            if (m_fields->crossSprite) {
                m_fields->crossSprite->setPosition(circleBtnSpr->getContentSize() / 2.f);
                m_fields->crossSprite->setScale(0.8f);
                m_fields->crossSprite->setVisible(false);
                circleBtnSpr->addChild(m_fields->crossSprite, 10);
            }
            auto aiBtn = CCMenuItemSpriteExtra::create(circleBtnSpr, this, menu_selector(AIEditorUIHook::onToggleAIChat));
            m_fields->btnMenu = CCMenu::create();
            m_fields->btnMenu->setPosition({195.f, winSize.height - 23.f});
            m_fields->btnMenu->addChild(aiBtn);
            this->addChild(m_fields->btnMenu, 100);
        }
        return true;
    }
    void updateObjectInfoLabel() {
        EditorUI::updateObjectInfoLabel();
        if (m_objectInfoLabel && m_selectedObject) {
            std::string current = m_objectInfoLabel->getString();
            m_objectInfoLabel->setString(
                fmt::format("{}\nObject ID: {}", current, m_selectedObject->m_objectID).c_str()
            );
        }
    }

    void onPlaytest(CCObject* sender) {
        EditorUI::onPlaytest(sender);
        if (m_fields->btnMenu) m_fields->btnMenu->setVisible(false);
        if (m_fields->chatWindow) m_fields->chatWindow->setVisible(false);
        if (m_fields->crossSprite) m_fields->crossSprite->setVisible(false);
    }

    void onStopPlaytest(CCObject* sender) {
        EditorUI::onStopPlaytest(sender);
        if (m_fields->btnMenu) m_fields->btnMenu->setVisible(true);
    }

    void onToggleAIChat(CCObject*) {
        bool nowVisible = false;
        if (!m_fields->chatWindow) {
            m_fields->chatWindow = AIChatWindow::create();
            m_fields->chatWindow->setPosition({110.f, 95.f});
            this->addChild(m_fields->chatWindow, 500);
            nowVisible = true;
        } else {
            nowVisible = !m_fields->chatWindow->isVisible();
            m_fields->chatWindow->setVisible(nowVisible);
        }
        if (m_fields->crossSprite) m_fields->crossSprite->setVisible(nowVisible);
    }
};
