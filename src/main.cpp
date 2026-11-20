э#include <Geode/Geode.hpp>
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
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    auto rt = CCRenderTexture::create(static_cast<int>(winSize.width), static_cast<int>(winSize.height));
    if (!rt) return "";

    rt->begin();
    CCDirector::sharedDirector()->getRunningScene()->visit();
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
    
    CCLayerColor* m_bg = nullptr;
    CCLayerColor* m_headerBg = nullptr;
    CCLayerColor* m_inputBg = nullptr;
    CCLabelTTF* m_title = nullptr;
    CCLabelTTF* m_chatLog = nullptr;
    CCTextFieldTTF* m_textField = nullptr;
    CCMenu* m_menu = nullptr;
    CCMenuItemSpriteExtra* m_sendBtn = nullptr;
    CCMenuItemSpriteExtra* m_abortBtn = nullptr;
    
    async::TaskHolder<web::WebResponse> m_webHolder;

public:
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

        this->setContentSize({230.f, 165.f});
        this->setTouchEnabled(true);
        this->setTouchMode(kCCTouchesOneByOne);
        this->setTouchPriority(-500);

        m_bg = CCLayerColor::create({18, 20, 32, 235}, 230.f, 165.f);
        this->addChild(m_bg, 0);

        m_headerBg = CCLayerColor::create({32, 36, 56, 255}, 230.f, 22.f);
        this->addChild(m_headerBg, 1);

        // Тёмно-серая плашка под поле ввода
        m_inputBg = CCLayerColor::create({35, 38, 52, 255}, 168.f, 24.f);
        this->addChild(m_inputBg, 2);

        m_title = CCLabelTTF::create("ИИ Ассистент", "ru_font.ttf"_spr, 11);
        this->addChild(m_title, 5);

        m_chatLog = CCLabelTTF::create(
            "Напиши запрос на русском (например: 'построй тройной шип').",
            "ru_font.ttf"_spr, 10, {214.f, 95.f}, kCCTextAlignmentLeft
        );
        this->addChild(m_chatLog, 5);

        // Нативное поле ввода с поддержкой русского TTF-шрифта и клавиатуры Android
        m_textField = CCTextFieldTTF::textFieldWithPlaceHolder(
            "Нажми сюда и пиши...", {160.f, 20.f}, kCCTextAlignmentLeft, "ru_font.ttf"_spr, 11.f
        );
        m_textField->setColorSpaceHolder({150, 150, 170});
        m_textField->setColor({255, 255, 255});
        this->addChild(m_textField, 10);

        m_menu = CCMenu::create();
        m_menu->setPosition({0.f, 0.f});
        this->addChild(m_menu, 15);

        auto sendSpr = ButtonSprite::create(">>", "goldFont.fnt", "GJ_button_01.png", 0.5f);
        m_sendBtn = CCMenuItemSpriteExtra::create(sendSpr, this, menu_selector(AIChatWindow::onSend));
        m_menu->addChild(m_sendBtn);

        auto abortSpr = ButtonSprite::create("Прервать", "goldFont.fnt", "GJ_button_06.png", 0.45f);
        m_abortBtn = CCMenuItemSpriteExtra::create(abortSpr, this, menu_selector(AIChatWindow::onAbort));
        m_abortBtn->setVisible(false);
        m_menu->addChild(m_abortBtn);

        updateLayoutPositions();
        return true;
    }

    void updateLayoutPositions() {
        CCSize s = this->getContentSize();
        m_bg->setContentSize(s);
        m_headerBg->setContentSize({s.width, 22.f});
        m_headerBg->setPosition({0.f, s.height - 22.f});
        
        m_title->setPosition({s.width / 2.f, s.height - 11.f});
        m_chatLog->setDimensions({s.width - 16.f, s.height - 62.f});
        m_chatLog->setPosition({s.width / 2.f, s.height / 2.f + 4.f});
        
        float inputWidth = s.width - 58.f;
        m_inputBg->setContentSize({inputWidth, 24.f});
        m_inputBg->setPosition({8.f, 6.f});
        
        m_textField->setDimensions({inputWidth - 8.f, 18.f});
        m_textField->setPosition({inputWidth / 2.f + 10.f, 17.f});
        
        m_sendBtn->setPosition({s.width - 24.f, 18.f});
        m_abortBtn->setPosition({s.width - 38.f, s.height - 11.f});
    }

    void onSend(CCObject*) {
        m_textField->detachWithIME();
        std::string userPrompt = m_textField->getString() ? m_textField->getString() : "";
        if (userPrompt.empty() || g_aiControlling) return;

        std::string apiUrl = Mod::get()->getSettingValue<std::string>("api-url");
        std::string apiKey = Mod::get()->getSettingValue<std::string>("api-key");
        std::string model = Mod::get()->getSettingValue<std::string>("model-name");

        if (apiKey.empty()) {
            m_chatLog->setString("Ошибка: Укажи API Ключ в настройках мода Geode!");
            return;
        }

        g_aiControlling = true;
        m_abortBtn->setVisible(true);
        m_textField->setString("");
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
            "Ты ИИ-строитель уровней в Geometry Dash 2.208. Текущая камера в районе X={:.0f}, Y={:.0f} (1 блок = 30 единиц). "
            "Отвечай ТОЛЬКО валидным JSON без markdown-обертки в формате: "
            "{{\"reply\": \"короткий ответ на русском\", \"objects\": [{{\"id\": 1, \"x\": 300, \"y\": 105, \"rot\": 0}}]}}. "
            "Популярные ID: 1 - блок, 8 - шип, 9 - маленький шип, 35 - желтый батут, 36 - желтая сфера, 12 - куб портал, 13 - кораблик портал.",
            camX, camY
        );

        matjson::Value contentArr = matjson::Value::array();
        contentArr.push(matjson::makeObject({
            {"type", "text"},
            {"text", userPrompt}
        }));
        if (!base64Img.empty()) {
            contentArr.push(matjson::makeObject({
                {"type", "image_url"},
                {"image_url", matjson::makeObject({
                    {"url", "data:image/png;base64," + base64Img}
                })}
            }));
        }

        matjson::Value messages = matjson::Value::array();
        messages.push(matjson::makeObject({{"role", "system"}, {"content", sysPrompt}}));
        messages.push(matjson::makeObject({{"role", "user"}, {"content", contentArr}}));

        matjson::Value payload = matjson::makeObject({
            {"model", model},
            {"messages", messages},
            {"max_tokens", 1000}
        });

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
                    std::string aiText = data["choices"]["message"]["content"].asString().unwrapOr("");
                    executeAIActions(aiText);
                } else {
                    m_chatLog->setString("Ошибка чтения ответа от сервера.");
                }
            } else {
                m_chatLog->setString(fmt::format("Ошибка API (Код {}): проверь URL и ключ.", res.code()).c_str());
            }
            finishAIControl();
        });
    }
    void executeAIActions(std::string rawText) {
        auto start = rawText.find('{');
        auto end = rawText.rfind('}');
        if (start == std::string::npos || end == std::string::npos) {
            m_chatLog->setString(("ИИ: " + rawText).c_str());
            return;
        }
        std::string cleanJson = rawText.substr(start, end - start + 1);

        auto parsed = matjson::parse(cleanJson);
        if (!parsed.isOk()) {
            m_chatLog->setString(("ИИ: " + rawText).c_str());
            return;
        }

        auto root = parsed.unwrap();
        std::string reply = root["reply"].asString().unwrapOr("Готово!");
        m_chatLog->setString(("ИИ: " + reply).c_str());

        auto editor = LevelEditorLayer::get();
        if (!editor) return;

        if (root["objects"].isArray()) {
            for (auto& objData : root["objects"].asArray().unwrap()) {
                if (!g_aiControlling) break;
                int id = objData["id"].asInt().unwrapOr(1);
                float x = static_cast<float>(objData["x"].asDouble().unwrapOr(0.0));
                float y = static_cast<float>(objData["y"].asDouble().unwrapOr(0.0));
                float rot = static_cast<float>(objData["rot"].asDouble().unwrapOr(0.0));

                if (auto gameObj = editor->createObject(id, {x, y}, false)) {
                    gameObj->setRotation(rot);
                }
            }
        }
    }

    void finishAIControl() {
        g_aiControlling = false;
        m_abortBtn->setVisible(false);
    }

    void onAbort(CCObject*) {
        m_webHolder.cancel();
        finishAIControl();
        m_chatLog->setString("Прервано! Управление возвращено.");
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        if (!this->isVisible()) return false;
        CCPoint pos = this->convertTouchToNodeSpace(touch);
        CCSize s = this->getContentSize();
        if (pos.x < 0 || pos.y < 0 || pos.x > s.width || pos.y > s.height) {
            m_textField->detachWithIME();
            return false;
        }

        // Если нажали на нижнюю панель с полем ввода — открываем клавиатуру Android
        if (pos.y < 36.f && pos.x < s.width - 42.f) {
            m_textField->attachWithIME();
            return true;
        }

        m_textField->detachWithIME();
        m_dragStart = touch->getLocation();
        if (pos.x > s.width - 22.f && pos.y < 22.f) {
            m_isResizing = true;
        } else if (pos.y > s.height - 22.f) {
            m_isDragging = true;
        }
        return true;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent*) override {
        CCPoint delta = touch->getLocation() - m_dragStart;
        m_dragStart = touch->getLocation();

        if (m_isDragging) {
            this->setPosition(this->getPosition() + delta);
        } else if (m_isResizing) {
            CCSize s = this->getContentSize();
            float newW = std::max(180.f, s.width + delta.x);
            float newH = std::max(130.f, s.height - delta.y);
            this->setContentSize({newW, newH});
            updateLayoutPositions();
        }
    }

    void ccTouchEnded(CCTouch*, CCEvent*) override {
        m_isDragging = false;
        m_isResizing = false;
    }
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
    };

    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto brainSpr = CCSprite::create("brain.png"_spr);
        if (brainSpr) {
            auto circleBtnSpr = CircleButtonSprite::create(
                brainSpr, CircleBaseColor::Green, CircleBaseSize::Small
            );
            circleBtnSpr->setScale(0.85f);

            m_fields->crossSprite = CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png");
            if (m_fields->crossSprite) {
                m_fields->crossSprite->setPosition(circleBtnSpr->getContentSize() / 2.f);
                m_fields->crossSprite->setScale(0.8f);
                m_fields->crossSprite->setVisible(false);
                circleBtnSpr->addChild(m_fields->crossSprite, 10);
            }

            auto aiBtn = CCMenuItemSpriteExtra::create(
                circleBtnSpr, this, menu_selector(AIEditorUIHook::onToggleAIChat)
            );
            
            auto menu = CCMenu::create();
            menu->setPosition({195.f, winSize.height - 23.f});
            menu->addChild(aiBtn);
            this->addChild(menu, 100);
        }

        return true;
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

        if (m_fields->crossSprite) {
            m_fields->crossSprite->setVisible(nowVisible);
        }
    }
};
