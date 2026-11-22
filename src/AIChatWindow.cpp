#include "AIChatWindow.hpp"
#include "Config.hpp"
#include "Prompt.hpp"
#include <Geode/modify/LevelEditorLayer.hpp>

AIChatWindow::~AIChatWindow() {
    m_webHolder.cancel();
    g_aiControlling = false;
}

AIChatWindow* AIChatWindow::create() {
    auto ret = new AIChatWindow();
    if (ret && ret->init()) { ret->autorelease(); return ret; }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool AIChatWindow::init() {
    if (!CCLayer::init()) return false;
    ensureConfigTemplate();
    g_aiControlling = false;
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
    m_chatLog = CCLabelTTF::create("Cloudflare готов! Пиши запрос по-русски:", "ru_font.ttf"_spr, 10, {214.f, 95.f}, kCCTextAlignmentLeft);
    this->addChild(m_chatLog, 5);

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

    m_providerSpr = ButtonSprite::create("Cloudflare", "goldFont.fnt", "GJ_button_04.png", 0.4f);
    this->addChild(m_providerSpr, 12);

    recreateTextField(false);
    updateLayoutPositions();
    return true;
}
void AIChatWindow::recreateTextField(bool openKeyboard) {
    if (m_textField) {
        m_textField->detachWithIME();
        m_textField->removeFromParent();
        m_textField = nullptr;
    }
    CCSize s = this->getContentSize();
    float inputWidth = s.width - 58.f;
    m_textField = CCTextFieldTTF::textFieldWithPlaceHolder(
        "Нажми сюда и пиши...", {inputWidth - 8.f, 18.f},
        kCCTextAlignmentLeft, "ru_font.ttf"_spr, 11.f
    );
    m_textField->setColorSpaceHolder({150, 150, 170});
    m_textField->setColor({255, 255, 255});
    m_textField->setPosition({inputWidth / 2.f + 10.f, 17.f});
    this->addChild(m_textField, 10);
    if (openKeyboard) m_textField->attachWithIME();
}

void AIChatWindow::updateLayoutPositions() {
    CCSize s = this->getContentSize();
    m_bg->setContentSize(s);
    m_headerBg->setContentSize({s.width, 22.f});
    m_headerBg->setPosition({0.f, s.height - 22.f});
    m_title->setPosition({32.f, s.height - 11.f});
    m_providerSpr->setPosition({s.width / 2.f + 12.f, s.height - 11.f});
    m_chatLog->setDimensions({s.width - 16.f, s.height - 62.f});
    m_chatLog->setPosition({s.width / 2.f, s.height / 2.f + 4.f});
    float inputWidth = s.width - 58.f;
    m_inputBg->setContentSize({inputWidth, 24.f});
    m_inputBg->setPosition({8.f, 6.f});
    if (m_textField) {
        m_textField->setDimensions({inputWidth - 8.f, 18.f});
        m_textField->setPosition({inputWidth / 2.f + 10.f, 17.f});
    }
    m_sendBtn->setPosition({s.width - 24.f, 18.f});
    m_abortBtn->setPosition({s.width - 24.f, s.height - 11.f});
}

void AIChatWindow::onSend(CCObject*) {
    if (!m_textField) return;
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

    std::string sysPrompt = buildAISystemPrompt(camX, camY);

    std::string cfIdRaw = readConfigValue("CF_ID");
    if (cfIdRaw.empty()) cfIdRaw = Mod::get()->getSettingValue<std::string>("cf-account-id");
    std::string cfId = extractHex32(cfIdRaw);

    std::string apiKey = readConfigValue("CF_KEY");
    if (apiKey.empty()) apiKey = trimStr(Mod::get()->getSettingValue<std::string>("api-key"));

    if (cfId.size() != 32 || apiKey.empty()) {
        auto cfgPath = Mod::get()->getConfigDir() / "ai_config.txt";
        m_chatLog->setString(fmt::format("ПРОВЕРКА КОНФИГА\nФайл: {}\nCF_ID: {}/32\nCF_KEY: {}",
            cfgPath.string(), cfId.size(), apiKey.empty() ? "ПУСТОЙ" : "НАЙДЕН").c_str());
        return;
    }

    recreateTextField(false);
    matjson::Value sysMsg = matjson::Value::object();
    sysMsg["role"] = "system"; sysMsg["content"] = sysPrompt;
    matjson::Value usrMsg = matjson::Value::object();
    usrMsg["role"] = "user"; usrMsg["content"] = userPrompt;
    matjson::Value messages = matjson::Value::array();
    messages.push(sysMsg); messages.push(usrMsg);

    g_aiControlling = true;
    m_abortBtn->setVisible(true);
    m_chatLog->setString(("Вы: " + userPrompt + "\n[Cloudflare думает...]").c_str());

    matjson::Value payload = matjson::Value::object();
    payload["messages"] = messages;

    std::string cfUrl = fmt::format(
        "{}://{}.{}/{}/{}/{}/{}/{}/{}cf/meta/llama-3.1-8b-instruct",
        "https", "api.cloudflare", "com", "client", "v4", "accounts", cfId, "ai/run", std::string(1, 64)
    );

    web::WebRequest req;
    req.userAgent("Mozilla/5.0 GeodeMod/1.0");
    req.header("Content-Type", "application/json");
    req.header("Authorization", "Bearer " + apiKey);
    req.bodyString(payload.dump());

    m_webHolder.spawn(req.post(cfUrl), [this](web::WebResponse res) {
        if (!g_aiControlling) return;
        std::string rawResp = res.string().unwrapOr("");
        if (res.ok() && res.json().isOk()) {
            auto data = res.json().unwrap();
            std::string aiText = data["result"]["response"].asString().unwrapOr("");
            if (aiText.empty() && data["result"]["choices"].isArray()) {
                auto arr = data["result"]["choices"].asArray().unwrap();
                if (!arr.empty()) aiText = arr.front()["message"]["content"].asString().unwrapOr("");
            }
            if (aiText.empty() && data["choices"].isArray()) {
                auto arr = data["choices"].asArray().unwrap();
                if (!arr.empty()) aiText = arr.front()["message"]["content"].asString().unwrapOr("");
            }
            if (aiText.empty()) m_chatLog->setString(("Ошибка ответа:\n" + rawResp.substr(0, 200)).c_str());
            else executeAIActions(aiText);
        } else if (res.ok()) executeAIActions(rawResp);
        else m_chatLog->setString(fmt::format("CF Код {}: {}\n{}", res.code(), res.errorMessage(), rawResp.substr(0, 110)).c_str());
        finishAIControl();
    });
}

void AIChatWindow::executeAIActions(const std::string& rawText) {
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
        for (auto& item : root["objects"].asArray().unwrap()) {
            if (!g_aiControlling) break;
            float objX = static_cast<float>(item["x"].asDouble().unwrapOr(300.0));
            float objY = static_cast<float>(item["y"].asDouble().unwrapOr(15.0));
            if (objY < 15.f) objY = 15.f;
            if (auto gameObj = editor->createObject(item["id"].asInt().unwrapOr(1), {objX, objY}, false)) {
                gameObj->setRotation(static_cast<float>(item["rot"].asDouble().unwrapOr(0.0)));
            }
        }
    }
}

void AIChatWindow::finishAIControl() { g_aiControlling = false; if (m_abortBtn) m_abortBtn->setVisible(false); }
void AIChatWindow::onAbort(CCObject*) { m_webHolder.cancel(); finishAIControl(); m_chatLog->setString("Прервано!"); }

bool AIChatWindow::ccTouchBegan(CCTouch* touch, CCEvent*) {
    if (!this->isVisible()) return false;
    CCPoint pos = this->convertTouchToNodeSpace(touch);
    CCSize s = this->getContentSize();
    if (pos.x < 0 || pos.y < 0 || pos.x > s.width || pos.y > s.height) { if (m_textField) m_textField->detachWithIME(); return false; }
    if (pos.y < 36.f && pos.x < s.width - 42.f) { recreateTextField(true); return true; }
    if (m_textField) m_textField->detachWithIME();
    m_dragStart = touch->getLocation();
    if (pos.x > s.width - 22.f && pos.y < 22.f) m_isResizing = true;
    else if (pos.y > s.height - 22.f) m_isDragging = true;
    return true;
}

void AIChatWindow::ccTouchMoved(CCTouch* touch, CCEvent*) {
    CCPoint delta = touch->getLocation() - m_dragStart;
    m_dragStart = touch->getLocation();
    if (m_isDragging) this->setPosition(this->getPosition() + delta);
    else if (m_isResizing) {
        CCSize s = this->getContentSize();
        this->setContentSize({std::max(180.f, s.width + delta.x), std::max(130.f, s.height - delta.y)});
        updateLayoutPositions();
    }
}

void AIChatWindow::ccTouchEnded(CCTouch*, CCEvent*) { m_isDragging = false; m_isResizing = false; }
