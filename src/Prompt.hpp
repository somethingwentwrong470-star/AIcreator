#pragma once
#include <Geode/Geode.hpp>
#include <string>

inline std::string getSelectedObjectsContext() {
    auto editor = LevelEditorLayer::get();
    if (!editor || !editor->m_editorUI) return "Выделенных объектов нет.";

    auto selected = editor->m_editorUI->getSelectedObjects();
    if (!selected || selected->count() == 0) return "Выделенных объектов нет.";

    std::string list;
    int limit = std::min<int>(selected->count(), 35);
    for (int i = 0; i < limit; ++i) {
        if (auto obj = static_cast<GameObject*>(selected->objectAtIndex(i))) {
            list += fmt::format("[id:{}, x:{:.0f}, y:{:.0f}, rot:{:.0f}] ",
                obj->m_objectID, obj->getPositionX(), obj->getPositionY(), obj->getRotation());
        }
    }
    return fmt::format("Выделенные игроком объекты ({} шт.): {}", selected->count(), list);
}

inline std::string buildAISystemPrompt(float camX, float camY) {
    float startX = std::max(300.f, std::round(camX / 30.f) * 30.f);
    float startY = std::max(120.f, std::round(camY / 30.f) * 30.f);
    std::string selContext = getSelectedObjectsContext();

    return fmt::format(
        "Ты ИИ-строитель уровней в Geometry Dash 2.208. "
        "ГЕОМЕТРИЯ СЕТКИ: 1 клетка = 30 единиц. Пол на Y=105, первый этаж на полу Y=120, второй Y=150, третий Y=180. НИКОГДА не ставь Y < 120. "
        "Камера: X={:.0f}, Y={:.0f}. Строй вправо от X={:.0f}. "
        "КОНТЕКСТ ВЫДЕЛЕНИЯ: {}. "
        "ВАЖНО: если игрок просто задает вопрос (не просит строить), возвращай пустые списки \"templates\": [] и \"objects\": []! "
        "Отвечай ТОЛЬКО валидным JSON без markdown: "
        "{{\"reply\": \"краткий ответ\", \"templates\": [{{\"name\": \"arch\", \"id\": 1, \"x\": 300, \"y\": 120}}], \"objects\": [{{\"id\": 8, \"x\": 360, \"y\": 120, \"rot\": 0}}]}}. "
        "ШАБЛОНЫ: "
        "island_small (3x2), island_wide (4x2), pillar_thin (1x4), pillar_thick (2x5), arch (арка 4x3), stairs (лесенка 4x4), wave_up (коридор волны вверх 4x6), wave_down (коридор волны вниз 4x6), heart (сердце 5x5). "
        "БАЗА ID: Блоки: 1-блок; Шипы: 8-шип, 9-мал.шип, 103-плоский; "
        "Орбы: 36-желт, 141-розов, 1333-красн, 84-син, 1022-зел, 1330-черн, 1704-зел.дэш, 1751-роз.дэш, 3004-паук; "
        "Батуты: 35-желт, 140-розов, 1332-красн, 67-син, 3005-паук; "
        "Порталы: 12-куб, 13-корабль, 47-шар, 111-НЛО, 660-волна, 745-робот, 1331-паук, 1933-свинг; "
        "Скорости: 200-медл, 201-1x, 202-2x, 203-3x, 1334-4x.",
        startX, startY, startX, selContext
    );
}
