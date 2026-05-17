#pragma once
#include "Assets.h"
#include "SoAEntityManager.hpp"

struct SliderElements 
{
    sf::IntRect handle;
    sf::IntRect track;
    sf::IntRect softGlow;
    sf::IntRect brightGlow;
};

struct PanelPanes
{
    sf::IntRect panel1;
    sf::IntRect panel2;
    sf::IntRect panel3;
};

struct PanelHitBoxes
{
    sf::IntRect button1;
    sf::IntRect button2;
    sf::IntRect button3;
};

struct ActionList
{
    std::string friendlyName;
    std::string realName;
    sf::Keyboard::Scancode actionCode;
    SoAEntityHandle textAction;
    SoAEntityHandle textKey;
    sf::IntRect boundingBox;
};


enum class ButtonVisual {
    OFF,
    BLUE_LOW,
    BLUE_HIGH,
    RED_LOW,
    RED_HIGH
};

enum class ButtonState {
    UP,
    DOWN,
    HOVER
};

enum class ButtonFunction {
    TOGGLE_MUSIC,
    TOGGLE_SFX,
    DO_SOMETHING,
    RECORD_KEY,
    LOAD_DEFAULTS,
    COMMIT_CHANGES,
    SAVE_GAME,
    RADIO_DIFFICULTY,
    QUIT
};

enum class LEDStates {
    OFF,
    RED_LO,
    AMBER_LO,
    GREEN_LO,
    RED_HI,
    AMBER_HI,
    GREEN_HI
};

enum class GameDifficulty {
    EASY,
    NORMAL,
    HARD
};

enum class PickupType {
    HEALTH,
    STAMINA,
    AMMO,
    WEAPON,
    KEY_ITEM
};

enum class UsageType {
    INSTANT,
    UNIQUE,
    STACKABLE,
    QUEST
};

enum class TextAnchor {
    CENTRE_LEFT,
    CENTRE,
    CENTRE_RIGHT,
    TOP_CENTRE,
    BOTTOM_CENTRE,
    TOP_RIGHT,
    TOP_LEFT,
    BOTTOM_RIGHT,
    BOTTOM_LEFT
};