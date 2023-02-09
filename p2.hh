#pragma once
#include <iostream>
#include <math.h>

inline double sq(double v)
{
  return v*v;
}

class P2
{
public:
  double x;
  double y;
  P2() : x(0), y(0) {}
  P2(double x_, double y_) : x(x_), y(y_) {}
  double length() const { return std::sqrt(x*x + y*y);}
  P2 normalize() const; 
  P2 rotate90() const {
    return P2{y, -x};
  }
  double dot(P2 const& b) const {
    return x*b.x + y*b.y;
  }
  P2 bounce(P2 const& axis) const;
  P2 aclamp(double v) const {
    return P2{
      std::max(std::min(v, x), -v),
      std::max(std::min(v, y), -v)
    };
  }
};

class V2
{
public:
  P2 start;
  P2 end;
};

inline P2 operator + (P2 const& a, P2 const& b) {
  return P2{a.x+b.x, a.y+b.y};
}

inline P2 operator - (P2 const& a, P2 const& b) {
  return P2{a.x-b.x, a.y-b.y};
}

inline P2 operator * (P2 const& a, double f) {
  return P2{a.x*f, a.y*f};
}
inline P2 operator / (P2 const& a, double f) {
  return P2{a.x/f, a.y/f};
}

inline bool operator == (const P2&a, const P2&b) {
  return a.x == b.x && a.y == b.y;
}

inline bool operator != (const P2&a, const P2&b) {
  return !(a==b);
}

inline P2 P2::normalize() const
{
  if (length() == 0.0)
    return *this;
  return *this / length();
}

inline P2 P2::bounce(P2 const& axis) const
{
  auto an = axis.rotate90().normalize();
  return *this - an * 2.0 * an.dot(*this);
}
inline std::ostream& operator << (std::ostream& os, P2 const& p)
{
  return os << '(' << p.x << ',' << p.y << ')';
}