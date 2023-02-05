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

class BuildingShipYard: public Building
{
public:
  BuildingShipYard(Player& player)
  : Building(player, Asset::BuildingBase)
  {
    graphics = new ClickablePixmap(game().getAsset(Asset::BuildingShipYard),
      [this](){onClick();});
  }
  void make(QLayout* layout, Asset what)
  {
    auto* b = new ClickablePushButton([this, what] { enqueue(what);});
    b->setIcon(QIcon(game().getAsset(what)));
    b->setIconSize(QSize(Game::buildingSize/2, Game::buildingSize/2));
    layout->addWidget(b);
  }
  void redrawQueue()
  {
    for (int i=0;i<_queue.size();++i)
      _queueButtons[i]->setIcon(QIcon(game().getAsset(_queue[i])));
    for (int i=_queue.size(); i<16;++i)
      _queueButtons[i]->setIcon(QIcon::fromTheme("call-stop"));
  }
  void removeQueue(int pos)
  {
    if (pos >= _queue.size())
      return;
    _queue.erase(_queue.begin() + pos);
    redrawQueue();
  }
  void startProducing(Asset what)
  {
    _producing = what;
    _producingButton->setIcon(QIcon(game().getAsset(what)));
    _productionStart = now();
    _timer = new QTimer();
    _timer->connect(_timer, &QTimer::timeout,
      std::bind(&BuildingShipYard::updateProduction, this));
    _timer->start(50);
  }
  void updateProduction()
  {
    auto elapsed = now() - _productionStart;
    typedef std::chrono::duration<float> float_seconds;
    auto secs = std::chrono::duration_cast<float_seconds>(elapsed).count();
    if (elapsed > std::chrono::seconds(10))
    {
      player().placeShip(_producing);
      _progress->setValue(0);
      if (_queue.empty())
      {
        _producingButton->setIcon(QIcon::fromTheme("call-stop"));
        _timer->stop();
        delete _timer;
        _timer = nullptr;
        _producing = Asset::AssetEnd;
      }
      else
      {
        _producing = _queue.front();
        _producingButton->setIcon(QIcon(game().getAsset(_producing)));
        _queue.erase(_queue.begin());
        _productionStart = now();
        redrawQueue();
      }
    }
    else
    {
      _progress->setValue(secs * 1000.0 / 10.0);
    }
  }
  void enqueue(Asset what)
  {
    if (_producing == Asset::AssetEnd)
      startProducing(what);
    else if (_queue.size() < 16)
    {
      _queue.push_back(what);
      _queueButtons[_queue.size()-1]->setIcon(QIcon(game().getAsset(what)));
    }
  }
  void onClick()
  {
    if (menu == nullptr)
    {
      QGroupBox *groupBox = new QGroupBox("Ship Yard");
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
      make(l, Asset::ShipFighter);
      make(l, Asset::ShipFrigate);
      make(l, Asset::ShipCruiser);
      layout->addLayout(l);
      layout->addWidget(new QLabel("production queue"));
      for (int y=0;y<4;++y)
      {
        l = new QBoxLayout(QBoxLayout::LeftToRight);
        for (int x=0;x<4;++x)
        {
          auto* b = new ClickablePushButton([this, pos=x+y*4] { removeQueue(pos);});
          b->setIcon(QIcon::fromTheme("call-stop"));
          b->setIconSize(QSize(Game::buildingSize/2, Game::buildingSize/2));
          _queueButtons.push_back(b);
          l->addWidget(b);
        }
        layout->addLayout(l);
      }
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
  std::vector<Asset> _queue;
  std::vector<QPushButton*> _queueButtons;
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
  case Asset::BuildingShipYard:
    b = new BuildingShipYard(*this);
    break;
  default:
    throw std::runtime_error("cannot build this");
  }
  _buildings.push_back(b);
  _game.addBuilding(*b, _buildings.size()-1);
}

void Player::placeShip(Asset type)
{
  auto s = std::make_shared<Ship>(game().config.ships[(int)type-(int)Asset::ShipFighter],
    *this, type,
    P2{game().w/2 + (rand()%50)-25, _flip? 120 : game().h - 120}
    );
  _game.addShip(s);
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
  config = Config
  {
    {//time accel  speed aspd  hp     dmg  asopt asmax
      {  8.0, 20.0, 60.0, 1.2,  30.0, 1.0, 2.0, 4.0 },
      { 19.5, 15.0, 40.0, 1.0,  60.0, 1.0, 1.5, 3.0 },
      { 38.0, 15.0, 30.0, 0.8, 120.0, 1.0, 0.2, 0.8 },
    },
    200.0
  };
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
  auto* t = new QTimer();
  t->connect(t, &QTimer::timeout,
      std::bind(&Game::update, this));
  t->start(50);
  _lastUpdate = now();
  app.exec();
}

Player& Game::otherPlayer(Player const& p)
{
  if (&p == players[0])
    return *players[1];
  else
    return *players[0];
}
void Game::update()
{
  auto delapsed = now() - _lastUpdate;
  typedef std::chrono::duration<float> float_seconds;
  auto elapsed = std::chrono::duration_cast<float_seconds>(delapsed).count();
  _lastUpdate = now();
  for (auto& sptr: _ships)
  {
    auto& s = *sptr;
    s.think(_ships, otherPlayer(s.player).buildings());
    s.rotation += s.angularSpeed * elapsed;
    s.speed.x += elapsed * s.acceleration * sin(s.rotation) * -1.0;
    s.speed.y += elapsed * s.acceleration * cos(s.rotation) * -1.0;
    float smag = s.speed.length();
    P2 snorm = s.speed.normalize();
    smag -= sgn(smag) * std::min((double)smag, elapsed * 10.0);
    s.speed = snorm * smag;
    //s.speed.x -= sgn(s.speed.x) * std::min((double)fabs(s.speed.x), elapsed * 10.0);
    //s.speed.y -= sgn(s.speed.y) * std::min((double)fabs(s.speed.y), elapsed * 10.0);
    s.position.x += s.speed.x * elapsed;
    s.position.y += s.speed.y * elapsed;
    s.pix->setPos(s.position.x, s.position.y);
    s.pix->setRotation(s.rotation * 180.0 / 3.14159);
    // shots timeout/out of range
    for (int i=0; i< s.shots.size(); ++i)
    {
      auto& shot = s.shots[i];
      P2 targetPos = shot.targetShip ? shot.targetShip->position : shot.targetBuilding->center;   
      if (now() - shot.start > std::chrono::seconds(1)
        || (targetPos-s.absolute(Ship::weaponLocations[s.typeIndex()][shot.slot])).length() > 70.0)
      {
        std::swap(s.shots[i], s.shots[s.shots.size()-1]);
        s.shots.pop_back();
      }
      else if (shot.hit)
      { // deal damage
        if (shot.targetShip)
          shot.targetShip->hp -= elapsed * s.config.damage;
        else
          shot.targetBuilding->hp -= elapsed * s.config.damage;
      }
    }
  }
  // remove dead ships
  for (int i=0; i<_ships.size(); ++i)
  {
    if (_ships[i]->hp <= 0.0)
    {
      _scene.removeItem(_ships[i]->pix);
      std::swap(_ships[i], _ships[_ships.size()-1]);
      _ships.pop_back(); // todo: dramatic explosion
    }
  }
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
    b.center = P2{h-buildingSize*slot-buildingSize/2, buildingSize/2};
  }
  else
  {
    item->setPos(buildingSize*slot, h-buildingSize);
    b.center = P2{buildingSize*slot+buildingSize/2, h-buildingSize/2};
  }
}

void Game::addShip(ShipPtr ship)
{
  _ships.push_back(ship);
  _scene.addItem(ship->pix);
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

Ship::Ship(ShipConfig const& config, Player& p, Asset st, P2 pos)
: position(pos), shipType(st), player(p), config(config)
{
  hp = config.hp;
  patrolPoint = destination + P2{rand()%160-80, rand()%160-80};
  pix = new QGraphicsPixmapItem(p.game().getAsset(st, p.flip()));
}

int Ship::freeWeaponSlots()
{
  int res = (1 << (weaponLocations[typeIndex()].size()+1)) - 1;
  for (auto const& s: shots)
    res &= ~ (1 << s.slot);
  return res;
}
int Ship::freeWeaponSlot()
{
  std::vector<bool> isBusy;
  isBusy.resize(weaponLocations[typeIndex()].size());
  for (auto const& s: shots)
    isBusy[s.slot] = true;
  return std::find(isBusy.begin(), isBusy.end(), false) - isBusy.begin();
}

P2 Ship::absolute(P2 pointOnShip)
{
  auto a = atan2(-pointOnShip.y, pointOnShip.x)-M_PI/2.0;
  auto len = pointOnShip.length();
  auto aa = a + rotation;
  return P2 { position.x + len * sin(aa), position.y + len * cos(aa)};
}

std::vector<std::vector<P2>> Ship::weaponLocations = {
{{0, -5}},
{{-8, -4}, {8, -4}},
{{-20, -11}, {20, -11}, {-20, 7}, { 20, 7}}
};