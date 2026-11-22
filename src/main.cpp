#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/web.hpp>
#include <fstream>
#include <sstream>
#include <vector>

using namespace geode::prelude;

bool g_aiControlling = false;

std::string trimStr(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

void ensureConfigTemplate() {
    auto cfgPath = Mod::get()->getConfigDir() / "ai_config.txt";
    if (!std::filesystem::exists(cfgPath)) {
        std::ofstream out(cfgPath);
        if (out) {
            out << "CF_ID=\n";
            out << "CF_KEY=\n";
        }
    }
}

std::string readConfigValue(const std::string& keyName) {
    ensureConfigTemplate();
    auto cfgPath = Mod::get()->getConfigDir() / "ai_config.txt";
    std::ifstream in(cfgPath);
    std::string line;
    while (std::getline(in, line)) {
        line = trimStr(line);
        if (line.rfind(keyName + "=", 0) == 0) {
            return trimStr(line.substr(keyName.size() + 1));
        }
    }
    return "";
}

std::string extractHex32(const std::string& raw) {
    std::string hexOnly;
    for (char c : raw) {
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
            hexOnly.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    if (hexOnly.size() >= 32) return hexOnly.substr(hexOnly.size() - 32);
    return hexOnly;
}

class AIChatWindow : public CCLayer {
protected:
    CCPoint m_dragStart;
    bool m_isDragging = false;
    bool m_isResizing = false;

    CCLayerColor* m_bg = nullptr;
    CCLayerColor* m_headerBg = nullptr;
    CCLabelTTF* m_title = nullptr;
    CCLabelTTF* m_badgeLabel = nullptr;
    CCLabelTTF* m_chatLog = nullptr;
    TextInput* m_textInput = nullptr;
    CCMenu* m_menu = nullptr;
    CCMenuItemSpriteExtra* m_sendBtn = nullptr;
    CCMenuItemSpriteExtra* m_clearBtn = nullptr;
    CCMenuItemSpriteExtra* m_abortBtn = nullptr;
    async::TaskHolder<web::WebResponse> m_webHolder;

public:
    ~AIChatWindow() override {
        m_webHolder.cancel();
        g_aiControlling = false;
    }

    static AIChatWindow* create() {
        auto ret = new AIChatWindow();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
    bool init() override {
        if (!CCLayer::init()) return false;
        ensureConfigTemplate();
        g_aiControlling = false;

        this->setContentSize({250.f, 170.f});
        this->setTouchEnabled(true);
        this->setTouchMode(kCCTouchesOneByOne);
        this->setTouchPriority(-500);

        m_bg = CCLayerColor::create({18, 20, 32, 240}, 250.f, 170.f);
        this->addChild(m_bg, 0);

        m_headerBg = CCLayerColor::create({32, 36, 56, 255}, 250.f, 22.f);
        this->addChild(m_headerBg, 1);

        m_title = CCLabelTTF::create("ИИ Чат", "ru_font.ttf"_spr, 11);
        m_title->setAnchorPoint({0.f, 0.5f});
        this->addChild(m_title, 5);

        m_badgeLabel = CCLabelTTF::create("Cloudflare AI", "ru_font.ttf"_spr, 9);
        m_badgeLabel->setColor({110, 210, 255});
        this->addChild(m_badgeLabel, 5);

        m_chatLog = CCLabelTTF::create(
            "Cloudflare готов! Напиши запрос (например: «построй 3 прыжка через шипы»):",
            "ru_font.ttf"_spr, 10, {234.f, 100.f}, kCCTextAlignmentLeft
        );
        this->addChild(m_chatLog, 5);

        m_textInput = TextInput::create(165.f, "Введи запрос...", "ru_font.ttf"_spr);
        m_textInput->setCommonFilter(CommonFilter::Any);
        m_textInput->setMaxCharCount(300);
        this->addChild(m_textInput, 10);

        m_menu = CCMenu::create();
        m_menu->setPosition({0.f, 0.f});
        m_menu->setTouchPriority(-505);
        this->addChild(m_menu, 15);

        auto clearSpr = ButtonSprite::create("X", "goldFont.fnt", "GJ_button_06.png", 0.42f);
        m_clearBtn = CCMenuItemSpriteExtra::create(clearSpr, this, menu_selector(AIChatWindow::onClearInput));
        m_menu->addChild(m_clearBtn);

        auto sendSpr = ButtonSprite::create(">>", "goldFont.fnt", "GJ_button_01.png", 0.48f);
        m_sendBtn = CCMenuItemSpriteExtra::create(sendSpr, this, menu_selector(AIChatWindow::onSend));
        m_menu->addChild(m_sendBtn);

        auto abortSpr = ButtonSprite::create("STOP", "goldFont.fnt", "GJ_button_06.png", 0.4f);
        m_abortBtn = CCMenuItemSpriteExtra::create(abortSpr, this, menu_selector(AIChatWindow::onAbort));
        m_abortBtn->setVisible(false);
        m_menu->addChild(m_abortBtn);

        updateLayoutPositions();
        return true;
    }

    void onClearInput(CCObject*) {
        if (m_textInput) m_textInput->setString("");
    }

    void updateLayoutPositions() {
        CCSize s = this->getContentSize();
        m_bg->setContentSize(s);
        m_headerBg->setContentSize({s.width, 22.f});
        m_headerBg->setPosition({0.f, s.height - 22.f});
        m_title->setPosition({10.f, s.height - 11.f});
        m_badgeLabel->setPosition({s.width / 2.f + 10.f, s.height - 11.f});
        m_chatLog->setDimensions({s.width - 16.f, s.height - 64.f});
        m_chatLog->setPosition({s.width / 2.f, s.height / 2.f + 5.f});

        float inputWidth = std::max(90.f, s.width - 78.f);
        m_textInput->setContentWidth(inputWidth);
        m_textInput->setPosition({8.f + inputWidth / 2.f, 18.f});
        m_clearBtn->setPosition({s.width - 52.f, 18.f});
        m_sendBtn->setPosition({s.width - 22.f, 18.f});
        m_abortBtn->setPosition({s.width - 26.f, s.height - 11.f});
    }
    void onSend(CCObject*) {
        if (!m_textInput) return;
        m_textInput->defocus();
        std::string userPrompt = trimStr(m_textInput->getString());
        if (userPrompt.empty() || g_aiControlling) return;

        float camX = 300.f, camY = 105.f;
        if (auto editor = LevelEditorLayer::get()) {
            if (editor->m_objectLayer) {
                camX = std::round((-editor->m_objectLayer->getPositionX() + 200.f) / 30.f) * 30.f;
                camY = std::round((-editor->m_objectLayer->getPositionY() + 105.f) / 30.f) * 30.f;
            }
        }

        std::string sysPrompt = fmt::format(
            "Ты ИИ-строитель уровней в Geometry Dash 2.2081. Центр экрана: baseX={:.0f}, baseY={:.0f} (1 клетка = 30, пол на Y=15). "
            "Отвечай ТОЛЬКО валидным JSON без markdown: "
            "{{\"reply\": \"краткий ответ\", \"objects\": [{{\"id\": 1, \"dx\": 0, \"dy\": 0, \"rot\": 0}}]}}. "
            "dx и dy — смещение в единицах (кратно 30) относительно baseX и baseY. "
            "ID: 1-блок, 8-шип, 9-мал.шип, 35-желт.батут, 36-желт.сфера, 12-куб, 13-корабль, 47-шар, 660-волна. "
            "Делай расстояния между препятствиями 90-150 по dx.", camX, camY
        );

        std::string cfIdRaw = readConfigValue("CF_ID");
        if (cfIdRaw.empty()) cfIdRaw = Mod::get()->getSettingValue<std::string>("cf-account-id");
        std::string cfId = extractHex32(cfIdRaw);

        std::string apiKey = readConfigValue("CF_KEY");
        if (apiKey.empty()) apiKey = trimStr(Mod::get()->getSettingValue<std::string>("api-key"));

        if (cfId.size() != 32 || apiKey.empty()) {
            auto cfgPath = Mod::get()->getConfigDir() / "ai_config.txt";
            m_chatLog->setString(fmt::format("Нет ключей Cloudflare!\nФайл: {}\nCF_ID: {}/32 | CF_KEY: {}",
                cfgPath.string(), cfId.size(), apiKey.empty() ? "НЕТ" : "ОК").c_str());
            return;
        }

        m_textInput->setString("");
        matjson::Value sysMsg = matjson::Value::object();
        sysMsg["role"] = "system"; sysMsg["content"] = sysPrompt;
        matjson::Value usrMsg = matjson::Value::object();
        usrMsg["role"] = "user"; usrMsg["content"] = userPrompt;
        matjson::Value messages = matjson::Value::array();
        messages.push(sysMsg); messages.push(usrMsg);

        g_aiControlling = true;
        m_abortBtn->setVisible(true);
        m_chatLog->setString(("Вы: " + userPrompt + "\n[Cloudflare строит...]").c_str());

        matjson::Value payload = matjson::Value::object();
        payload["messages"] = messages;

        std::string cfUrl = "https://cloudflare.com" + cfId + "/ai/run/@cf/meta/llama-3.1-8b-instruct";
        web::WebRequest req;
        req.userAgent("Mozilla/5.0 GeodeMod/1.0");
        req.header("Content-Type", "application/json");
        req.header("Authorization", "Bearer " + apiKey);
        req.bodyString(payload.dump());

        m_webHolder.spawn(req.post(cfUrl), [this, camX, camY](web::WebResponse res) {
            if (!g_aiControlling) return;
            std::string rawResp = res.string().unwrapOr("");
            if (res.ok() && res.json().isOk()) {
                auto data = res.json().unwrap();
                std::string aiText = data["result"]["response"].asString().unwrapOr("");
                if (aiText.empty()) aiText = data["result"]["choices"][0]["message"]["content"].asString().unwrapOr("");
                if (aiText.empty()) aiText = data["choices"][0]["message"]["content"].asString().unwrapOr("");
                if (aiText.empty()) m_chatLog->setString(("Ошибка разбора:\n" + rawResp.substr(0, 200)).c_str());
                else executeAIActions(aiText, camX, camY);
            } else if (res.ok()) {
                executeAIActions(rawResp, camX, camY);
            } else {
                m_chatLog->setString(fmt::format("CF Ошибка {}: {}\n{}", res.code(), res.errorMessage(), rawResp.substr(0, 110)).c_str());
            }
            finishAIControl();
        });
    }
    void executeAIActions(const std::string& rawText, float baseX, float baseY) {
        auto start = rawText.find('{');
        auto end = rawText.rfind('}');
        if (start == std::string::npos || end == std::string::npos || end <= start) {
            m_chatLog->setString(("ИИ: " + rawText.substr(0, 180)).c_str());
            return;
        }
        auto parsed = matjson::parse(rawText.substr(start, end - start + 1));
        if (!parsed.isOk()) {
            m_chatLog->setString(("ИИ: " + rawText.substr(0, 180)).c_str());
            return;
        }
        auto root = parsed.unwrap();
        std::string replyText = root["reply"].asString().unwrapOr("Готово!");
        auto editor = LevelEditorLayer::get();
        int spawnedCount = 0;

        if (editor && root["objects"].isArray()) {
            for (auto& objData : root["objects"].asArray().unwrap()) {
                if (!g_aiControlling) break;
                int objId = objData["id"].asInt().unwrapOr(1);
                float posX = baseX + static_cast<float>(objData["dx"].asDouble().unwrapOr(0.0));
                float posY = baseY + static_cast<float>(objData["dy"].asDouble().unwrapOr(0.0));
                if (objData.contains("x") && !objData.contains("dx")) posX = static_cast<float>(objData["x"].asDouble().unwrapOr(baseX));
                if (objData.contains("y") && !objData.contains("dy")) posY = static_cast<float>(objData["y"].asDouble().unwrapOr(baseY));
                if (posY < 15.f) posY = 15.f;

                if (auto gameObj = editor->createObject(objId, {posX, posY}, false)) {
                    gameObj->setRotation(static_cast<float>(objData["rot"].asDouble().unwrapOr(0.0)));
                    spawnedCount++;
                }
            }
        }
        m_chatLog->setString(spawnedCount > 0
            ? fmt::format("ИИ: {} (объектов: {})", replyText, spawnedCount).c_str()
            : ("ИИ: " + replyText).c_str());
    }

    void finishAIControl() { g_aiControlling = false; if (m_abortBtn) m_abortBtn->setVisible(false); }
    void onAbort(CCObject*) { m_webHolder.cancel(); finishAIControl(); m_chatLog->setString("Запрос прерван."); }

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        if (!this->isVisible()) return false;
        CCPoint pos = this->convertTouchToNodeSpace(touch);
        CCSize s = this->getContentSize();
        if (pos.x < 0 || pos.y < 0 || pos.x > s.width || pos.y > s.height) { if (m_textInput) m_textInput->defocus(); return false; }
        if (pos.y < 36.f && pos.x < s.width - 18.f) return false;
        if (m_textInput) m_textInput->defocus();
        m_dragStart = touch->getLocation();
        if (pos.x > s.width - 22.f && pos.y < 22.f) m_isResizing = true;
        else if (pos.y > s.height - 22.f) m_isDragging = true;
        return true;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent*) override {
        CCPoint delta = touch->getLocation() - m_dragStart;
        m_dragStart = touch->getLocation();
        if (m_isDragging) this->setPosition(this->getPosition() + delta);
        else if (m_isResizing) {
            CCSize s = this->getContentSize();
            this->setContentSize({std::max(210.f, s.width + delta.x), std::max(135.f, s.height - delta.y)});
            updateLayoutPositions();
        }
    }
    void ccTouchEnded(CCTouch*, CCEvent*) override { m_isDragging = false; m_isResizing = false; }
};

class $modify(AIEditorLayerHook, LevelEditorLayer) {
    bool ccTouchBegan(CCTouch* touch, CCEvent* event) {
        if (g_aiControlling) return false;
        return LevelEditorLayer::ccTouchBegan(touch, event);
    }
};

class $modify(AIEditorUIHook, EditorUI) {
    struct Fields { AIChatWindow* chatWindow = nullptr; CCSprite* crossSprite = nullptr; CCMenu* btnMenu = nullptr; };

    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;
        g_aiControlling = false;
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto brainSpr = CCSprite::create("brain.png"_spr);
        if (brainSpr) {
            auto dummyNode = CCNode::create(); dummyNode->setContentSize({30.f, 30.f});
            auto circleBtnSpr = CircleButtonSprite::create(dummyNode, CircleBaseColor::Green, CircleBaseSize::Small);
            circleBtnSpr->setScale(0.85f);
            brainSpr->setPosition(circleBtnSpr->getContentSize() / 2.f);
            if (brainSpr->getContentSize().width > 0) brainSpr->setScale(22.f / brainSpr->getContentSize().width);
            circleBtnSpr->addChild(brainSpr, 5);

            m_fields->crossSprite = CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png");
            if (m_fields->crossSprite) {
                m_fields->crossSprite->setPosition(circleBtnSpr->getContentSize() / 2.f);
                m_fields->crossSprite->setScale(0.8f); m_fields->crossSprite->setVisible(false);
                circleBtnSpr->addChild(m_fields->crossSprite, 10);
            }
            auto aiBtn = CCMenuItemSpriteExtra::create(circleBtnSpr, this, menu_selector(AIEditorUIHook::onToggleAIChat));
            m_fields->btnMenu = CCMenu::create(); m_fields->btnMenu->setPosition({195.f, winSize.height - 23.f});
            m_fields->btnMenu->addChild(aiBtn); this->addChild(m_fields->btnMenu, 100);
        }
        return true;
    }

    void onPlaytest(CCObject* sender) {
        EditorUI::onPlaytest(sender);
        if (m_fields->btnMenu) m_fields->btnMenu->setVisible(false);
        if (m_fields->chatWindow) m_fields->chatWindow->setVisible(false);
        if (m_fields->crossSprite) m_fields->crossSprite->setVisible(false);
    }

    void onStopPlaytest(CCObject* sender) { EditorUI::onStopPlaytest(sender); if (m_fields->btnMenu) m_fields->btnMenu->setVisible(true); }

    void onToggleAIChat(CCObject*) {
        bool nowVisible = false;
        if (!m_fields->chatWindow) {
            m_fields->chatWindow = AIChatWindow::create(); m_fields->chatWindow->setPosition({110.f, 95.f});
            this->addChild(m_fields->chatWindow, 500); nowVisible = true;
        } else {
            nowVisible = !m_fields->chatWindow->isVisible(); m_fields->chatWindow->setVisible(nowVisible);
        }
        if (m_fields->crossSprite) m_fields->crossSprite->setVisible(nowVisible);
    }
};
