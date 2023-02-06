#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QWidget>
#include <QPixmap>
#include <functional>
#include <memory>
#include <chrono>
#include "p2.hh"

typedef std::chrono::high_resolution_clock::time_point Time;
Time now();

template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

enum class Asset
{
  ShipFighter,
  ShipFrigate,
  ShipCruiser,
  BuildingBase,
  BuildingTurret,
  BuildingMissileLauncher,
  BuildingShipYard,
  BuildingPowerGenerator,
  AssetEnd,
};
class Player;
class Game;
class Building
{
public:
  Building(Player& player, Asset asset);
  Asset assetType() { return _asset;}
  double hp = 200.0;
  double hpMax = 200.0;
  QGraphicsItem* graphics;
  Player& player() {return _player;}
  Game& game();
  P2 center;
protected:
  Player& _player;
  Asset _asset;
};
using BuildingPtr = std::shared_ptr<Building>;

class Player
{
public:
  Player(Game& game, bool flip);
  void placeBuilding(Asset type);
  void placeShip(Asset type);
  bool flip() { return _flip;}
  void showMenu(QWidget* widget);
  void showPlayerMenu();
  void moveShips(int to);
  Game& game() { return _game;}
  const std::vector<Building*> buildings() { return _buildings;}
protected:
  Game& _game;
  std::vector<Building*> _buildings;
  bool _flip;
  QWidget* _menu = nullptr;
  QWidget* _current = nullptr;
  QPushButton* _kinds[3];
  std::vector<std::vector<QPushButton*>> _zones;
  QTabWidget* _tabs;
};

class Ship;
using ShipPtr = std::shared_ptr<Ship>;

class Laser
{
public:
  int slot;
  ShipPtr targetShip;
  Building* targetBuilding;
  P2 hitLocation;
  Time start;
  bool hit;
  QGraphicsLineItem* pix;
};

struct ShipConfig
{
  double buildTime;
  double acceleration;
  double cruseSpeed;
  double angularSpeed;
  double hp;
  double damage;
  double aimOptimal; // optimal max angular velocity: always hit below
  double aimMax; // max angular velocity: always miss above
};

class Ship
{
public:
  Ship(ShipConfig const& config, Player& player, Asset shipType, P2 spawnPoint); 
  void think(std::vector<ShipPtr> const& ships, std::vector<Building*> const& buildings);
  int typeIndex() { return (int)shipType - (int)Asset::ShipFighter;}
  int freeWeaponSlot();
  int freeWeaponSlots(); // bitmask
  P2 absolute(P2 pointOnShip);
  void setDestination(P2 target);
  void fireAt(ShipPtr target, int weaponSlot);
  double angularVelocity(ShipPtr target);
  //state
  P2 position;
  double rotation = 0.0; // rad, 0 pointing up
  P2 speed = {0.0,0.0};
  std::vector<Laser> shots;
  double hp = 50.0;
  //control
  double acceleration = 0.0;
  double angularSpeed = 0.0;
  //state
  Asset shipType;
  Player& player;

  //IA
  P2 patrolPoint = {400.0, 400.0};
  P2 destination = {400.0, 400.0};

  // display
  QGraphicsPixmapItem* pix;

  ShipConfig const& config;
  static std::vector<std::vector<P2>> weaponLocations;
};

struct Config
{
  ShipConfig ships[3];
  double buildingHp;
};


class Game
{
public:
  Player* players[2];
  void setup(int w, int h);
  void run(QApplication& app);
  void update();
  void showMenu(QWidget* widget, bool flip);
  void addBuilding(Building& b, int slot);
  void addShip(ShipPtr ship);
  Player& otherPlayer(Player const& p);
  QPixmap& getAsset(Asset asset, bool flip=false);
  std::vector<ShipPtr> ships() { return _ships;}
  int w, h;
  Config config;
  QGraphicsScene& scene() { return _scene;}
  static const int buildingSize = 80;
  static const constexpr int assetSize[10] = {15, 30, 50, 80, 80, 80, 80, 80, 80, 80};
private:
  void loadAssets();
  std::vector<std::vector<QPixmap*>> _assetTextures;
  QGraphicsScene _scene;
  QGraphicsProxyWidget* _menus[2] = {nullptr, nullptr};
  std::vector<ShipPtr> _ships;
  Time _lastUpdate;
};