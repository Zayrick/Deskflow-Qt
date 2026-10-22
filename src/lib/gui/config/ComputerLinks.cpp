/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ComputerLinks.h"

#include "ComputerList.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace deskflow::gui {

namespace {

using Side = ComputerLink::Side;

/// Overlap between two touching rectangles along the edge @p side of @p source, in canvas units.
struct Contact
{
  double start = 0;
  double end = 0;
};

bool contact(const QRectF &source, const QRectF &target, Side side, Contact &out)
{
  double sourceEdge = 0;
  double targetEdge = 0;
  switch (side) {
  case Side::Left:
    sourceEdge = source.left();
    targetEdge = target.right();
    break;
  case Side::Right:
    sourceEdge = source.right();
    targetEdge = target.left();
    break;
  case Side::Up:
    sourceEdge = source.top();
    targetEdge = target.bottom();
    break;
  case Side::Down:
    sourceEdge = source.bottom();
    targetEdge = target.top();
    break;
  }
  if (std::abs(sourceEdge - targetEdge) > kLinkTolerance)
    return false;

  const bool horizontal = side == Side::Left || side == Side::Right;
  out.start = horizontal ? std::max(source.top(), target.top()) : std::max(source.left(), target.left());
  out.end = horizontal ? std::min(source.bottom(), target.bottom()) : std::min(source.right(), target.right());
  return out.end - out.start > kLinkTolerance;
}

/// Position of @p value along the edge of @p rect that is crossed when travelling @p side, in percent.
double percentAlong(const QRectF &rect, Side side, double value)
{
  const bool horizontal = side == Side::Left || side == Side::Right;
  const double start = horizontal ? rect.top() : rect.left();
  const double length = horizontal ? rect.height() : rect.width();
  if (length <= 0)
    return 0;
  const double percent = std::round((value - start) / length * 10000.0) / 100.0;
  return std::clamp(percent, 0.0, 100.0);
}

QString formatInterval(double start, double end)
{
  if (start <= 0 && end >= 100)
    return {};
  return QStringLiteral("(%1,%2)").arg(QString::number(start), QString::number(end));
}

constexpr std::array kSides = {Side::Left, Side::Right, Side::Up, Side::Down};

} // namespace

QString sideName(Side side)
{
  switch (side) {
  case Side::Left:
    return QStringLiteral("left");
  case Side::Right:
    return QStringLiteral("right");
  case Side::Up:
    return QStringLiteral("up");
  case Side::Down:
    return QStringLiteral("down");
  }
  return {};
}

QString ComputerLink::toString() const
{
  return QStringLiteral("%1%2 = %3%4")
      .arg(sideName(side), formatInterval(sourceStart, sourceEnd), target, formatInterval(targetStart, targetEnd));
}

QList<ComputerLink> computeLinks(const ComputerList &computers)
{
  QList<ComputerLink> links;
  for (const auto &source : computers) {
    if (source.isNull())
      continue;
    const auto sourceRect = source.rect();
    for (const auto side : kSides) {
      QList<ComputerLink> edgeLinks;
      for (const auto &target : computers) {
        if (target.isNull() || &target == &source)
          continue;
        const auto targetRect = target.rect();
        Contact c;
        if (!contact(sourceRect, targetRect, side, c))
          continue;
        ComputerLink link;
        link.source = source.name();
        link.side = side;
        link.sourceStart = percentAlong(sourceRect, side, c.start);
        link.sourceEnd = percentAlong(sourceRect, side, c.end);
        link.target = target.name();
        link.targetStart = percentAlong(targetRect, side, c.start);
        link.targetEnd = percentAlong(targetRect, side, c.end);
        edgeLinks.append(link);
      }

      // The server refuses overlapping intervals on one edge, rounding (or overlapping computers) could produce them.
      std::ranges::sort(edgeLinks, {}, &ComputerLink::sourceStart);
      double previousEnd = 0;
      for (auto &link : edgeLinks) {
        link.sourceStart = std::max(link.sourceStart, previousEnd);
        if (link.sourceStart >= link.sourceEnd || link.targetStart >= link.targetEnd)
          continue;
        previousEnd = link.sourceEnd;
        links.append(link);
      }
    }
  }
  return links;
}

QList<QLineF> sharedEdges(const ComputerList &computers)
{
  QList<QLineF> edges;
  for (const auto &source : computers) {
    if (source.isNull())
      continue;
    const auto sourceRect = source.rect();
    // Right and down only, so every shared edge is reported once.
    for (const auto side : {Side::Right, Side::Down}) {
      for (const auto &target : computers) {
        if (target.isNull() || &target == &source)
          continue;
        Contact c;
        if (!contact(sourceRect, target.rect(), side, c))
          continue;
        if (side == Side::Right)
          edges.append(QLineF(sourceRect.right(), c.start, sourceRect.right(), c.end));
        else
          edges.append(QLineF(c.start, sourceRect.bottom(), c.end, sourceRect.bottom()));
      }
    }
  }
  return edges;
}

} // namespace deskflow::gui
