/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QLineF>
#include <QList>
#include <QString>

#include <cstdint>

class ComputerList;

namespace deskflow::gui {

/// Link between part of an edge of one computer and part of the facing edge of another, derived from the layout.
struct ComputerLink
{
  enum class Side : int8_t
  {
    Left,
    Right,
    Up,
    Down
  };

  QString source;
  Side side = Side::Left;
  /// Interval along the source edge in percent (0-100), measured from the top / left end.
  double sourceStart = 0;
  double sourceEnd = 100;
  QString target;
  /// Interval along the target edge in percent (0-100).
  double targetStart = 0;
  double targetEnd = 100;

  /// Formats the link the way the server config's links section expects, e.g. `right(0,50) = other(25,100)`.
  [[nodiscard]] QString toString() const;

  bool operator==(const ComputerLink &other) const = default;
};

/// Computers whose edges are closer than this (canvas units) are considered touching.
inline constexpr double kLinkTolerance = 1.0;

/**
 * @brief computeLinks finds every shared edge segment between computers.
 *
 * Intervals are rounded to 0.01 percent, links on the same source edge never overlap after rounding and links that
 * would collapse to an empty interval are dropped, so the server always accepts the result.
 */
QList<ComputerLink> computeLinks(const ComputerList &computers);

/// The shared edge segments in canvas coordinates, each one reported once.
QList<QLineF> sharedEdges(const ComputerList &computers);

/// Name of the side as used in the server config.
QString sideName(ComputerLink::Side side);

} // namespace deskflow::gui
