#include <iostream>

#include "qttest.hpp"


double simpleAngle(double a)
{
  while (a > M_PI)
    a = a - M_PI*2.0;
  while (a < -M_PI)
    a = M_PI*2.0 + a;
  return a;
}

double deltaAngle(double src, double dst)
{
  src = simpleAngle(src);
  dst = simpleAngle(dst);
  double a = std::min(src, dst);
  double b = std::max(src, dst);
  double res = b - a;
  if (res > M_PI)
    res = -(M_PI * 2 - res);
  if (a == dst)
    res = -res;
  return res;
}

static P2 randomHitLocation(Ship const& target)
{
  switch (target.shipType)
  {
  case Asset::ShipFighter:
    return P2{rand()%10, rand()%30 - 15};
  case Asset::ShipFrigate:
    return P2{rand()%30 - 15, rand()%40 - 20};
  case Asset::ShipCruiser:
    return P2{rand()%16 - 8, rand()%50 - 25};
  case Asset::MissileEMP:
    return P2{rand()%10, rand()%30 - 15};
  }
}

static P2 randomBuildingHitLocation()
{
  return P2{rand()%80 - 40, rand()%80 - 40};
}

void Ship::setDestination(P2 target)
{
  patrolPoint = destination = target;
}

double Ship::angularVelocity(ShipPtr target)
{
  P2 rSpeed = target->speed - speed;
  P2 rPos = target->position - position;
  P2 tang = P2 { -rPos.y, rPos.x};
  tang = tang / tang.length();
  double tSpeed = tang.x * rSpeed.x + tang.y * rSpeed.y;
  double aSpeed = fabs(atan(1.0 / rPos.length())) * fabs(tSpeed);
  return aSpeed;
}
void Ship::fireAt(ShipPtr target, int weaponSlot)
{
  double av = angularVelocity(target);
  bool hit;
  if (av <= config.aimOptimal)
    hit = true;
  else if (av >= config.aimMax)
    hit = false;
  else
    hit = (double)(rand()%1000)/1000.0 > (av-config.aimOptimal)/(config.aimMax-config.aimOptimal);
  if (hit)
    shots.push_back(Laser{weaponSlot, target, nullptr, randomHitLocation(*target), now(), true});
  else
    shots.push_back(Laser{weaponSlot, target, nullptr, randomHitLocation(*target), now(), false});
  shots.back().pix = new QGraphicsLineItem();
  shots.back().pix->setPen(QPen(
    hit?
       (player.flip()? QColor(70, 70, 255) : QColor(255, 255, 70))
       :(player.flip()? QColor(100, 100, 140) : QColor(140, 100, 100))
    ));
  player.game().scene().addItem(shots.back().pix);
  //player.screen().recordShot(typeIndex(), hit);
}

void dealFreezeDamage(std::vector<ShipPtr> const& ships, Player& player, P2 position, double range, double time)
{
  for (auto& ship: ships)
  {
    if (&ship->player == &player)
      continue;
    if ((position-ship->position).length() > range)
      continue;
    ship->jammedUntil = now() + std::chrono::milliseconds((long)(time* 1000.0));
    for (auto& shot: ship->shots)
    {
      player.game().scene().removeItem(shot.pix);
      delete shot.pix;
    }
    ship->shots.clear();
  }
}

void Ship::think(std::vector<ShipPtr> const& ships, std::vector<Building*> const& buildings)
{
  if ((position-patrolPoint).length() < (shipType == Asset::MissileEMP ? 20.0 : 10.0f))
  { // generate new patrolpoint
    if (shipType == Asset::MissileEMP)
    {
      // blow up
      hp = -1.0;
      dealFreezeDamage(ships, player, position, config.range, config.damage);
      return;
    }
    auto spread = (int)player.spread;
    P2 delta{rand()%spread-spread/2, rand()%spread-spread/2};
    patrolPoint = destination - delta;
  }
  if (config.moving)
  {
    P2 v = patrolPoint - position;
    double abadrep = atan2(-v.y, v.x); // reversed referrential, 0 is up-left
    double a = simpleAngle(abadrep-M_PI/2.0);
    double aSpeed = simpleAngle(atan2(-speed.y, speed.x)-M_PI/2.0);
    
    double da = deltaAngle(rotation, a);
    double good = fabs(da) < 1; // only accelerate if pushing in the right direction
    angularSpeed = (da > 0.0 ? 1.0 : -1.0) * config.angularSpeed;
    acceleration = (speed.length() > config.cruseSpeed  || !good) ? 0.0 : config.acceleration;
    /*std::cerr << "target " << patrolPoint << " pos " << position
    << " abr " << abadrep << " ta " << a << " a " << rotation
    << " dp " << v << " da " << da << std::endl;
    */
  }
  // fire control. Only one new shot per frame
  auto freeSlotsMask = freeWeaponSlots();
  if (shots.size() < weaponLocations[typeIndex()].size())
  {
    for (auto const& s: ships)
    {
      if (&s->player == &player || !s->config.moving)
        continue;
      for (int shift = 0; 1 << shift <= freeSlotsMask; ++shift)
      {
        if (((1 << shift) & freeSlotsMask) == 0)
          continue;
        auto wl = weaponLocations[typeIndex()][shift];
        auto awl = absolute(wl);
        if ((awl-s->position).length() < config.range)
        {
          fireAt(s, shift);
          break;
        }
      }
    }
  }
  if (shots.size() < weaponLocations[typeIndex()].size())
  {
    for (auto const& b: buildings)
    {
      if (b->dead)
        continue;
      for (int shift = 0; 1 << shift <= freeSlotsMask; ++shift)
      {
        if (((1 << shift) & freeSlotsMask) == 0)
          continue;
        auto wl = weaponLocations[typeIndex()][shift];
        auto awl = absolute(wl);
        if ((awl-b->center).length() < config.range)
        {
          shots.push_back(Laser{shift, nullptr, b, randomBuildingHitLocation(), now(), true});
          shots.back().pix = new QGraphicsLineItem();
          shots.back().pix->setPen(QPen(
            (player.flip()? QColor(70, 70, 255) : QColor(255, 255, 70))
            ));
          player.game().scene().addItem(shots.back().pix);
          break;
        }
      }
    }
  }
}