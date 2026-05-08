#pragma once

#include "Entity.hpp"
#include "Vec2.hpp"

struct Intersect;

namespace Physics
{    
    Vec2f GetOverlap(std::shared_ptr<Entity> a, std::shared_ptr<Entity> b);
    Vec2f GetPreviousOverlap(std::shared_ptr<Entity> a, std::shared_ptr<Entity> b);
    bool IsInside(const Vec2f& pos, std::shared_ptr<Entity> e);
    Intersect LineIntersect(const Vec2f& a, const Vec2f& b, const Vec2f& c, const Vec2f& d);
    bool EntityIntersect(const Vec2f& a, const Vec2f& b, std::shared_ptr<Entity> e);
    Vec2f GetClosestIntersection(const Vec2f& a, const Vec2f& b, EntityManager& em);
}