#pragma once

#include "../ECS/ECSTypes.h"

class SceneManager;

namespace EditorDebug
{
    void RenderObjectDebugWindow(SceneManager& sceneManager, Entity selectedEntity, bool& showWindow);
}
