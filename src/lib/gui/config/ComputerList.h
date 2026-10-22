/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2021 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "Computer.h"

#include <QRectF>

/**
 * @brief The computers placed on the server's layout canvas.
 *
 * Only real computers are stored; their geometry (Computer::rect) decides which edges are linked.
 */
class ComputerList : public QList<Computer>
{
public:
  ComputerList() = default;

  /**
   * @brief appendToRight places a new computer to the right of the right most computer, top aligned with it so the
   * two share an edge, and appends it to the list. The first computer is placed at the origin.
   * @return the index of the appended computer
   */
  qsizetype appendToRight(Computer newComputer);

  /// @return index of the computer named @p name or -1
  [[nodiscard]] qsizetype indexOfName(const QString &name) const;

  /// @return index of the server computer or -1
  [[nodiscard]] qsizetype serverIndex() const;

  /// @return true if @p rect overlaps (not just touches) any computer other than the one at @p ignoreIndex
  [[nodiscard]] bool overlapsAny(const QRectF &rect, qsizetype ignoreIndex = -1) const;

  /// @return bounding rectangle of all computers on the canvas
  [[nodiscard]] QRectF boundingRect() const;

  /**
   * @brief resolveOverlap moves the computer at @p index by the smallest distance that makes it stop overlapping the
   * others, keeping it flush against the computer it was pushed away from.
   * @return false if no such position was found, the computer is left untouched in that case
   */
  bool resolveOverlap(qsizetype index);

  /// Overlap smaller than this (in canvas units) is treated as touching.
  static constexpr double kTouchTolerance = 1.0;
};
