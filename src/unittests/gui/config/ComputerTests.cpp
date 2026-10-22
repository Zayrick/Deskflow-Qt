/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ComputerTests.h"

#include "common/Settings.h"
#include "gui/config/Computer.h"

#include <QDataStream>

void ComputerTests::initTestCase()
{
  QDir dir;
  QVERIFY(dir.mkpath(m_settingsPath));

  QFile oldSettings(m_settingsFile);
  if (oldSettings.exists())
    oldSettings.remove();

  Settings::setSettingsFile(m_settingsFile);
  Settings::setStateFile(m_stateFile);
}

void ComputerTests::basicFunctionality()
{
  Computer computer;
  QVERIFY(computer.isNull());

  computer.setName("stub");
  QVERIFY(!computer.isNull());

  computer.saveSettings(Settings::proxy());
  computer.loadSettings(Settings::proxy());
  QCOMPARE("stub", computer.name());
}

void ComputerTests::geometryDefaults()
{
  const Computer computer("stub");
  QCOMPARE(computer.position(), QPoint(0, 0));
  QCOMPARE(computer.resolution(), Computer::kDefaultResolution);
  QCOMPARE(computer.scale(), 1.0);
  QCOMPARE(computer.rect(), QRectF(0, 0, 1920, 1080));
}

void ComputerTests::geometrySettings()
{
  auto &settings = Settings::proxy();
  settings.beginGroup(QStringLiteral("geometryTest"));

  Computer computer("geometry");
  computer.setPosition(QPoint(100, -50));
  computer.setResolution(QSize(2560, 1440));
  computer.setScale(0.5);
  computer.saveSettings(settings);
  QVERIFY(Computer::hasGeometry(settings));

  Computer loaded;
  loaded.loadSettings(settings);
  settings.endGroup();

  QCOMPARE(loaded.name(), QStringLiteral("geometry"));
  QCOMPARE(loaded.position(), QPoint(100, -50));
  QCOMPARE(loaded.resolution(), QSize(2560, 1440));
  QCOMPARE(loaded.scale(), 0.5);
  QCOMPARE(loaded.rect(), QRectF(100, -50, 1280, 720));
}

void ComputerTests::legacySettingsHaveNoGeometry()
{
  auto &settings = Settings::proxy();
  settings.beginGroup(QStringLiteral("legacyTest"));
  settings.setValue(QStringLiteral("name"), QStringLiteral("legacy"));
  QVERIFY(!Computer::hasGeometry(settings));

  Computer loaded;
  loaded.loadSettings(settings);
  settings.endGroup();

  QCOMPARE(loaded.name(), QStringLiteral("legacy"));
  QCOMPARE(loaded.position(), QPoint(0, 0));
  QCOMPARE(loaded.resolution(), Computer::kDefaultResolution);
  QCOMPARE(loaded.scale(), 1.0);
}

void ComputerTests::scaleIsClamped()
{
  Computer computer("stub");

  computer.setScale(0.0);
  QCOMPARE(computer.scale(), 1.0);

  computer.setScale(-2.0);
  QCOMPARE(computer.scale(), 1.0);

  computer.setScale(0.001);
  QCOMPARE(computer.scale(), Computer::kMinScale);

  computer.setScale(1000.0);
  QCOMPARE(computer.scale(), Computer::kMaxScale);

  computer.setScale(0.75);
  QCOMPARE(computer.scale(), 0.75);
}

void ComputerTests::invalidResolutionFallsBack()
{
  Computer computer("stub");

  computer.setResolution(QSize(1280, 1024));
  QCOMPARE(computer.resolution(), QSize(1280, 1024));

  computer.setResolution(QSize());
  QCOMPARE(computer.resolution(), Computer::kDefaultResolution);

  computer.setResolution(QSize(0, 1080));
  QCOMPARE(computer.resolution(), Computer::kDefaultResolution);
}

void ComputerTests::equalityIncludesGeometry()
{
  Computer a("stub");
  Computer b("stub");
  QVERIFY(a == b);

  b.setPosition(QPoint(1, 0));
  QVERIFY(!(a == b));

  b = a;
  b.setResolution(QSize(800, 600));
  QVERIFY(!(a == b));

  b = a;
  b.setScale(2.0);
  QVERIFY(!(a == b));
}

void ComputerTests::dataStreamKeepsGeometry()
{
  Computer computer("stream");
  computer.setPosition(QPoint(-1920, 300));
  computer.setResolution(QSize(3840, 2160));
  computer.setScale(0.25);

  QByteArray data;
  {
    QDataStream out(&data, QIODevice::WriteOnly);
    out << computer;
  }

  Computer read;
  QDataStream in(&data, QIODevice::ReadOnly);
  in >> read;

  QCOMPARE(in.status(), QDataStream::Ok);
  QVERIFY(read == computer);
  QCOMPARE(read.rect(), QRectF(-1920, 300, 960, 540));
}

QTEST_MAIN(ComputerTests)
