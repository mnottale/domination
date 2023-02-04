#include <QApplication>
#include <QTabWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsProxyWidget>
#include <QGroupBox>
#include <QLabel>
#include <QFormLayout>
#include <QLineEdit>

#include "qttest.hpp"

QWidget* make()
{
  QGroupBox *groupBox = new QGroupBox("Contact Details");
  QLabel *numberLabel = new QLabel("Telephone number");
  QLineEdit *numberEdit = new QLineEdit;

  QFormLayout *layout = new QFormLayout;
  layout->addRow(numberLabel, numberEdit);
  groupBox->setLayout(layout);
  return groupBox;
}
int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  Game g;
  g.setup();
  g.run(app);
  
 /*
    QApplication app(argc, argv);

    QGraphicsScene scene;
    QGraphicsProxyWidget *proxya = scene.addWidget(make());
    QGraphicsProxyWidget *proxyb = scene.addWidget(make());
    QGraphicsProxyWidget *proxyc = scene.addWidget(make());
    proxyb->setRotation(90);
    proxyb->setPos(100,100);
    proxyc->setRotation(45);
    proxyc->setPos(800, 200);

    QGraphicsView view(&scene);
    view.show();

    return app.exec();
    */
}
using Callback = std::function<void()>;
class ClickablePixmap: public QGraphicsPixmapItem
{
public:
  ClickablePixmap(QPixmap& pixmap, Callback onClick)
  : QGraphicsPixmapItem(pixmap)
  , _onClick(onClick)
  {
  }
  void mousePressEvent(QGraphicsSceneMouseEvent *event) override
 {
   std::cerr << "CLICK " << std::endl;
   _onClick();
 }
private:
  Callback _onClick;
};

class BuildingBase: public Building
{
public:
  BuildingBase(Player& player)
  : Building(player, Asset::BuildingBase)
  {
    graphics = new ClickablePixmap(game().getAsset(Asset::BuildingBase),
      [this](){onClick();});
  }
  void onClick()
  {
    if (menu == nullptr)
    {
      menu = make();
    }
    _player.showMenu(menu);
  }
private:
  QWidget* menu = nullptr;
};

Building::Building(Player& player, Asset asset)
: _player(player)
, _asset(asset)
{}

Player::Player(Game& game, bool flip)
: _game(game)
, _flip(flip)
{
}
Game& Building::game() {return _player.game();}

void Player::showMenu(QWidget* widget)
{
  _game.showMenu(widget, _flip);
}

void Player::placeBuilding(Asset type)
{
  Building* b = nullptr;
  switch (type)
  {
  case Asset::BuildingBase:
    b = new BuildingBase(*this);
    break;
  default:
    throw new std::runtime_error("cannot build this");
  }
  _buildings.push_back(b);
  _game.addBuilding(*b, _buildings.size()-1);
}

static std::string assetName[] = {
  "ship_fighter*.png",
  "ship_frigate*.png",
  "ship_cruiser*.png",
  "building_base.png",
  "building_turret.png",
  "building_powergenerator.png",
  "building_missilelauncher.png",
  "building_shipyard.png"
};
void Game::loadAssets()
{
  for (int i=0; i<(int)Asset::AssetEnd; ++i)
  {
    std::string const& name = assetName[i];
    _assetTextures.emplace_back();
    auto star = name.find_first_of('*');
    if (star == std::string::npos)
      _assetTextures.back().push_back(new QPixmap(("assets/" + name).c_str()));
    else
    {
      auto b = name.substr(0, star) + "_blue" + name.substr(star+1);
      auto y = name.substr(0, star) + "_yellow" + name.substr(star+1);
      _assetTextures.back().push_back(new QPixmap(("assets/" + b).c_str()));
      _assetTextures.back().push_back(new QPixmap(("assets/" + y).c_str()));
    }
  }
}

QPixmap& Game::getAsset(Asset asset, bool flip)
{
  int aa = (int)asset;
  auto const& ts = _assetTextures[aa];
  if (ts.size() == 1)
    return *ts[0];
  else
    return *ts[flip ? 0 : 1];
}

void Game::setup()
{
  loadAssets();
  players[0] = new Player(*this, false);
  players[1] = new Player(*this, true);
  players[0]->placeBuilding(Asset::BuildingBase);
  players[1]->placeBuilding(Asset::BuildingBase);
}

void Game::run(QApplication& app)
{
  QGraphicsView view(&_scene);
  view.show();
  app.exec();
}

void Game::addBuilding(Building& b, int slot)
{
  bool flip = b.player().flip();
  auto item = b.graphics;
  _scene.addItem(item);
  if (flip)
    item->setRotation(180);
}

void Game::showMenu(QWidget* widget, bool flip)
{
  int idx = flip ? 1:0;
  if (_menus[idx] != nullptr)
  {
    _scene.removeItem(_menus[idx]);
    delete _menus[idx];
  }
  _menus[idx] = _scene.addWidget(widget);
  if (flip)
    _menus[idx]->setRotation(180);
}