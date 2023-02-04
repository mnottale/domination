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

enum class Asset
{
  ShipFighter,
  ShipFrigate,
  ShipCruiser,
  BuildingBase,
  BuildingTurret,
  BuildingPowerGenerator,
  BuildingMissileLauncher,
  BuildingShipYard,
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
  bool flip() { return _flip;}
  void showMenu(QWidget* widget);
  Game& game() { return _game;}
protected:
  Game& _game;
  std::vector<Building*> _buildings;
  bool _flip;
};

class Game
{
public:
  Player* players[2];
  void setup();
  void run(QApplication& app);
  void showMenu(QWidget* widget, bool flip);
  void addBuilding(Building& b, int slot);
  QPixmap& getAsset(Asset asset, bool flip=false);
private:
  void loadAssets();
  std::vector<std::vector<QPixmap*>> _assetTextures;
  QGraphicsScene _scene;
  QGraphicsProxyWidget* _menus[2] = {nullptr, nullptr};
};