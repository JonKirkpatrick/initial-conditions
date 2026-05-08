#include "Scene_Menu.h"
#include "Scene_IC_Camp.h"
#include "Assets.h"
#include "GameEngine.h"
#include "Components.hpp"
#include "Action.hpp"
#include "InputBindings.h"
#include "Theme.h"
#include <cmath>
           
Scene_Menu::Scene_Menu(GameEngine& gameEngine)
    : Scene(gameEngine)
{
    init();
}

void Scene_Menu::init()
{
    registerAction(sf::Keyboard::Scancode::W,       InputAction::Up);
    registerAction(sf::Keyboard::Scancode::S,       InputAction::Down);
    registerAction(sf::Keyboard::Scancode::D,       InputAction::Play);
    registerAction(sf::Keyboard::Scancode::Escape,  InputAction::Quit);
           
    m_title = "INITIAL CONDITIONS";
    m_menuStrings.push_back("New Game");
    m_menuStrings.push_back("Load Game");
    m_menuStrings.push_back("Options");

    m_menuBackground.loadFromFile("images/IC_Title.png");
    m_menuBackground.setSmooth(true);
}

std::optional<size_t> Scene_Menu::hitTestMenuIndex(const Vec2i& mousePosition) const
{
    sf::Vector2f mappedMouse = m_game.window().mapPixelToCoords(
        sf::Vector2i(mousePosition.x, mousePosition.y)
    );

    sf::Text menuText(Assets::Instance().getFont("Good"));
    menuText.setCharacterSize(48);

    for (size_t i = 0; i < m_menuStrings.size(); i++)
    {
        menuText.setString(m_menuStrings[i]);
        float textWidth = menuText.getGlobalBounds().size.x;
        float textHeight = menuText.getGlobalBounds().size.y;
        float x = m_game.window().getSize().x / 2.f - textWidth / 2.f;
        float y = 325.f + i * 72.f + 16.f;

        sf::FloatRect hitRect({x, y}, {textWidth, textHeight});
        if (hitRect.contains(mappedMouse))
        {
            return i;
        }
    }

    return std::nullopt;
}

void Scene_Menu::activateSelectedMenuItem()
{
    switch (m_selectedMenuIndex)
    {
        case 0:
            m_game.changeScene("CAMP", std::make_shared<Scene_IC_Camp>(m_game, "open_world.txt"), false, false);
            break;
        case 1:
            break;
        case 2:
            break;
    }
}

void Scene_Menu::onExit()
{
}

void Scene_Menu::onEnter()
{
    auto& track = Assets::Instance().getMusic("MusicTitle");
    AudioManager::Instance().music.playExclusive(track);
}
           
void Scene_Menu::update()
{
    m_entityManager.update();
}

void Scene_Menu::sDoAction(const Action& action)
{
    const std::vector<bool> isEnabled = {
        true,               // 0: New Game
        true,               // 1: Load Game
        true                // 2: Options
    };
    const size_t numItems = m_menuStrings.size();

    if (action.type() == "START")
    {
        if (action.name() == InputAction::Up)
        {
            size_t nextIndex = m_selectedMenuIndex;
            
            for (size_t i = 1; i <= numItems; ++i)
            {
                nextIndex = (m_selectedMenuIndex + numItems - i) % numItems;

                if (isEnabled[nextIndex])
                {
                    m_selectedMenuIndex = nextIndex;
                    break;
                }
            }
        }
        else if (action.name() == InputAction::Down)
        {
            size_t nextIndex = m_selectedMenuIndex;
            
            for (size_t i = 1; i <= numItems; ++i)
            {
                nextIndex = (m_selectedMenuIndex + i) % numItems;

                if (isEnabled[nextIndex])
                {
                    m_selectedMenuIndex = nextIndex;
                    break;
                }
            }
        }
        else if (action.name() == InputAction::Play)
        {
            activateSelectedMenuItem();
        }
        else if (action.name() == InputAction::Quit)
        {
            onEnd();
        }
        else if (action.name() == InputAction::LeftClick)
        {
            if (const auto hoveredIndex = hitTestMenuIndex(action.pos()))
            {
                m_selectedMenuIndex = *hoveredIndex;
                activateSelectedMenuItem();
            }
        }
        else if (action.name() == InputAction::MouseMove)
        {
            auto prevHovered = m_hoveredMenuIndex;
            m_hoveredMenuIndex = hitTestMenuIndex(action.pos());
            if (m_hoveredMenuIndex)
            {
                m_selectedMenuIndex = *m_hoveredMenuIndex;
                // If the hovered menu item changed, reset the hover start time
                if (!prevHovered || *prevHovered != *m_hoveredMenuIndex)
                {
                    m_hoverStartTime = m_game.getElapsedClock().getElapsedTime().asSeconds();
                }
            }
            else
            {
                m_hoverStartTime = std::nullopt;
            }
        }
    }
}

void Scene_Menu::sRender()
{
    m_game.window().setView(sf::View(sf::FloatRect(
        {0.f, 0.f},
        {float(m_game.window().getSize().x), float(m_game.window().getSize().y)}
    )));
    m_game.window().clear(Theme::color(Theme::ColorRole::BackgroundBase));
    sf::Sprite backgroundSprite(m_menuBackground);
    backgroundSprite.setPosition({0, 0});
    Vec2f scale(
        static_cast<float>(m_game.window().getSize().x) / static_cast<float>(m_menuBackground.getSize().x),
        static_cast<float>(m_game.window().getSize().y) / static_cast<float>(m_menuBackground.getSize().y)
    );
    backgroundSprite.setScale(scale);

    m_game.window().draw(backgroundSprite);

    sf::Text menuText(Assets::Instance().getFont("Good"));
    sf::Text menuTextShadow(Assets::Instance().getFont("Good"));
    menuText.setCharacterSize(76);
    menuTextShadow.setCharacterSize(76);
    menuTextShadow.setFillColor(Theme::color("shadow"));

    menuText.setString(m_title);
    menuText.setFillColor(Theme::color("major-title"));
    Vec2f titlePosition(m_game.window().getSize().x / 2 - menuText.getGlobalBounds().size.x / 2, 184);
    menuTextShadow.setString(m_title);
    menuTextShadow.setPosition(titlePosition + Vec2f(3, 3)); // slight offset for shadow effect
    m_game.window().draw(menuTextShadow);
    menuText.setPosition(titlePosition);
    m_game.window().draw(menuText);

    std::string previousScene = m_game.getPreviousScene();
    bool canReturnToGame = !previousScene.empty() && previousScene != "MENU";

    
    const sf::Color& disabledColor = Theme::color("disabled");
    const sf::Color& normalColor = Theme::color("normal");
    const sf::Color& selectedColor = Theme::color("active");

    // draw all of the menu options
    menuText.setCharacterSize(48);
    menuTextShadow.setCharacterSize(48);

    const float elapsedSeconds = m_game.getElapsedClock().getElapsedTime().asSeconds();
    for (size_t i = 0; i < m_menuStrings.size(); i++)
    {
        bool isSelected = (i == m_selectedMenuIndex);
        bool isHovered = m_hoveredMenuIndex.has_value() && i == *m_hoveredMenuIndex;
        bool isEnabled = true;

        // --- Conditional Coloring ---
        sf::Color finalColor;
        if (!isEnabled) {
            finalColor = disabledColor;
        } else if (isSelected) {
            finalColor = selectedColor;
        } else {
            finalColor = normalColor;
        }

        menuText.setString(m_menuStrings[i]);
        menuText.setScale({ 1.0f, 1.0f });
        menuTextShadow.setScale({ 1.0f, 1.0f });

        float hoverPulseY = 0.0f;
        float height = 0.0f;
        if (isHovered)
        {
            float hoverAnimTime = 0.0f;
            if (m_hoverStartTime)
            {
                hoverAnimTime = elapsedSeconds - *m_hoverStartTime;
                if (hoverAnimTime < 0.0f) hoverAnimTime = 0.0f; // safety
            }
            // Use hoverAnimTime instead of global elapsedSeconds for the pulse
            const float pulse = std::sin(hoverAnimTime * 7.5f);
            height = std::pow((pulse + 1.0f) * 0.5f, 1.5f);
            const float scale = 1.0f + (height * 0.03f);
            const float shadowScale = 1.0f + (height * 0.05f);
            menuText.setScale({ scale, scale });
            menuTextShadow.setScale({ shadowScale, shadowScale });
            hoverPulseY = -3.0f * height;
        }

        Vec2f itemPosition(
            m_game.window().getSize().x / 2 - menuText.getGlobalBounds().size.x / 2,
            325 + i * 72 + hoverPulseY
        );
        
        menuText.setFillColor(finalColor);
        
        menuTextShadow.setString(m_menuStrings[i]);
        const Vec2f baseShadowOffset = Vec2f(3, 3);
        const Vec2f shadowOffset = baseShadowOffset + Vec2f(3.0f, 3.0f) * height;
        float alphaFactor = 1.0f - (height * height * 0.3f);
        sf::Color shadowColor = Theme::color("shadow");
        shadowColor.a = static_cast<uint8_t>(shadowColor.a * alphaFactor);
        menuTextShadow.setLetterSpacing(1.0f - (height * 0.125f));
        menuTextShadow.setFillColor(shadowColor);
        menuTextShadow.setPosition(itemPosition + shadowOffset);
        m_game.window().draw(menuTextShadow);
        menuText.setPosition(itemPosition);
        m_game.window().draw(menuText);
    }

    // Reset transform and style state before drawing footer text.
    menuText.setScale({ 1.0f, 1.0f });
    menuTextShadow.setScale({ 1.0f, 1.0f });
    menuTextShadow.setFillColor(Theme::color("shadow"));
    menuTextShadow.setLetterSpacing(1.0f);
}

void Scene_Menu::onEnd()
{
    m_hasEnded = true;
    m_game.quit();
}