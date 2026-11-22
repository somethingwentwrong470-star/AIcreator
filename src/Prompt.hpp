#pragma once
#include <Geode/Geode.hpp>
#include <string>
#include <vector>

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
        "ГЕОМЕТРИЯ СЕТКИ: 1 клетка = 30 единиц. Белая линия пола на Y=105! "
        "НИКОГДА не ставь Y < 120 (иначе объект уйдет под землю!). "
        "Первый этаж на полу: Y=120, второй: Y=150, третий: Y=180, четвертый: Y=210. "
        "Для обычных блоков ставь rot=0. "
        "Текущая камера: X={:.0f}, Y={:.0f}. Строй вправо от X={:.0f} с шагом 30, 60 или 90. "
        "КОНТЕКСТ ВЫДЕЛЕНИЯ: {}. "
        "Если игрок спрашивает про выделенные объекты или просит их продолжить/достроить, опирайся на их координаты и id. "
        "Отвечай ТОЛЬКО валидным JSON без markdown: "
        "{{\"reply\": \"краткий ответ\", \"objects\": [{{\"id\": 1, \"x\": 300, \"y\": 120, \"rot\": 0}}]}}. "
        "БАЗА ID: "
        "Блоки: 1-блок; "
        "Шипы: 8-обычный, 9-маленький, 39-очень маленький, 103-плоский; "
        "Орбы: 36-желтый, 141-розовый, 1333-красный, 84-синий гравитации, 1022-зеленый, 1330-черный, 1704-зеленый дэш, 1751-розовый дэш, 3004-паучий; "
        "Батуты: 35-желтый, 140-розовый, 1332-красный, 67-синий, 3005-паучий; "
        "Порталы: 12-куб, 13-корабль, 47-шар, 111-НЛО, 660-волна, 745-робот, 1331-паук, 1933-свинг; "
        "Скорости: 200-медл, 201-1x, 202-2x, 203-3x, 1334-4x. "
        "ПРИМЕР прыжка через шип на полу: "
        "{{\"reply\": \"Построил прыжок!\", \"objects\": ["
        "{{\"id\": 1, \"x\": 300, \"y\": 120, \"rot\": 0}}, "
        "{{\"id\": 8, \"x\": 390, \"y\": 120, \"rot\": 0}}, "
        "{{\"id\": 36, \"x\": 420, \"y\": 180, \"rot\": 0}}, "
        "{{\"id\": 1, \"x\": 510, \"y\": 150, \"rot\": 0}}]}}.",
        startX, startY, startX, selContext
    );
}
