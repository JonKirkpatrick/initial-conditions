#include "scenes/Scene.h"
#include "core/GameEngine.h"

Scene::Scene(GameEngine& gameEngine)
    : m_game(gameEngine)
{
    m_actionMap = m_game.getActionMap();
}

void Scene::setPaused(bool paused)
{
    m_paused = paused;
}
           
size_t Scene::width() const
{
    return m_game.window().getSize().x;
}

size_t Scene::height() const
{
    return m_game.window().getSize().y;
}

size_t Scene::currentFrame() const
{
    return m_currentFrame;
}

bool Scene::hasEnded() const
{
    return m_hasEnded;
}

const ActionMap& Scene::getActionMap() const
{
    return m_actionMap;
}

void Scene::registerAction(sf::Keyboard::Scancode inputKey, std::string_view actionName)
{
    m_actionMap[inputKey] = std::string(actionName);
}

void Scene::doAction(const Action& action)
{
    if (action.name() == "NONE") { return; }

    sDoAction(action);
}

void Scene::simulate(const size_t frames)
{
    for (size_t i = 0; i < frames; i++)
    {
        update();
    }
}