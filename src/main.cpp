#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
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

std::string captureEditorBase64() {
    auto editor = LevelEditorLayer::get();
    if (!editor) return "";
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    auto rt = CCRenderTexture::create(static_cast<int>(winSize.width), static_cast<int>(winSize.height));
    if (!rt) return "";
    rt->beginWithClear(0.f, 0.f, 0.f, 1.f);
    editor->visit();
    rt->end();
    auto tempPath = Mod::get()->getSaveDir() / "ai_screen.png";
    if (!rt->saveToFile(tempPath.string().c_str(), kCCImageFormatPNG)) return "";
    std::ifstream file(tempPath, std::ios::binary);
    if (!file) return "";
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, valb = -6;
    for (uint8_t c : bytes) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) { out.push_back(chars[(val >> valb) & 0x3F]); valb -= 6; }
    }
    if (valb > -6) out.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}
class AIChatWindow : public CCLayer {
protected:
    CCPoint m_dragStart;
    bool m_isDragging = false;
    bool m_isResizing = false;
    bool m_useCloudflare = false; // false = Pollinations (без ключа), true = Cloudflare

    CCLayerColor* m_bg = nullptr;
    CCLayerColor* m_headerBg = nullptr;
    CCLayerColor* m_inputBg = nullptr;
    CCLabelTTF* m_title = nullptr;
    CCLabelTTF* m_chatLog = nullptr;
    CCTextFieldTTF* m_textField = nullptr;
    CCMenu* m_menu = nullptr;
    CCMenuItemSpriteExtra* m_sendBtn = nullptr;
    CCMenuItemSpriteExtra* m_abortBtn = nullptr;
    CCMenuItemSpriteExtra* m_providerBtn = nullptr;
    ButtonSprite* m_providerSpr = nullptr;
    async::TaskHolder<web::WebResponse> m_webHolder;

public:
    static AIChatWindow* create() {
        auto ret = new AIChatWindow();
        if (ret && ret->init()) { ret->autorelease(); return ret; }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init() override {
        if (!CCLayer::init()) return false;
        ensureConfigTemplate();
        this->setContentSize({230.f, 165.f});
        this->setTouchEnabled(true);
        this->setTouchMode(kCCTouchesOneByOne);
        this->setTouchPriority(-500);

        m_bg = CCLayerColor::create({18, 20, 32, 235}, 230.f, 165.f);
        this->addChild(m_bg, 0);
        m_headerBg = CCLayerColor::create({32, 36, 56, 255}, 230.f, 22.f);
        this->addChild(m_headerBg, 1);
        m_inputBg = CCLayerColor::create({35, 38, 52, 255}, 168.f, 24.f);
        this->addChild(m_inputBg, 2);

        m_title = CCLabelTTF::create("ИИ Чат", "ru_font.ttf"_spr, 11);
        this->addChild(m_title, 5);
        m_chatLog = CCLabelTTF::create("Pollinations готов (ключ не нужен)! Пиши по-русски:", "ru_font.ttf"_spr, 10, {214.f, 95.f}, kCCTextAlignmentLeft);
        this->addChild(m_chatLog, 5);

        m_textField = CCTextFieldTTF::textFieldWithPlaceHolder("Нажми сюда и пиши...", {160.f, 20.f}, kCCTextAlignmentLeft, "ru_font.ttf"_spr, 11.f);
        m_textField->setColorSpaceHolder({150, 150, 170});
        m_textField->setColor({255, 255, 255});
        this->addChild(m_textField, 10);

        m_menu = CCMenu::create();
        m_menu->setPosition({0.f, 0.f});
        m_menu->setTouchPriority(-505);
        this->addChild(m_menu, 15);

        auto sendSpr = ButtonSprite::create(">>", "goldFont.fnt", "GJ_button_01.png", 0.5f);
        m_sendBtn = CCMenuItemSpriteExtra::create(sendSpr, this, menu_selector(AIChatWindow::onSend));
        m_menu->addChild(m_sendBtn);

        auto abortSpr = ButtonSprite::create("STOP", "goldFont.fnt", "GJ_button_06.png", 0.42f);
        m_abortBtn = CCMenuItemSpriteExtra::create(abortSpr, this, menu_selector(AIChatWindow::onAbort));
        m_abortBtn->setVisible(false);
        m_menu->addChild(m_abortBtn);

        m_providerSpr = ButtonSprite::create("Pollinations", "goldFont.fnt", "GJ_button_04.png", 0.4f);
        m_providerBtn = CCMenuItemSpriteExtra::create(m_providerSpr, this, menu_selector(AIChatWindow::onSwitchProvider));
        m_menu->addChild(m_providerBtn);

        updateLayoutPositions();
        return true;
    }

    void onSwitchProvider(CCObject*) {
        m_useCloudflare = !m_useCloudflare;
        m_providerSpr->setString(m_useCloudflare ? "Cloudflare" : "Pollinations");
        m_chatLog->setString(m_useCloudflare ? "Выбран Cloudflare (со скриншотом экрана)" : "Выбран Pollinations (бесплатно со скриншотом)");
    }

    void updateLayoutPositions() {
        CCSize s = this->getContentSize();
        m_bg->setContentSize(s);
        m_headerBg->setContentSize({s.width, 22.f});
        m_headerBg->setPosition({0.f, s.height - 22.f});
        m_title->setPosition({32.f, s.height - 11.f});
        m_providerBtn->setPosition({s.width / 2.f + 12.f, s.height - 11.f});
        m_chatLog->setDimensions({s.width - 16.f, s.height - 62.f});
        m_chatLog->setPosition({s.width / 2.f, s.height / 2.f + 4.f});
        float inputWidth = s.width - 58.f;
        m_inputBg->setContentSize({inputWidth, 24.f});
        m_inputBg->setPosition({8.f, 6.f});
        m_textField->setDimensions({inputWidth - 8.f, 18.f});
        m_textField->setPosition({inputWidth / 2.f + 10.f, 17.f});
        m_sendBtn->setPosition({s.width - 24.f, 18.f});
        m_abortBtn->setPosition({s.width - 24.f, s.height - 11.f});
    }
    void onSend(CCObject*) {
        m_textField->detachWithIME();
        std::string userPrompt = m_textField->getString() ? m_textField->getString() : "";
        if (userPrompt.empty() || g_aiControlling) return;

        float camX = 300.f, camY = 105.f;
        if (auto editor = LevelEditorLayer::get()) {
            if (editor->m_objectLayer) {
                camX = -editor->m_objectLayer->getPositionX() + 200.f;
                camY = -editor->m_objectLayer->getPositionY() + 100.f;
            }
        }

        std::string sysPrompt = fmt::format(
            "Ты ИИ-строитель уровней в Geometry Dash 2.208. Камера X={:.0f}, Y={:.0f} (1 блок=30). "
            "Отвечай ТОЛЬКО валидным JSON без markdown: {{\"reply\": \"ответ\", \"objects\": [{{\"id\": 1, \"x\": 300, \"y\": 105, \"rot\": 0}}]}}. "
            "ID: 1-блок, 8-шип, 9-мал.шип, 35-желт.батут, 36-желт.сфера, 12-куб, 13-корабль.", camX, camY
        );

        std::string b64 = captureEditorBase64();

        matjson::Value contentArr = matjson::Value::array();
        contentArr.push(matjson::makeObject({{"type", "text"}, {"text", userPrompt}}));
        if (!b64.empty()) {
            contentArr.push(matjson::makeObject({{"type", "image_url"}, {"image_url", matjson::makeObject({{"url", "data:image/png;base64," + b64}})}}));
        }

        matjson::Value messages = matjson::Value::array();
        messages.push(matjson::makeObject({{"role", "system"}, {"content", sysPrompt}}));
        messages.push(matjson::makeObject({{"role", "user"}, {"content", userPrompt}}));

        if (!m_useCloudflare) {
            g_aiControlling = true;
            m_abortBtn->setVisible(true);
            m_chatLog->setString(("Вы: " + userPrompt + "\n[Скриншот сделан, Pollinations думает...]").c_str());

            std::string pollUrl = std::string("https://") + "text.pollinations." + "ai/";
            matjson::Value payload = matjson::makeObject({
                {"messages", messages},
                {"model", "openai"},
                {"jsonMode", true}
            });

            web::WebRequest req;
            req.userAgent("Mozilla/5.0 GeodeMod/1.0");
            req.header("Content-Type", "application/json");
            req.bodyJSON(payload);

            m_webHolder.spawn(req.post(pollUrl), [this](web::WebResponse res) {
                if (!g_aiControlling) return;
                std::string rawResp = res.string().unwrapOr("");
                if (res.ok() && !rawResp.empty()) {
                    executeAIActions(rawResp);
                } else {
                    std::string errMsg(res.errorMessage());
                    m_chatLog->setString(fmt::format("Pollinations Код {}: {}\n{}", res.code(), errMsg, rawResp.substr(0, 90)).c_str());
                }
                finishAIControl();
            });
            return;
        }

        std::string cfIdRaw = readConfigValue("CF_ID");
        if (cfIdRaw.empty()) cfIdRaw = Mod::get()->getSettingValue<std::string>("cf-account-id");
        std::string cfId = extractHex32(cfIdRaw);

        std::string apiKey = readConfigValue("CF_KEY");
        if (apiKey.empty()) apiKey = trimStr(Mod::get()->getSettingValue<std::string>("api-key"));

        if (cfId.size() != 32 || apiKey.empty()) {
            m_chatLog->setString("Укажи 32-значный CF_ID и CF_KEY в ai_config.txt!");
            return;
        }

        g_aiControlling = true;
        m_abortBtn->setVisible(true);
        m_chatLog->setString(("Вы: " + userPrompt + "\n[Скриншот сделан, Cloudflare думает...]").c_str());

        std::string cfUrl = std::string("https://") + "api.cloudflare." + "com/client/v4/accounts/" + cfId + "/ai/run/@cf/meta/llama-3.2-11b-vision-instruct";
        matjson::Value payload = matjson::makeObject({{"messages", messages}});

        web::WebRequest req;
        req.userAgent("Mozilla/5.0 GeodeMod/1.0");
        req.header("Content-Type", "application/json");
        req.header("Authorization", "Bearer " + apiKey);
        req.bodyJSON(payload);

        m_webHolder.spawn(req.post(cfUrl), [this](web::WebResponse res) {
            if (!g_aiControlling) return;
            std::string rawResp = res.string().unwrapOr("");
            if (res.ok()) {
                auto jsonRes = res.json();
                if (jsonRes.isOk()) {
                    auto data = jsonRes.unwrap();
                    std::string aiText = data["result"]["response"].asString().unwrapOr("");
                    if (aiText.empty()) m_chatLog->setString(("Ответ: " + rawResp.substr(0, 110)).c_str());
                    else executeAIActions(aiText);
                } else executeAIActions(rawResp);
            } else {
                std::string errMsg(res.errorMessage());
                m_chatLog->setString(fmt::format("CF Код {}: {}\n{}", res.code(), errMsg, rawResp.substr(0, 90)).c_str());
            }
            finishAIControl();
        });
    }
    void executeAIActions(std::string rawText) {
        auto start = rawText.find('{');
        auto end = rawText.rfind('}');
        if (start == std::string::npos || end == std::string::npos) { m_chatLog->setString(("ИИ: " + rawText.substr(0, 120)).c_str()); return; }
        auto parsed = matjson::parse(rawText.substr(start, end - start + 1));
        if (!parsed.isOk()) { m_chatLog->setString(("ИИ: " + rawText.substr(0, 120)).c_str()); return; }

        auto root = parsed.unwrap();
        m_chatLog->setString(("ИИ: " + root["reply"].asString().unwrapOr("Готово!")).c_str());
        auto editor = LevelEditorLayer::get();
        if (!editor) return;

        if (root["objects"].isArray()) {
            for (auto& objData : root["objects"].asArray().unwrap()) {
                if (!g_aiControlling) break;
                if (auto gameObj = editor->createObject(objData["id"].asInt().unwrapOr(1), {static_cast<float>(objData["x"].asDouble().unwrapOr(0.0)), static_cast<float>(objData["y"].asDouble().unwrapOr(0.0))}, false)) {
                    gameObj->setRotation(static_cast<float>(objData["rot"].asDouble().unwrapOr(0.0)));
                }
            }
        }
    }

    void finishAIControl() { g_aiControlling = false; m_abortBtn->setVisible(false); }
    void onAbort(CCObject*) { m_webHolder.cancel(); finishAIControl(); m_chatLog->setString("Прервано!"); }

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        if (!this->isVisible()) return false;
        CCPoint pos = this->convertTouchToNodeSpace(touch);
        CCSize s = this->getContentSize();
        if (pos.x < 0 || pos.y < 0 || pos.x > s.width || pos.y > s.height) {
            m_textField->detachWithIME();
            return false;
        }
        if (pos.y < 36.f && pos.x < s.width - 42.f) {
            m_textField->attachWithIME();
            return true;
        }
        m_textField->detachWithIME();
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
            this->setContentSize({std::max(180.f, s.width + delta.x), std::max(130.f, s.height - delta.y)});
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
    struct Fields {
        AIChatWindow* chatWindow = nullptr;
        CCSprite* crossSprite = nullptr;
        CCMenu* btnMenu = nullptr;
    };

    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;
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
