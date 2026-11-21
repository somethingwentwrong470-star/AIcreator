#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/utils/web.hpp>
#include <fstream>
#include <vector>

using namespace geode::prelude;

bool g_aiControlling = false;

std::string encodeFileToBase64(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, valb = -6;
    for (uint8_t c : bytes) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

std::string captureEditorScreenshotBase64() {
    auto editor = LevelEditorLayer::get();
    if (!editor) return "";

    auto winSize = CCDirector::sharedDirector()->getWinSize();
    auto rt = CCRenderTexture::create(static_cast<int>(winSize.width), static_cast<int>(winSize.height));
    if (!rt) return "";

    rt->beginWithClear(0.f, 0.f, 0.f, 1.f);
    editor->visit();
    rt->end();

    auto tempPath = Mod::get()->getSaveDir() / "ai_screen.png";
    if (rt->saveToFile(tempPath.string().c_str(), kCCImageFormatPNG)) {
        return encodeFileToBase64(tempPath);
    }
    return "";
}
class AIChatWindow : public CCLayer {
protected:
    CCPoint m_dragStart;
    bool m_isDragging = false;
    bool m_isResizing = false;
    int m_providerIndex = 0;

    CCLayerColor* m_bg = nullptr;
    CCLayerColor* m_headerBg = nullptr;
    CCLabelTTF* m_title = nullptr;
    CCLabelTTF* m_chatLog = nullptr;
    CCLabelTTF* m_previewLabel = nullptr;
    TextInput* m_input = nullptr;
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

    const char* getProviderName() const {
        const char* names[] = {"Cloudflare", "Pollinations", "VseGPT", "OpenRouter"};
        return names[m_providerIndex % 4];
    }

    bool init() override {
        if (!CCLayer::init()) return false;
        this->setContentSize({230.f, 175.f});
        this->setTouchEnabled(true);
        this->setTouchMode(kCCTouchesOneByOne);
        this->setTouchPriority(-500);

        m_bg = CCLayerColor::create({18, 20, 32, 235}, 230.f, 175.f);
        this->addChild(m_bg, 0);
        m_headerBg = CCLayerColor::create({32, 36, 56, 255}, 230.f, 22.f);
        this->addChild(m_headerBg, 1);

        m_title = CCLabelTTF::create("ИИ Чат", "ru_font.ttf"_spr, 11);
        this->addChild(m_title, 5);
        m_chatLog = CCLabelTTF::create("Выбери ИИ сверху и пиши запрос!", "ru_font.ttf"_spr, 10, {214.f, 82.f}, kCCTextAlignmentLeft);
        this->addChild(m_chatLog, 5);

        m_previewLabel = CCLabelTTF::create("Ввод: ", "ru_font.ttf"_spr, 9, {214.f, 16.f}, kCCTextAlignmentLeft);
        m_previewLabel->setColor({140, 220, 255});
        this->addChild(m_previewLabel, 8);

        // Полностью видимый и нажимаемый TextInput
        m_input = TextInput::create(160.f, "Tap here...");
        m_input->setCommonFilter(CommonFilter::Any);
        m_input->setMaxCharCount(150);
        m_input->setScale(0.95f);
        this->addChild(m_input, 20);

        m_input->setCallback([this](std::string const& str) {
            m_previewLabel->setString(("Ввод: " + str).c_str());
        });

        m_menu = CCMenu::create();
        m_menu->setPosition({0.f, 0.f});
        m_menu->setTouchPriority(-505);
        this->addChild(m_menu, 25);

        auto sendSpr = ButtonSprite::create(">>", "goldFont.fnt", "GJ_button_01.png", 0.5f);
        m_sendBtn = CCMenuItemSpriteExtra::create(sendSpr, this, menu_selector(AIChatWindow::onSend));
        m_menu->addChild(m_sendBtn);

        auto abortSpr = ButtonSprite::create("STOP", "goldFont.fnt", "GJ_button_06.png", 0.42f);
        m_abortBtn = CCMenuItemSpriteExtra::create(abortSpr, this, menu_selector(AIChatWindow::onAbort));
        m_abortBtn->setVisible(false);
        m_menu->addChild(m_abortBtn);

        m_providerSpr = ButtonSprite::create(getProviderName(), "goldFont.fnt", "GJ_button_04.png", 0.4f);
        m_providerBtn = CCMenuItemSpriteExtra::create(m_providerSpr, this, menu_selector(AIChatWindow::onSwitchProvider));
        m_menu->addChild(m_providerBtn);

        updateLayoutPositions();
        return true;
    }

    void onSwitchProvider(CCObject*) {
        m_providerIndex = (m_providerIndex + 1) % 4;
        m_providerSpr->setString(getProviderName());
        m_chatLog->setString(fmt::format("Выбран сервис: {}", getProviderName()).c_str());
    }

    void updateLayoutPositions() {
        CCSize s = this->getContentSize();
        m_bg->setContentSize(s);
        m_headerBg->setContentSize({s.width, 22.f});
        m_headerBg->setPosition({0.f, s.height - 22.f});
        m_title->setPosition({32.f, s.height - 11.f});
        m_providerBtn->setPosition({s.width / 2.f + 12.f, s.height - 11.f});
        m_chatLog->setDimensions({s.width - 16.f, s.height - 80.f});
        m_chatLog->setPosition({s.width / 2.f, s.height / 2.f + 12.f});
        m_previewLabel->setDimensions({s.width - 16.f, 16.f});
        m_previewLabel->setPosition({s.width / 2.f, 40.f});
        float inputWidth = s.width - 58.f;
        m_input->setPosition({inputWidth / 2.f + 10.f, 18.f});
        m_sendBtn->setPosition({s.width - 24.f, 18.f});
        m_abortBtn->setPosition({s.width - 24.f, s.height - 11.f});
    }
    void onSend(CCObject*) {
        std::string userPrompt = m_input->getString();
        if (userPrompt.empty() || g_aiControlling) return;

        std::string cfId = Mod::get()->getSettingValue<std::string>("cf-account-id");
        std::string apiKey = Mod::get()->getSettingValue<std::string>("api-key");
        std::string apiUrl = "", model = "";

        if (m_providerIndex == 0) {
            if (cfId.empty()) { m_chatLog->setString("Ошибка: Укажи Cloudflare Account ID в настройках!"); return; }
            apiUrl = "https://cloudflare.com" + cfId + "/ai/v1/chat/completions";
            model = "@cf/meta/llama-3.2-11b-vision-instruct";
        } else if (m_providerIndex == 1) {
            apiUrl = "https://pollinations.ai";
            model = "openai";
            if (apiKey.empty()) apiKey = "free";
        } else if (m_providerIndex == 2) {
            apiUrl = "https://vsegpt.ru";
            model = "openai/gpt-4o-mini";
        } else {
            apiUrl = "https://openrouter.ai";
            model = "openrouter/free";
        }

        if (apiKey.empty()) { m_chatLog->setString("Ошибка: Укажи API Ключ в настройках!"); return; }

        g_aiControlling = true;
        m_abortBtn->setVisible(true);
        m_input->setString("");
        m_previewLabel->setString("Ввод: ");
        m_chatLog->setString(("Вы: " + userPrompt + "\n[Делаю скриншот и думаю...]").c_str());

        std::string base64Img = captureEditorScreenshotBase64();
        float camX = 300.f, camY = 105.f;
        if (auto editor = LevelEditorLayer::get()) {
            if (editor->m_objectLayer) {
                camX = -editor->m_objectLayer->getPositionX() + 200.f;
                camY = -editor->m_objectLayer->getPositionY() + 100.f;
            }
        }

        std::string sysPrompt = fmt::format(
            "Ты ИИ-строитель уровней в Geometry Dash 2.208. Камера X={:.0f}, Y={:.0f}. "
            "Отвечай ТОЛЬКО валидным JSON без markdown: {{\"reply\": \"ответ\", \"objects\": [{{\"id\": 1, \"x\": 300, \"y\": 105, \"rot\": 0}}]}}. "
            "ID: 1-блок, 8-шип, 9-мал.шип, 35-желт.батут, 36-желт.сфера, 12-куб, 13-корабль.", camX, camY
        );

        matjson::Value contentArr = matjson::Value::array();
        contentArr.push(matjson::makeObject({{"type", "text"}, {"text", userPrompt}}));
        if (!base64Img.empty()) {
            contentArr.push(matjson::makeObject({{"type", "image_url"}, {"image_url", matjson::makeObject({{"url", "data:image/png;base64," + base64Img}})}}));
        }

        matjson::Value messages = matjson::Value::array();
        messages.push(matjson::makeObject({{"role", "system"}, {"content", sysPrompt}}));
        messages.push(matjson::makeObject({{"role", "user"}, {"content", contentArr}}));

        matjson::Value payload = matjson::Value::array();
        payload = matjson::makeObject({{"model", model}, {"messages", messages}, {"max_tokens", 1000}});

        web::WebRequest req;
        req.header("Content-Type", "application/json");
        req.header("Authorization", "Bearer " + apiKey);
        req.bodyJSON(payload);

        m_webHolder.spawn(req.post(apiUrl), [this](web::WebResponse res) {
            if (!g_aiControlling) return;
            if (res.ok()) {
                auto jsonRes = res.json();
                if (jsonRes.isOk()) {
                    auto data = jsonRes.unwrap();
                    std::string aiText = "";
                    if (data["choices"].isArray() && !data["choices"].asArray().unwrap().empty()) {
                        aiText = data["choices"]["message"]["content"].asString().unwrapOr("");
                    }
                    if (aiText.empty()) m_chatLog->setString("Пустой ответ от ИИ.");
                    else executeAIActions(aiText);
                } else m_chatLog->setString("Ошибка чтения JSON ответа.");
            } else {
                std::string errBody = res.string().unwrapOr("");
                std::string errMsg(res.errorMessage());
                m_chatLog->setString(fmt::format("Код {}: {}\n{}", res.code(), errMsg, errBody.substr(0, 90)).c_str());
            }
            finishAIControl();
        });
    }

    void executeAIActions(std::string rawText) {
        auto start = rawText.find('{');
        auto end = rawText.rfind('}');
        if (start == std::string::npos || end == std::string::npos) { m_chatLog->setString(("ИИ: " + rawText).c_str()); return; }
        auto parsed = matjson::parse(rawText.substr(start, end - start + 1));
        if (!parsed.isOk()) { m_chatLog->setString(("ИИ: " + rawText).c_str()); return; }

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
        if (pos.x < 0 || pos.y < 0 || pos.x > s.width || pos.y > s.height) return false;
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
            auto circleBg = CCSprite::createWithSpriteFrameName("GJ_button_01.png");
            circleBg->setScale(0.62f);

            brainSpr->setPosition(circleBg->getContentSize() / 2.f);
            brainSpr->setScale(0.14f);
            circleBg->addChild(brainSpr, 5);

            m_fields->crossSprite = CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png");
            if (m_fields->crossSprite) {
                m_fields->crossSprite->setPosition(circleBg->getContentSize() / 2.f);
                m_fields->crossSprite->setScale(0.85f);
                m_fields->crossSprite->setVisible(false);
                circleBg->addChild(m_fields->crossSprite, 10);
            }

            auto aiBtn = CCMenuItemSpriteExtra::create(circleBg, this, menu_selector(AIEditorUIHook::onToggleAIChat));
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
