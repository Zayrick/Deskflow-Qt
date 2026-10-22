/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2021 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ComputerList.h"

#include <QLineF>

#include <algorithm>
#include <cmath>

namespace {

bool overlaps(const QRectF &a, const QRectF &b)
{
  const auto overlapX = std::min(a.right(), b.right()) - std::max(a.left(), b.left());
  const auto overlapY = std::min(a.bottom(), b.bottom()) - std::max(a.top(), b.top());
  return overlapX > ComputerList::kTouchTolerance && overlapY > ComputerList::kTouchTolerance;
}

} // namespace

qsizetype ComputerList::appendToRight(Computer newComputer)
{
  if (isEmpty()) {
    newComputer.setPosition(QPoint(0, 0));
  } else {
    const auto rightMost = std::ranges::max_element(*this, {}, [](const Computer &c) { return c.rect().right(); });
    const auto anchor = rightMost->rect();
    newComputer.setPosition(QPoint(static_cast<int>(std::ceil(anchor.right())), static_cast<int>(anchor.top())));
  }
  append(newComputer);
  return size() - 1;
}

qsizetype ComputerList::indexOfName(const QString &name) const
{
  for (qsizetype i = 0; i < size(); ++i) {
    if (!at(i).isNull() && at(i).name() == name)
      return i;
  }
  return -1;
}

qsizetype ComputerList::serverIndex() const
{
  for (qsizetype i = 0; i < size(); ++i) {
    if (at(i).isServer())
      return i;
  }
  return -1;
}

bool ComputerList::overlapsAny(const QRectF &rect, qsizetype ignoreIndex) const
{
  for (qsizetype i = 0; i < size(); ++i) {
    if (i != ignoreIndex && !at(i).isNull() && overlaps(rect, at(i).rect()))
      return true;
  }
  return false;
}

QRectF ComputerList::boundingRect() const
{
  QRectF bounds;
  for (const auto &computer : *this) {
    if (!computer.isNull())
      bounds = bounds.isNull() ? computer.rect() : bounds.united(computer.rect());
  }
  return bounds;
}

bool ComputerList::resolveOverlap(qsizetype index)
{
  if (index < 0 || index >= size())
    return false;

  auto &computer = operator[](index);
  const auto rect = computer.rect();
  if (!overlapsAny(rect, index))
    return true;

  // Candidate positions put the computer flush against one side of any other computer.
  QList<QPointF> candidates;
  for (qsizetype i = 0; i < size(); ++i) {
    if (i == index || at(i).isNull())
      continue;
    const auto other = at(i).rect();
    candidates.append({other.left() - rect.width(), rect.top()});
    candidates.append({other.right(), rect.top()});
    candidates.append({rect.left(), other.top() - rect.height()});
    candidates.append({rect.left(), other.bottom()});
  }
  std::ranges::sort(candidates, {}, [&rect](const QPointF &p) { return QLineF(rect.topLeft(), p).length(); });

  for (const auto &candidate : std::as_const(candidates)) {
    const QPoint position(static_cast<int>(std::round(candidate.x())), static_cast<int>(std::round(candidate.y())));
    if (!overlapsAny(QRectF(position, rect.size()), index)) {
      computer.setPosition(position);
      return true;
    }
  }
  return false;
}
