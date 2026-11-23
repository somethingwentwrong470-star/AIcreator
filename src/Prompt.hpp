#pragma once
#include <Geode/Geode.hpp>
#include <fstream>
#include <sstream>
#include <string>

using namespace geode::prelude;

inline std::string getDefaultPromptRules() {
    return
        "Ты ИИ-строитель уровней в Geometry Dash 2.208. 1 клетка = 30 единиц. "
        "Пол на Y=105, первый этаж на полу Y=120, второй Y=150, третий Y=180. НИКОГДА не ставь Y < 120! "
        "Если игрок просто задает вопрос без просьбы строить, возвращай пустые списки \"t\":[],\"o\":[]. "
        "Отвечай ТОЛЬКО валидным JSON без markdown: "
        "{\"r\":\"краткий ответ\",\"t\":[{\"n\":\"arch\",\"id\":1,\"x\":300,\"y\":120}],\"o\":[{\"id\":8,\"x\":360,\"y\":120,\"rot\":0}]}. "
        "Шаблоны n: island_small (3x2), island_wide (4x2), pillar_thin (1x4), pillar_thick (2x5), arch (арка 4x3), stairs (лесенка 4x4), wave_up (волна вверх 4x6), wave_down (волна вниз 4x6), heart (сердце 5x5). "
        "БАЗА ID: 1-блок, 8-шип, 9-мал.шип, 103-плоск.шип, "
        "Орбы: 36-желт, 141-розов, 1333-красн, 84-син, 1022-зел, 1330-черн, 1704-зел.дэш, 1751-роз.дэш, 3004-паук; "
        "Батуты: 35-желт, 140-розов, 1332-красн, 67-син, 3005-паук; "
        "Порталы: 12-куб, 13-корабль, 47-шар, 111-НЛО, 660-волна, 745-робот, 1331-паук, 1933-свинг; "
        "Скорости: 200-медл, 201-1x, 202-2x, 203-3x, 1334-4x.";
}

inline std::string loadEditablePromptRules() {
    auto promptPath = Mod::get()->getConfigDir() / "ai_prompt.txt";
    std::error_code ec;
    if (!std::filesystem::exists(promptPath, ec)) {
        std::filesystem::create_directories(Mod::get()->getConfigDir(), ec);
        std::ofstream out(promptPath);
        if (out.is_open()) out << getDefaultPromptRules();
        return getDefaultPromptRules();
    }
    std::ifstream in(promptPath);
    if (!in.is_open()) return getDefaultPromptRules();
    std::stringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();
    return content.empty() ? getDefaultPromptRules() : content;
}

inline std::string getSelectedObjectsContext() {
    auto editor = LevelEditorLayer::get();
    if (!editor || !editor->m_editorUI) return "нет";
    auto selected = editor->m_editorUI->getSelectedObjects();
    if (!selected || selected->count() == 0) return "нет";
    std::string list;
    int limit = std::min<int>(selected->count(), 25);
    for (int i = 0; i < limit; ++i) {
        if (auto obj = static_cast<GameObject*>(selected->objectAtIndex(i))) {
            list += fmt::format("[id:{},x:{:.0f},y:{:.0f}] ", obj->m_objectID, obj->getPositionX(), obj->getPositionY());
        }
    }
    return fmt::format("{} шт: {}", selected->count(), list);
}

inline std::string buildAISystemPrompt(float camX, float camY) {
    float startX = std::max(300.f, std::round(camX / 30.f) * 30.f);
    float startY = std::max(120.f, std::round(camY / 30.f) * 30.f);
    return fmt::format(
        "{}\nТЕКУЩИЙ КОНТЕКСТ: Камера X={:.0f}, Y={:.0f} (строй вправо от X={:.0f}). Выделено игроком: {}.",
        loadEditablePromptRules(), startX, startY, startX, getSelectedObjectsContext()
    );
}
