# Domination

Domination is a two player game for big touchscreen where each player builds
a fleet of mighty ships in an attempt to destroy the oponent's base.

![Screenshot](medias/domination.png?raw=true "Screenshot")

## Building

Optionally edit qttest.cpp main() to set resolution, scale and fullsceen mode.
Then:

    qmake qttest.pro
    make

## Playing

Tap any building to access it's interface. Tap the same building again to
access the ship control interface.

### Ship control

Double-tap a building to access the ship control interface. The left-part
determines which ships will receive the move order (by kind and quandrant position)
the right part sends move orders to those ships.

The right scroll bar control the spread of ships. The lower the more tightly
packed ships will be.

Once ships reach their destination point they will patrol continuously around it,
and automatically engage any target in range.

### Buildings

You can have multiple copies of each building, but they will only produce a
bonus in production time and not do anything of their own, except the turret.

#### Base

It produces other buildings.

#### Ship yard

Produces ships of three types, fighter, frigates and cruisers. Has a build queue.

#### Turret

Will attack enemy ships that come too close

#### Power plant

Produces speed bonuses to other building types. It's interface provides a way
to specify the power repartition.

#### Missile launcher

Produces and launches EMP missiles that will freeze ennemy ships around the
explosion point.

