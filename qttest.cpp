#include <QApplication>
#include <QTabWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsProxyWidget>
#include <QGroupBox>
#include <QPushButton>
#include <QTimer>
#include <QProgressBar>
#include <QLabel>
#include <QFormLayout>
#include <QLineEdit>

#include "qttest.hpp"

Time now()
{
  static double dilatation = -1.0;
  if (dilatation == -1.0)
  {
    auto ds = getenv("DILATATION");
    if (ds == nullptr)
      dilatation = 1.0;
    else
      dilatation = std::stod(ds);
  }
  if (dilatation == 1.0)
    return std::chrono::high_resolution_clock::now();
  static Time start = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::high_resolution_clock::now()-start;
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
  us = (double)us * dilatation;
  return start + std::chrono::microseconds(us);
}

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
  g.setup(1600, 900);
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

class ClickablePushButton: public QPushButton
{
public:
  ClickablePushButton(Callback onClick)
  : _onClick(onClick)
  {
  }
  void mousePressEvent(QMouseEvent *event) override
  {
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
  void updateProduction()
  {
    auto elapsed = now() - _productionStart;
    if (elapsed > std::chrono::seconds(10))
    {
      _producingButton->setIcon(QIcon::fromTheme("call-stop"));
      _timer->stop();
      delete _timer;
      _timer = nullptr;
      _progress->setValue(0);
      player().placeBuilding(_producing);
    }
    else
    {
      typedef std::chrono::duration<float> float_seconds;
      auto secs = std::chrono::duration_cast<float_seconds>(elapsed).count();
      _progress->setValue(secs * 1000.0 / 10.0);
    }
  }
  void startProduction(Asset what)
  {
    if (_timer != nullptr)
    {
       _timer->stop();
       delete _timer;
       _timer = nullptr;
    }
    _producing = what;
    _producingButton->setIcon(QIcon(game().getAsset(what)));
    _productionStart = now();
    _timer = new QTimer();
    _timer->connect(_timer, &QTimer::timeout,
      std::bind(&BuildingBase::updateProduction, this));
    _timer->start(50);
  }
  void make(QLayout* layout, Asset what)
  {
    auto* b = new ClickablePushButton([this, what] { startProduction(what);});
    b->setIcon(QIcon(game().getAsset(what)));
    b->setIconSize(QSize(Game::buildingSize/2, Game::buildingSize/2));
    layout->addWidget(b);
  }
  void onClick()
  {
    if (menu == nullptr)
    {
      QGroupBox *groupBox = new QGroupBox("Main base");
      QBoxLayout *layout = new QBoxLayout(QBoxLayout::TopToBottom);
      layout->addWidget(new QLabel("currently producing"));
      QPushButton *button = new QPushButton;
      button->setIcon(QIcon::fromTheme("call-stop"));
      button->setIconSize(QSize(Game::buildingSize/2, Game::buildingSize/2));
      layout->addWidget(button);
      _producingButton = button;
      auto* bar = new QProgressBar();
      bar->setMinimum(0);
      bar->setMaximum(1000);
      layout->addWidget(bar);
      _progress = bar;
      layout->addWidget(new QLabel("start production:"));
      auto* l = new QBoxLayout(QBoxLayout::LeftToRight);
      make(l, Asset::BuildingBase);
      make(l, Asset::BuildingTurret);
      make(l, Asset::BuildingPowerGenerator);
      layout->addLayout(l);
      l = new QBoxLayout(QBoxLayout::LeftToRight);
      make(l, Asset::BuildingMissileLauncher);
      make(l, Asset::BuildingShipYard);
      layout->addLayout(l);
      groupBox->setLayout(layout);
      menu = groupBox;
    }
    _player.showMenu(menu);
  }
private:
  QWidget* menu = nullptr;
  Asset _producing = Asset::AssetEnd;
  QPushButton* _producingButton;
  QProgressBar* _progress;
  QTimer* _timer = nullptr;
  Time _productionStart;
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
    throw std::runtime_error("cannot build this");
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

void Game::setup(int w, int h)
{
  this->w = w;
  this->h = h;
  loadAssets();
  players[0] = new Player(*this, false);
  players[1] = new Player(*this, true);
  players[0]->placeBuilding(Asset::BuildingBase);
  players[1]->placeBuilding(Asset::BuildingBase);
  _scene.addLine(h, 0, h, h);
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
  {
    item->setRotation(180);
    item->setPos(h-buildingSize*slot, buildingSize);
  }
  else
    item->setPos(buildingSize*slot, h-buildingSize);
}

void Game::showMenu(QWidget* widget, bool flip)
{
  int idx = flip ? 1:0;
  if (_menus[idx] != nullptr)
  {
    _scene.removeItem(_menus[idx]);
    _menus[idx]->setWidget(nullptr);
    delete _menus[idx];
  }
  _menus[idx] = _scene.addWidget(widget);
  if (flip)
  {
    _menus[idx]->setRotation(180);
    _menus[idx]->setPos(h+widget->width(), widget->height());
  }
  else
  {
    _menus[idx]->setPos(h, h-widget->height());
  }
}