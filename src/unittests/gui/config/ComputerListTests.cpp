/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ComputerListTests.h"

#include "gui/config/ComputerList.h"

namespace {

Computer makeComputer(const QString &name, QPoint position, QSize resolution = Computer::kDefaultResolution)
{
  Computer computer(name);
  computer.setPosition(position);
  computer.setResolution(resolution);
  return computer;
}

} // namespace

void ComputerListTests::appendToRight()
{
  ComputerList computers;
  QCOMPARE(computers.appendToRight(Computer("a")), qsizetype(0));
  QCOMPARE(computers.at(0).position(), QPoint(0, 0));

  computers.append(makeComputer("b", {0, 1080}, QSize(3000, 1000)));
  QCOMPARE(computers.appendToRight(Computer("c")), qsizetype(2));
  // right of the right most computer (b), top aligned with it
  QCOMPARE(computers.at(2).position(), QPoint(3000, 1080));
  QCOMPARE(computers.indexOfName("c"), qsizetype(2));
  QCOMPARE(computers.indexOfName("missing"), qsizetype(-1));
}

void ComputerListTests::touchingIsNotOverlapping()
{
  ComputerList computers;
  computers.append(makeComputer("a", {0, 0}));

  QVERIFY(!computers.overlapsAny(QRectF(1920, 0, 1920, 1080)));
  QVERIFY(!computers.overlapsAny(QRectF(1919.5, 0, 1920, 1080)));
  QVERIFY(computers.overlapsAny(QRectF(1900, 0, 1920, 1080)));
  QVERIFY(!computers.overlapsAny(QRectF(0, 0, 1920, 1080), 0));
}

void ComputerListTests::resolveOverlapPushesOut()
{
  ComputerList computers;
  computers.append(makeComputer("a", {0, 0}));
  computers.append(makeComputer("b", {1800, 100}));

  QVERIFY(computers.resolveOverlap(1));
  // the shortest way out is to the right of a
  QCOMPARE(computers.at(1).position(), QPoint(1920, 100));
  QVERIFY(!computers.overlapsAny(computers.at(1).rect(), 1));
}

void ComputerListTests::boundingRect()
{
  ComputerList computers;
  QVERIFY(computers.boundingRect().isNull());

  computers.append(makeComputer("a", {0, 0}));
  computers.append(makeComputer("b", {-1280, 1080}, QSize(1280, 1024)));
  QCOMPARE(computers.boundingRect(), QRectF(-1280, 0, 3200, 2104));
}

QTEST_MAIN(ComputerListTests)
