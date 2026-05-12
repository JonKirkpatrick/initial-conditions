#pragma once
           
#include "Animation.hpp"
#include "Assets.h"
#include "UIStructs.h"
#include "Theme.h"
#include "Vec2.hpp"
#include <array>

class Entity;


class Component
{
public:
    bool exists = false;
};

class CTransform3D : public Component
{
public:
    sf::Vector3f pos        = { 0.0f, 0.0f,  0.0f };
    sf::Vector3f prevPos    = { 0.0f, 0.0f,  0.0f };
    sf::Vector3f scale      = { 1.0f, 1.0f,  1.0f };
    sf::Vector3f velocity   = { 0.0f, 0.0f,  0.0f };
    float pitch     = 0;
    float yaw       = 0;
    float roll      = 0;
    float speed     = 0;
    bool onGround   = true;

    CTransform3D() = default;
    CTransform3D(const sf::Vector3f & p)
        : pos(p) {}
    CTransform3D(const sf::Vector3f & p, const sf::Vector3f & sp, const sf::Vector3f & sc, float pitch, float yaw, float roll)
        : pos(p), prevPos(p), velocity(sp), scale(sc), pitch(pitch), yaw(yaw), roll(roll) {}
};

class CPlayer : public Component
{
public:
    sf::Vector3f pos        = { 0.0f, 0.0f,  0.0f };
    sf::Vector3f prevPos    = { 0.0f, 0.0f,  0.0f };
    sf::Vector2f facing     = { 0.0f, -1.0f };
    bool onGround           = true;
    bool sprinting          = false;
    bool crouching          = false;
    float moveSpeed         = 0;
    float rotSpeed          = 0;
    float bobAccumulator    = 0;

    CPlayer() = default;
};

class CCamera : public Component
{
public:
    float fovY = 3.14159265f / 4.f;
    float aspectRatio = 1.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.f;
    sf::Vector2u viewportSize = { 800, 600 };

    CCamera() = default;
    CCamera(float fovY, float aspectRatio, float nearPlane, float farPlane, sf::Vector2u viewportSize)
        : fovY(fovY), aspectRatio(aspectRatio), nearPlane(nearPlane), farPlane(farPlane), viewportSize(viewportSize) {}
};

class CTransform : public Component
{
public:
    Vec2f pos        = { 0.0, 0.0 };
    Vec2f prevPos    = { 0.0, 0.0 };
    Vec2f scale      = { 1.0, 1.0 };
    Vec2f velocity   = { 0.0, 0.0 };
    Vec2f facing     = { 0.0, 1.0 };
    float angle     = 0;
    float speed     = 0;
    bool onGround = false;

    CTransform() = default;
    CTransform(const Vec2f & p)
        : pos(p) {}
    CTransform(const Vec2f & p, const Vec2f & sp, const Vec2f & sc, float a)
        : pos(p), prevPos(p), velocity(sp), scale(sc), angle(a) {}

};

class CLifeSpan : public Component
{
public:
    int lifespan = 0;
    int frameCreated = 0;
    CLifeSpan() = default;
    CLifeSpan(int duration, int frame) 
        : lifespan(duration), frameCreated(frame) {}
};

class CInput : public Component
{
public:
    bool up             = false;
    bool down           = false;
    bool left           = false;
    bool right          = false;
    bool forward        = false;
    bool backward       = false;
    bool yawLeft        = false;
    bool yawRight       = false;
    bool pitchUp        = false;
    bool pitchDown      = false;
    bool rollLeft       = false;
    bool rollRight      = false;
    bool strafe         = false;
    bool jump           = false;
    bool sprint         = false;
    bool interact       = false;
    float xAxis        = 0.f;
    float yAxis        = 0.f;
    Vec2f mouseDelta     = { 0.f, 0.f };

    CInput() {}
};

class CBoundingBox : public Component
{
public:
    Vec2f size;
    Vec2f halfSize;    
    bool blockMove = false;
    bool blockVision = false;
    CBoundingBox() = default;
    CBoundingBox(const Vec2f& s)
        : size(s), halfSize(s.x / 2, s.y / 2) {}
    CBoundingBox(const Vec2f& s, bool m, bool v)
        : size(s), blockMove(m), blockVision(v), halfSize(s.x / 2, s.y / 2) {}
};

class CBoundingBoxTimer : public Component
{
public:
    int framesRemaining;
    CBoundingBoxTimer() = default;
    CBoundingBoxTimer(int framesRemaining)
        : framesRemaining(framesRemaining) {}
};

class CCrashBox : public Component
{
public:
    Vec2f relativeOrigin = { 0.0f, 0.0f };
    Vec2f size;    
    CCrashBox() = default;
    CCrashBox(const Vec2f& s, const Vec2f& r)
        : relativeOrigin(r), size(s) {}
};

class CAnimation : public Component
{
public:
    Animation animation;
    bool repeat = false;
    CAnimation() = default;
    CAnimation(const Animation & animation, bool r)
        : animation(animation), repeat(r) {}
};

class CGravity : public Component
{
public:
    float gravity = 0;
    CGravity() = default;
    CGravity(float g) : gravity(g) {}
};

class CState : public Component
{
public:
    std::string state = "jumping";
    CState() = default;
    CState(const std::string & s) : state(s) {}
};

class CShape : public Component
{
public:
    sf::CircleShape circle;

    CShape() = default;
    CShape(float radius, size_t points, const sf::Color & fill, const sf::Color & outline, float thickness)
        : circle(radius, points)
    {
        circle.setFillColor(fill);
        circle.setOutlineColor(outline);
        circle.setOutlineThickness(thickness);
        circle.setOrigin({ radius, radius });
    }
};

class CParallax : public Component
{
public:
    int layer = 1;
    Vec2f pos;
    CParallax() = default;
    CParallax(int layer)
        : layer(layer) {}
    CParallax(int layer, Vec2f pos)
        : layer(layer), pos(pos) {}
};

class CLerp : public Component
{
public:
    Vec2f destination;
    float speed;
    CLerp() = default;
    CLerp(Vec2f destination, float speed)
        : destination(destination), speed(speed) {}
};

class CLightSource : public Component
{
public:
    CLightSource() = default;
};

class CText : public Component
{
public:
    sf::Text text;


    CText() // Default constructor constructs default text since it lacks its own default constructor
        : text(Assets::Instance().getFont("Megaman"), "DefaultText", 24)
    {
        text.setFillColor(Theme::color(Theme::ColorRole::TextBody));
    }

    CText(const std::string & fontType, std::string textContent , const int fontSize, const Vec2f & position)
        : text(Assets::Instance().getFont(fontType), textContent, fontSize)
    {
        text.setFillColor(Theme::color(Theme::ColorRole::TextBody));
        text.setPosition({position.x, position.y});
    }
};

class CUISlider : public Component
{
public:
    float minValue = 0.0f;
    float maxValue = 100.0f;
    float currentValue = 50.0f;
    float dragGrabOffset = 0.0f;
    size_t panelIndex = 0;
    bool dragging = false;
    bool active = false;
    SliderElements elements;
    Vec2i hitBoxSize = Vec2i(77,40);
    Vec2i hitBoxOffset = Vec2i(26,45);
    std::string texture = "TexSlider";
    std::string targetAudioBus = "music";

    CUISlider() = default;
    CUISlider(float minV, float maxV, float currV, std::string bus)
        : minValue(minV), maxValue(maxV), currentValue(currV), targetAudioBus(bus) {}
};

class CUIButton : public Component
{
public:
    ButtonVisual visual = ButtonVisual::OFF;
    ButtonState state = ButtonState::UP;
    ButtonFunction function = ButtonFunction::TOGGLE_MUSIC;
    GameDifficulty difficulty = GameDifficulty::EASY;
    bool buttonBlue = true;
    bool isToggle = true;

    // Right now I'm only actually using onPressed, 
    std::function<void()> onPressed;
    std::function<void()> onReleased;
    std::function<void()> onHovered;
    std::function<void()> onUnhovered;

    Vec2i hitBoxSize = Vec2i(48,48);
    Vec2i hitBoxOffset = Vec2i(-24,-24);
    bool hovered = false;
    bool pressed = false;
    size_t panelIndex = 0;

    CUIButton() = default;
};

/*
 * I strayed from our usual pattern here a bit by using a texture instead of
 * an animation for the panel.  I drew the three panels in a single texture
 * and it seemed a little easier to manage that way.
 */
class CUIPanel : public Component
{
public:
    int activePane = 0;
    std::string texture = "TexPanels";
    sf::IntRect panelInteriorRect;
    PanelPanes panes;
    PanelHitBoxes hitBoxes;
    CUIPanel() = default;
};

class CTextLabel : public Component
{
public:
    int panelIndex = 0;
    std::string text = "";
    std::string font = "Hack";
    TextAnchor anchor = TextAnchor::TOP_CENTRE;
    int characterSize = 34;
    sf::Color color = sf::Color::Black;
    CTextLabel() = default;
};

class CLEDIndicator : public Component
{
public:
    int panelIndex = 0;
    size_t errorCounter = 0;
    LEDStates state = LEDStates::OFF;
    ButtonFunction function = ButtonFunction::RECORD_KEY;
    CLEDIndicator() = default;
};

class COrb : public Component
{
public:
    sf::Color color = sf::Color::White;
    float radius = 50.0f;
    float bobRate = 2.0f;           // cycles per second
    float bobMagnitude = 8.0f;      // units of vertical movement
    float bobPhase = 0.0f;          // current phase [0, 1)
    float heightAboveGround = 100.0f;

    COrb() = default;
    COrb(const sf::Color& c, float r, float rate = 2.0f, float mag = 8.0f, float heightAbove = 100.0f)
        : color(c), radius(r), bobRate(rate), bobMagnitude(mag), heightAboveGround(heightAbove) {}
};

static_assert(std::is_default_constructible_v<CTransform3D>);
static_assert(std::is_default_constructible_v<CPlayer>);
static_assert(std::is_default_constructible_v<CCamera>);
static_assert(std::is_default_constructible_v<CTransform>);
static_assert(std::is_default_constructible_v<CLifeSpan>);
static_assert(std::is_default_constructible_v<CInput>);
static_assert(std::is_default_constructible_v<CBoundingBox>);
static_assert(std::is_default_constructible_v<CBoundingBoxTimer>);
static_assert(std::is_default_constructible_v<CCrashBox>);
static_assert(std::is_default_constructible_v<CAnimation>);
static_assert(std::is_default_constructible_v<CGravity>);
static_assert(std::is_default_constructible_v<CState>);
static_assert(std::is_default_constructible_v<CShape>);
static_assert(std::is_default_constructible_v<CParallax>);
static_assert(std::is_default_constructible_v<CLerp>);
static_assert(std::is_default_constructible_v<CLightSource>);
static_assert(std::is_default_constructible_v<CText>);
static_assert(std::is_default_constructible_v<CUISlider>);
static_assert(std::is_default_constructible_v<CUIButton>);
static_assert(std::is_default_constructible_v<CUIPanel>);
static_assert(std::is_default_constructible_v<CTextLabel>);
static_assert(std::is_default_constructible_v<CLEDIndicator>);
static_assert(std::is_default_constructible_v<COrb>);

static_assert(std::is_default_constructible_v<Vec2f>);
static_assert(std::is_default_constructible_v<Animation>);
static_assert(std::is_default_constructible_v<SliderElements>);
