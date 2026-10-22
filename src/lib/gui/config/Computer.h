/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "ComputerConfig.h"

#include "common/KeyboardModifier.h"
#include "common/QSettingsProxy.h"

#include <QIcon>
#include <QList>
#include <QPixmap>
#include <QPoint>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>

class QSettings;
class QTextStream;
class ComputerSettingsDialog;

class Computer : public ComputerConfig
{
  friend class ComputerSettingsDialog;
  friend class ComputerLayoutView;

  friend QDataStream &operator<<(QDataStream &outStream, const Computer &computer)
  {
    return outStream << computer.name() << computer.switchCornerSize() << computer.aliases() << computer.modifiers()
                     << computer.switchCorners() << computer.fixes() << computer.isServer() << computer.m_Position
                     << computer.m_Resolution << computer.m_Scale;
  }

  friend QDataStream &operator>>(QDataStream &inStream, Computer &computer)
  {
    return inStream >> computer.m_Name >> computer.m_SwitchCornerSize >> computer.m_Aliases >> computer.m_Modifiers >>
           computer.m_SwitchCorners >> computer.m_Fixes >> computer.m_isServer >> computer.m_Position >>
           computer.m_Resolution >> computer.m_Scale;
  }

public:
  /// Native resolution assumed for computers whose size is not known yet.
  inline static const QSize kDefaultResolution{1920, 1080};
  static constexpr double kMinScale = 0.05;
  static constexpr double kMaxScale = 20.0;

  explicit Computer(const QString &name = QString());

  [[nodiscard]] const QPixmap &pixmap() const
  {
    return m_Pixmap;
  }
  [[nodiscard]] const QString &name() const
  {
    return m_Name;
  }
  [[nodiscard]] const QStringList &aliases() const
  {
    return m_Aliases;
  }

  [[nodiscard]] bool isNull() const
  {
    return m_Name.isEmpty();
  }
  [[nodiscard]] int modifier(int m) const
  {
    return m_Modifiers[m] == static_cast<int>(KeyboardModifier::DefaultMod) ? m : m_Modifiers[m];
  }
  [[nodiscard]] const QList<int> &modifiers() const
  {
    return m_Modifiers;
  }
  [[nodiscard]] bool switchCorner(int c) const
  {
    return m_SwitchCorners[c];
  }
  [[nodiscard]] const QList<bool> &switchCorners() const
  {
    return m_SwitchCorners;
  }
  [[nodiscard]] int switchCornerSize() const
  {
    return m_SwitchCornerSize;
  }
  [[nodiscard]] bool fix(const Fix f) const
  {
    return m_Fixes[static_cast<int8_t>(f)];
  }
  [[nodiscard]] const QList<bool> &fixes() const
  {
    return m_Fixes;
  }

  void loadSettings(QSettingsProxy &settings);
  void saveSettings(QSettingsProxy &settings) const;
  /// True if the current settings array entry carries layout geometry (configs older than the free layout don't).
  static bool hasGeometry(const QSettingsProxy &settings);
  [[nodiscard]] QString screensSection() const;

  [[nodiscard]] bool swapped() const
  {
    return m_Swapped;
  }

  void setName(const QString &name)
  {
    m_Name = name;
  }
  [[nodiscard]] bool isServer() const
  {
    return m_isServer;
  }
  void markAsServer()
  {
    m_isServer = true;
  }

  /// Top left corner on the layout canvas, in canvas units.
  [[nodiscard]] QPoint position() const
  {
    return m_Position;
  }
  void setPosition(const QPoint &position)
  {
    m_Position = position;
  }
  /// Native size of the computer's desktop (union of all of its displays).
  [[nodiscard]] QSize resolution() const
  {
    return m_Resolution;
  }
  void setResolution(const QSize &resolution);
  /// Uniform scale applied to the resolution when placed on the canvas.
  [[nodiscard]] double scale() const
  {
    return m_Scale;
  }
  void setScale(double scale);
  /// The area this computer occupies on the layout canvas.
  [[nodiscard]] QRectF rect() const
  {
    return QRectF(m_Position, QSizeF(m_Resolution) * m_Scale);
  }

  bool operator==(const Computer &computer) const;

protected:
  QStringList &aliases()
  {
    return m_Aliases;
  }
  void setModifier(const KeyboardModifier m, const int n)
  {
    m_Modifiers[static_cast<int8_t>(m)] = n;
  }
  QList<int> &modifiers()
  {
    return m_Modifiers;
  }
  void addAlias(const QString &alias)
  {
    m_Aliases.append(alias);
  }
  void setSwitchCorner(const SwitchCorner c, const bool on)
  {
    m_SwitchCorners[static_cast<int8_t>(c)] = on;
  }
  QList<bool> &switchCorners()
  {
    return m_SwitchCorners;
  }
  void setSwitchCornerSize(const int val)
  {
    m_SwitchCornerSize = val;
  }
  void setFix(const Fix f, const bool on)
  {
    m_Fixes[static_cast<int8_t>(f)] = on;
  }
  QList<bool> &fixes()
  {
    return m_Fixes;
  }
  void setSwapped(const bool on)
  {
    m_Swapped = on;
  }

private:
  inline static const auto kKeyX = QStringLiteral("x");
  inline static const auto kKeyY = QStringLiteral("y");
  inline static const auto kKeyWidth = QStringLiteral("width");
  inline static const auto kKeyHeight = QStringLiteral("height");
  inline static const auto kKeyScale = QStringLiteral("scale");

  QPixmap m_Pixmap = QIcon::fromTheme("video-display").pixmap(QSize(96, 96));
  QString m_Name = {};
  QStringList m_Aliases = {};
  QList<int> m_Modifiers = {0, 1, 2, 3, 4, 5, 6};
  QList<bool> m_SwitchCorners = {false, false, false, false};
  int m_SwitchCornerSize = 0;
  QList<bool> m_Fixes{false, false, false, false};
  bool m_Swapped = false;
  bool m_isServer = false;
  QPoint m_Position = {0, 0};
  QSize m_Resolution = kDefaultResolution;
  double m_Scale = 1.0;
};
