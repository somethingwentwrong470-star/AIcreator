#pragma once
#include <Geode/Geode.hpp>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

using namespace geode::prelude;

inline std::string getDefaultPromptRules() {
    return
        "Ты ИИ-строитель уровней в Geometry Dash 2.208. 1 клетка = 30 ед. Центры клеток: 15, 45, 75, 105... (формула: 15 + клетка*30). "
        "Пол на Y=0. Ряды снизу вверх: 1-й (на полу) Y=15, 2-й Y=45, 3-й Y=75, 4-й Y=105, 5-й Y=135, 6-й Y=165, 7-й Y=195, 8-й Y=225, 9-й Y=255, 10-й Y=285. "
        "НИКОГДА не ставь Y < 15! Если игрок просто общается и не просит строить, верни \"t\":[],\"o\":[]. "
        "Отвечай ТОЛЬКО валидным JSON без markdown: "
        "{\"r\":\"краткий ответ\",\"t\":[{\"n\":\"arch\",\"id\":1,\"x\":315,\"y\":15}],\"o\":[{\"id\":8,\"x\":465,\"y\":15,\"rot\":0}]}. "
        "ШАБЛОНЫ n (ставь на Y=15 для наземных и коридоров): "
        "island_small (3x2), island_wide (4x2), pillar_thin (1x4), pillar_thick (2x5), arch (арка 4x3), stairs (лесенка 4x4), "
        "wave_up (волна вверх 4x6), wave_down (волна вниз 4x6), spike_triple (3 шипа), orb_jump (яма с шипами и желт. орбом 11x3), "
        "pad_jump (желт. батут и платформа 7x3), spider_ceiling (потолок паука с шипами 11x9), ship_corridor (тоннель корабля с островами 13x10). "
        "ПРАВИЛО ID В ШАБЛОНАХ: в \"t\" всегда пиши \"id\":1! Меняй \"id\" на декоративный (например 1329) ТОЛЬКО если игрок прямо попросил декор. Никогда не подставляй ID из выделенных объектов! "
        "РЕЖИМЫ И ВЫСОТА ЭКРАНА: "
        "Ball (шар, меняет гравитацию) - макс 8 клеток (потолок Y=225); "
        "Spider (паук, телепорт пол/потолок) - макс 9 клеток (потолок Y=255); "
        "Ship/UFO/Swing (полёт) и Wave (движение под 45 град.) - макс 10 клеток (потолок Y=285); "
        "Cube (прыжок ~2 клетки вверх, 3 в длину) и Robot (высота прыжка зависит от зажатия) - высота вверх не ограничена. "
        "БАЗА ID: 1-блок, 8-шип, 9-мал.шип, 103-плоск.шип; "
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
    return fmt::format("{} шт (справка, не используй их ID для замены блоков): {}", selected->count(), list);
}

inline std::string buildAISystemPrompt(float camX, float camY) {
    float startX = std::max(315.f, std::floor(camX / 30.f) * 30.f + 15.f);
    float startY = std::max(15.f, std::floor(camY / 30.f) * 30.f + 15.f);
    return fmt::format(
        "{}\nТЕКУЩИЙ КОНТЕКСТ: Камера X={:.0f}, Y={:.0f} (строй вправо начиная с X={:.0f}, пол на Y=15). Выделено игроком: {}.",
        loadEditablePromptRules(), startX, startY, startX, getSelectedObjectsContext()
    );
}
