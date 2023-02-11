#include <QApplication>
#include <QTabWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsProxyWidget>
#include <QGroupBox>
#include <QSlider>
#include <QPushButton>
#include <QTimer>
#include <QProgressBar>
#include <QLabel>
#include <QFormLayout>
#include <QLineEdit>

#include <set>

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
class ManagedAnimation: public QGraphicsPixmapItem
{
public:
  ManagedAnimation(Game& game, AnimatedAsset asset, int frameTimeMs, double scale)
  :QGraphicsPixmapItem(*game.getAnimatedAsset(asset)[0])
  , _game(game)
  , _a(game.getAnimatedAsset(asset))
  {
    setScale(scale);
    _timer = new QTimer();
    _timer->connect(_timer, &QTimer::timeout, std::bind(&ManagedAnimation::onTimer, this));
    _timer->setInterval(frameTimeMs);
    _timer->start();
  }
  void onTimer()
  {
    ++_idx;
    if (_idx >= _a.size())
    {
      _timer->stop();
      delete _timer;
      _game.scene().removeItem(this);
      delete this;
      return;
    }
    setPixmap(*_a[_idx]);
  }
private:
  Game& _game;
  const std::vector<QPixmap*>& _a;
  QTimer* _timer;
  int _idx;
};
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

void Building::activate()
{
  _excl->setPos(graphics->pos());
  _excl->setRotation(graphics->rotation());
  _excl->setZValue(3);
}

void Building::setWarning(bool enabled)
{
  if (enabled == _exclOn)
    return;
  if (enabled)
    game().scene().addItem(_excl);
  else
    game().scene().removeItem(_excl);
  _exclOn = enabled;
}

class BuildingCore: public Building
{
public:
  BuildingCore(Player& player, Asset type)
  :Building(player, type)
  {
    graphics = new ClickablePixmap(game().getAsset(type),
      [this](){show();});
  }
  void show()
  {
    // redirect click to first instance
    for(auto* b: player().buildings())
    {
      if (b->assetType() == assetType())
      {
        static_cast<BuildingCore*>(b)->onClick();
        break;
      }
    }
  }
  virtual void onClick() = 0;
};

class BuildingMissileLauncher: public BuildingCore
{
public:
  BuildingMissileLauncher(Player& player)
  : BuildingCore(player, Asset::BuildingMissileLauncher)
  {
  }
  void launch(int to)
  {
    if (_progress->value()<1000)
      return;
    _progress->setValue(0);
    delete _timer;
    _timer = nullptr;
    double gh = game().h;
    double tx = ((double)(to/5) + 0.5)/5.0 * gh;
    double ty = ((double)(to%5) + 0.5)/5.0 * gh;
    if (player().flip())
    {
      tx = game().h-tx;
      ty = game().h-ty;
    }
    auto ship = std::make_shared<Ship>(game().config.ships[(int)Asset::MissileEMP], player(), Asset::MissileEMP, center);
    ship->patrolPoint = ship->destination = P2{tx, ty};
    game().addShip(ship);
  }
  void updateProduction()
  {
    auto elapsed = now() - _productionStart;
    typedef std::chrono::duration<float> float_seconds;
    auto secs = std::chrono::duration_cast<float_seconds>(elapsed).count();
    int bidx = buildingIndex(Asset::BuildingMissileLauncher);
    auto buildTime = 30.0
      * player().powerFactors[bidx]
      * pow(1.0 - game().config.dupBonuses[bidx], player().countOf[bidx]-1);
    _progress->setValue(std::min(secs * 1000.0 / buildTime, 1000.0));
    if (_progress->value() >= 1000)
    {
      setWarning(true);
      _timer->stop();
    }
  }
  void enqueue(Asset what)
  {
    if (_timer != nullptr)
      return;
    _productionStart = now();
    _timer = new QTimer();
    _timer->connect(_timer, &QTimer::timeout, std::bind(&BuildingMissileLauncher::updateProduction, this));
    _timer->start(50);
    setWarning(false);
  }
  void make(QLayout* layout, Asset what)
  {
    auto* b = new ClickablePushButton([this, what] { enqueue(what);});
    b->setIcon(QIcon(game().getAsset(what)));
    b->setIconSize(QSize(Game::buildingSize/2, Game::buildingSize/2));
    layout->addWidget(b);
  }
  void onClick() override
  {
    if (_menu == nullptr)
    {
      QGroupBox *groupBox = new QGroupBox("Missile launcher");
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
      make(layout, Asset::MissileEMP);
      layout->addWidget(new QLabel("Launch missile"));
      auto* quad = new QGroupBox("");
      quad->setMaximumWidth(180);
      auto* ql = new QGridLayout();
      ql->setHorizontalSpacing(1);
      ql->setVerticalSpacing(1);
      const int SZ = 5;
      for (int i=0; i <SZ*SZ;++i)
      {
        auto* b = new ClickablePushButton([this, idx=i]{launch(idx);});
        b->setText("x");
        b->setMinimumWidth(20);
        b->setMinimumHeight(20);
        b->setMaximumHeight(100);
        ql->addWidget(b, i%SZ, i/SZ);
      }
      quad->setLayout(ql);
      layout->addWidget(quad);
      groupBox->setLayout(layout);
      _menu = groupBox;
    }
    player().showMenu(_menu);
  }
private:
  QPushButton* _producingButton;
  QProgressBar* _progress;
  QWidget* _menu = nullptr;
  QTimer* _timer = nullptr;
  Time _productionStart;
};
class BuildingTurret: public BuildingCore
{
public:
  BuildingTurret(Player& player)
  : BuildingCore(player, Asset::BuildingTurret)
  {
    weaponConfig = game().config.turret;
    _timer = new QTimer();
    _timer->connect(_timer, &QTimer::timeout,
      std::bind(&BuildingTurret::updateBonus, this));
    _timer->start(500);
  }
  void onClick() override
  {
  }
  void activate() override
  {
    BuildingCore::activate();
    weapon = std::make_shared<Ship>(weaponConfig, player(), Asset::ShipFighter, center);
    game().addShip(weapon);
  }
  void updateBonus()
  {
    double pwr = 1.0 - player().powerFactors[buildingIndex(Asset::BuildingTurret)];
    pwr /= (double)player().countOf[buildingIndex(Asset::BuildingTurret)];
    double wd = game().config.turret.damage * (1.0 + pwr);
    if (prevWD != wd)
    {
      std::cerr << "turret damage " << prevWD << " -> " << wd << std::endl;
      weaponConfig.damage = wd;
      prevWD = wd;
    }
  }
  void destroyed() override
  {
    auto& ships = game().ships();
    auto it = std::find(ships.begin(), ships.end(), weapon);
    if (it != ships.end())
    {
      int idx = it - ships.begin();
      std::swap(ships[idx], ships[ships.size()-1]);
      ships.pop_back();
    }
    //std::erase(game().ships(), weapon);
    //game().ships().erase(std::find(game().ships().begin(), game().ships().end(), weapon));
    _timer->stop();
  }
private:
  double prevWD = 0;
  ShipConfig weaponConfig;
  ShipPtr weapon;
  QTimer* _timer;
};
class BuildingPowerGenerator: public BuildingCore
{
public:
  BuildingPowerGenerator(Player& player)
  : BuildingCore(player, Asset::BuildingPowerGenerator)
  {
  }
  void onClick() override
  {
    if (menu == nullptr)
    {
      constexpr const char* bn[] = {"Base", "Turr", "Miss", "Ship"};
      QGroupBox *groupBox = new QGroupBox("Power Generator");
      auto *layout = new QGridLayout();
      for (int i=0;i<4;i++)
      {
        auto* s = new QSlider(Qt::Vertical);
        _sliders[i] = s;
        s->setMaximum(100);
        s->setMinimum(0);
        s->setValue(25);
        s->connect(s, &QSlider::valueChanged,
          std::bind(&BuildingPowerGenerator::sliderChange, this, i, std::placeholders::_1));
        layout->addWidget(s, 0, i);
        layout->addWidget(new QLabel(bn[i]), 1, i);
      }
      groupBox->setLayout(layout);
      menu = groupBox;
    }
     _player.showMenu(menu);
  }
  void sliderChange(int idx, int value)
  {
    if (_nested)
      return;
    int rest = 0;
    for (int i=0;i<4;i++)
    {
      if (i != idx)
        rest += _sliders[i]->value();
    }
    if (rest + value != 100)
    {
      if (rest == 0)
        rest = 1;
      double factor = (double)(100-value) / (double)rest;
      _nested = true;
      for (int i=0;i<4;i++)
      {
        if (i != idx)
          _sliders[i]->setValue(_sliders[i]->value() * factor);
      }
      _nested = false;
    }
    int bidx = buildingIndex(Asset::BuildingPowerGenerator);
    double factor = pow(1.0+ game().config.dupBonuses[bidx], player().countOf[bidx]-1);
    for (int i=0;i<4;i++)
    {
      double raw = (double)_sliders[i]->value() / 100.0;
      double bonus = raw * game().config.powerBonuses[i] * factor;
      player().powerFactors[i] = 1.0 - bonus;
    }
  }
private:
  QWidget* menu = nullptr;
  QSlider * _sliders[4];
  bool _nested = false;
};
class BuildingShipYard: public BuildingCore
{
public:
  BuildingShipYard(Player& player)
  : BuildingCore(player, Asset::BuildingShipYard)
  {
  }
  void make(QLayout* layout, Asset what)
  {
    auto* b = new ClickablePushButton([this, what] { enqueue(what);});
    b->setIcon(QIcon(game().getAsset(what, player().flip())));
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
    int bidx = buildingIndex(Asset::BuildingShipYard);
    auto buildTime = game().config.ships[(int)_producing].buildTime
     * player().powerFactors[bidx]
     * pow(1.0 - game().config.dupBonuses[bidx], player().countOf[bidx]-1);
    if (secs > buildTime)
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
        setWarning(true);
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
      _progress->setValue(secs * 1000.0 / buildTime);
    }
  }
  void enqueue(Asset what)
  {
    if (_producing == Asset::AssetEnd)
    {
      startProducing(what);
      setWarning(false);
    }
    else if (_queue.size() < 16)
    {
      _queue.push_back(what);
      _queueButtons[_queue.size()-1]->setIcon(QIcon(game().getAsset(what)));
    }
  }
  void onClick() override
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
class BuildingBase: public BuildingCore
{
public:
  BuildingBase(Player& player)
  : BuildingCore(player, Asset::BuildingBase)
  {
  }
  void updateProduction()
  {
    auto elapsed = now() - _productionStart;
    typedef std::chrono::duration<float> float_seconds;
    auto secs = std::chrono::duration_cast<float_seconds>(elapsed).count();
    int bidx = buildingIndex(Asset::BuildingBase);
    auto buildTime = game().config.buildingBuildTime[(int)_producing-(int)Asset::BuildingBase]
     * player().powerFactors[bidx]
     * pow(1.0 - game().config.dupBonuses[bidx], player().countOf[bidx]-1);
    if (secs > buildTime)
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
      _progress->setValue(secs * 1000.0 / buildTime);
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
  void onClick() override
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
{
  _excl = new QGraphicsPixmapItem(player.game().getAsset(Asset::Exclamation));
}

Player::Player(Game& game, bool flip)
: _game(game)
, _flip(flip)
{
  waypoint = P2{_game.h/2 + (rand()%50)-25, _flip? 120 : _game.h - 120};
  spread = _game.h/5;
}

Game& Building::game() {return _player.game();}

void Player::showMenu(QWidget* widget)
{
  if (_current == widget)
  {
    showPlayerMenu();
  }
  else
  {
    _current = widget;
    _game.showMenu(widget, _flip);
  }
}

void Player::setWaypoint(int to)
{
  double gh = game().h;
  double tx = ((double)(to/5) + 0.5)/5.0 * gh;
  double ty = ((double)(to%5) + 0.5)/5.0 * gh;
  if (_flip)
  {
    tx = game().h-tx;
    ty = game().h-ty;
  }
  P2 tgt{tx,ty};
  waypoint = tgt;
  std::cerr << "waypoint set to " << tx << " " << ty << std::endl;
}

void Player::showPlayerMenu()
{
  if (_menu == nullptr)
  {
    QGroupBox *groupBox = new QGroupBox("Move ships");
    QBoxLayout *layout = new QBoxLayout(QBoxLayout::TopToBottom);
    //layout->addWidget(new QLabel("kind filter"));
    auto* l = new QBoxLayout(QBoxLayout::LeftToRight);
    for (int i=0; i<3;i++)
    {
      auto* b = new QPushButton();
      b->setIcon(QIcon(game().getAsset((Asset)i)));
      b->setIconSize(QSize(Game::buildingSize/2, Game::buildingSize/2));
      b->setCheckable(true);
      _kinds[i] = b;
      l->addWidget(b);
    }
    layout->addLayout(l);
    //layout->addWidget(new QLabel("zone filter"));
    auto* tw = new QTabWidget();
    tw->setMaximumWidth(200);
    tw->addTab(new QLabel("EVERYWHERE"), "all");
    for (int s=2; s <6; ++s)
    {
      _zones.emplace_back();
      auto* quad = new QGroupBox("");
      quad->setMaximumWidth(180);
      auto* ql = new QGridLayout();
      ql->setHorizontalSpacing(1);
      ql->setVerticalSpacing(1);
      for (int i=0; i <s*s;++i)
      {
        auto* b = new QPushButton("x");
        b->setMinimumWidth(20);
        b->setMinimumHeight(20);
        b->setMaximumHeight(100);
        //b->setMaximumWidth(20);
        b->setCheckable(true);
        ql->addWidget(b, i%s, i/s);
        _zones[s-2].push_back(b);
      }
      quad->setLayout(ql);
      tw->addTab(quad, std::to_string(s).c_str());
    }
    _tabs = tw;
    layout->addWidget(tw);
    //layout->addWidget(new QLabel("move to"));
    tw = new QTabWidget();
    tw->setMaximumWidth(200);
    for (int w=0; w<2; w++)
    {
      auto* quad = new QGroupBox("");
      quad->setMaximumWidth(180);
      auto* ql = new QGridLayout();
      ql->setHorizontalSpacing(1);
      ql->setVerticalSpacing(1);
      const int SZ = 5;
      for (int i=0; i <SZ*SZ;++i)
      {
        auto* b = new ClickablePushButton(
          w? (Callback)[this, idx=i]{setWaypoint(idx);}
          : (Callback)[this, idx=i]{moveShips(idx);});
        b->setText("x");
        b->setMinimumWidth(20);
        b->setMinimumHeight(20);
        b->setMaximumHeight(100);
        ql->addWidget(b, i%SZ, i/SZ);
      }
      quad->setLayout(ql);
      tw->addTab(quad, w? "WP" : "move");
    }
    auto* slspread = new QSlider(Qt::Vertical);
    slspread->setMinimum(game().h/20);
    slspread->setMaximum(game().h/4);
    slspread->setValue(game().h/5);
    slspread->connect(slspread, &QSlider::valueChanged, [this](int v){this->spread = v;});
    tw->addTab(slspread, "SPRD");
    layout->addWidget(tw);
    groupBox->setLayout(layout);
    _menu = groupBox;
  }
  _current = nullptr;
  _game.showMenu(_menu, _flip);
}
double clamp(double v, double min, double max)
{
  if (v < min)
    return min;
  if (v > max)
    return max;
  return v;
}

void Player::moveShips(int to)
{
  double gh = game().h;
  double tx = ((double)(to/5) + 0.5)/5.0 * gh;
  double ty = ((double)(to%5) + 0.5)/5.0 * gh;
  if (_flip)
  {
    tx = game().h-tx;
    ty = game().h-ty;
  }
  P2 tgt{tx,ty};
  auto ti = _tabs->currentIndex();
  auto* z = ti == 0 ? nullptr: &_zones[ti-1];
  double zc = ti+1;
  for (auto& s: game().ships())
  {
    if (&s->player != this)
      continue;
    if (!_kinds[(int)s->shipType]->isChecked())
      continue;
    if (ti == 0)
    {
      s->setDestination(tgt);
      continue;
    }
    int ix = clamp(s->position.x, 0, gh-1) / gh * zc;
    int iy = clamp(s->position.y, 0, gh-1) / gh * zc;
    if (_flip)
    {
      ix = zc-ix-1;
      iy = zc-iy-1;
    }
    if (!(*z)[ix*zc+iy]->isChecked())
      continue;
    s->setDestination(tgt);
  }
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
  case Asset::BuildingPowerGenerator:
    b = new BuildingPowerGenerator(*this);
    break;
  case Asset::BuildingTurret:
    b = new BuildingTurret(*this);
    break;
  case Asset::BuildingMissileLauncher:
    b = new BuildingMissileLauncher(*this);
    break;
  default:
    throw std::runtime_error("cannot build this");
  }
  _buildings.push_back(b);
  _game.addBuilding(*b, _buildings.size()-1);
  countOf[buildingIndex(type)]++;
}

void Player::placeShip(Asset type)
{
  auto s = std::make_shared<Ship>(game().config.ships[(int)type-(int)Asset::ShipFighter],
    *this, type,
    P2{game().h/2 + (rand()%50)-25, _flip? 120 : game().h - 120}
    );
  s->patrolPoint = s->destination = waypoint;
  _game.addShip(s);
}
static std::string assetName[] = {
  "ship_fighter*.png",
  "ship_frigate*.png",
  "ship_cruiser*.png",
  "missile_emp.png",
  "building_base.png",
  "building_turret.png",
  "building_missilelauncher.png",
  "building_shipyard.png",
  "building_powergenerator.png",
  "exclamation.png",
};
struct AnimatedAssetNames
{
  std::string pattern;
  int start;
  int end;
};

AnimatedAssetNames animatedAssetNames[] = {
  {"explosion/explosion-%02d.png", 1, 50},
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
  for (int i=0; i<(int)AnimatedAsset::AssetEnd; ++i)
  {
    char an[1024];
    auto& a = animatedAssetNames[i];
    _animatedAssets.emplace_back();
    for (int x = a.start; x < a.end; x++)
    {
      snprintf(an, 1024, ("assets/"+a.pattern).c_str(), x);
      _animatedAssets.back().push_back(new QPixmap(an));
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
    {//time accel  speed aspd  hp     dmg  asopt asmax range moving
      {  8.0, 20.0, 60.0, 1.2,  30.0, 1.0, 2.0, 4.0, 60, true},
      { 19.5, 15.0, 40.0, 1.0,  60.0, 1.0, 1.5, 3.0, 60, true},
      { 38.0, 15.0, 30.0, 0.8, 120.0, 1.0, 0.2, 0.8, 60, true},
      { 15.0, 40.0, 150.0, 2.0, 120.0, 13.0, 0.2, 0.8, 200, true},
    },
    200.0,
    //bas turr miss ship powr
    {0.2, 0.3, 0.3, 0.2}, //power factory bonuses
    {0.3, 0.4, 0.8, 0.3, 0.3}, // dup bonuses
    {8.0, 20.0, 60.0, 1.2,  30.0, 5.0, 3.0, 6.0, 90, false}, // turret
    //build time
    {60, 40, 40, 50, 40},
  };
  this->w = w;
  this->h = h;
  loadAssets();
  players[0] = new Player(*this, false);
  players[1] = new Player(*this, true);
  players[0]->placeBuilding(Asset::BuildingBase);
  players[1]->placeBuilding(Asset::BuildingBase);
  players[0]->placeBuilding(Asset::BuildingShipYard);
  players[1]->placeBuilding(Asset::BuildingShipYard);
  _scene.addLine(h, 0, h, h);
  _scene.setBackgroundBrush(QBrush(Qt::black));
  //_scene.addRect(0, 0, h, h, QPen(), QBrush(Qt::black));
}

void Game::run(QApplication& app)
{
  QGraphicsView view(&_scene);
  view.show();
  auto* t = new QTimer();
  t->connect(t, &QTimer::timeout,
      std::bind(&Game::update, this));
  t->start(10);
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
  std::set<Building*> hitBuilding;
  for (auto& sptr: _ships)
  {
    auto& s = *sptr;
    if (s.jammedUntil > now())
    {
      s.angularSpeed = 0;
      s.acceleration = 0;
    }
    else
      s.think(_ships, otherPlayer(s.player).buildings());
    if (s.config.moving)
    {
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
      auto asz = assetSize[(int)s.shipType];
      auto d = (double)asz * sqrt(2.0) / 2.0;
      auto dx = d * sin(s.rotation + M_PI / 4.0);
      auto dy = d * cos(s.rotation + M_PI / 4.0);
      s.pix->setPos(s.position.x-dx, s.position.y-dy);
      s.pix->setRotation(-s.rotation * 180.0 / M_PI);
    }
    // shots timeout/out of range
    for (int i=0; i< s.shots.size(); ++i)
    {
      auto& shot = s.shots[i];
      P2 targetPos = shot.targetShip ? shot.targetShip->position : shot.targetBuilding->center;   
      if (now() - shot.start > std::chrono::seconds(1)
        || (targetPos-s.absolute(Ship::weaponLocations[s.typeIndex()][shot.slot])).length() > s.config.range*1.15)
      {
        scene().removeItem(shot.pix);
        delete shot.pix;
        std::swap(s.shots[i], s.shots[s.shots.size()-1]);
        s.shots.pop_back();
      }
      else 
      {
        if (shot.hit)
        { // deal damage
          if (shot.targetShip)
            shot.targetShip->hp -= elapsed * s.config.damage;
          else
          {
            shot.targetBuilding->hp -= elapsed * s.config.damage;
            hitBuilding.insert(shot.targetBuilding);
          }
        }
        // draw shot
        P2 hl;
        if (shot.targetShip)
          hl = shot.targetShip->absolute(shot.hitLocation);
        else
          hl = shot.targetBuilding->center + shot.hitLocation;
        P2 sl = Ship::weaponLocations[s.typeIndex()][shot.slot];
        sl = s.absolute(sl);
        shot.pix->setLine(hl.x, hl.y, sl.x, sl.y);
      }
    }
  }
  // update building hp
  for(auto* b: hitBuilding)
  {
    b->healthBar->setRect(0,0,b->hp/b->hpMax * (double)buildingSize, 4);
    if (b->hp <= 0)
    {
      auto* ma = new ManagedAnimation(*this, AnimatedAsset::Explosion, 14, (double)buildingSize/200.0);
      ma->setPos(b->graphics->pos());
      ma->setRotation(b->graphics->rotation());
      _scene.addItem(ma);
      _scene.removeItem(b->graphics);
      _scene.removeItem(b->healthBar);
      b->destroyed();
      b->dead = true;
    }
  }
  // remove dead ships
  for (int i=0; i<_ships.size(); ++i)
  {
    if (_ships[i]->hp <= 0.0)
    {
      for (auto& shot: _ships[i]->shots)
      {
        _scene.removeItem(shot.pix);
        delete shot.pix;
      }
      _scene.removeItem(_ships[i]->pix);
      auto* ma = new ManagedAnimation(*this, AnimatedAsset::Explosion, 14, (double)assetSize[(int)_ships[i]->shipType]/200.0);
      ma->setPos(_ships[i]->pix->pos());
      ma->setRotation(_ships[i]->pix->rotation());
      _scene.addItem(ma);
      std::swap(_ships[i], _ships[_ships.size()-1]);
      _ships.pop_back(); // todo: dramatic explosion
    }
  }
}

std::vector<QPixmap*>const& Game::getAnimatedAsset(AnimatedAsset asset)
{
  return _animatedAssets[(int)asset];
}

void Game::addBuilding(Building& b, int slot)
{
  bool flip = b.player().flip();
  auto item = b.graphics;
  b.healthBar = new QGraphicsRectItem(0, 0, buildingSize, 4);
  b.healthBar->setBrush(QBrush(Qt::green));
  _scene.addItem(item);
  _scene.addItem(b.healthBar);
  if (flip)
  {
    item->setRotation(180);
    item->setPos(h-buildingSize*slot, buildingSize+4);
    b.center = P2{h-buildingSize*slot-buildingSize/2, buildingSize/2};
    b.healthBar->setRotation(180);
    b.healthBar->setPos(h-buildingSize*slot, 4);
  }
  else
  {
    item->setPos(buildingSize*slot, h-buildingSize);
    b.center = P2{buildingSize*slot+buildingSize/2, h-buildingSize/2};
    b.healthBar->setPos(buildingSize*slot, h-4);
  }
  b.activate();
}

void Game::addShip(ShipPtr ship)
{
  _ships.push_back(ship);
  if (ship->config.moving)
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
  destination = pos;
  patrolPoint = destination + P2{rand()%160-80, rand()%160-80};
  pix = new QGraphicsPixmapItem(p.game().getAsset(st, p.flip()));
  pix->setZValue(10);
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
{{-20, -11}, {20, -11}, {-20, 7}, { 20, 7}},
{}
};