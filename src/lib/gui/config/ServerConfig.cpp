/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfig.h"

#include "common/Hotkey.h"
#include "common/Settings.h"

#include "ComputerLinks.h"

#include <QAbstractButton>
#include <QPushButton>

#include <algorithm>

using enum ComputerConfig::SwitchCorner;
using enum ComputerConfig::Fix;

ServerConfig::ServerConfig()
{
  recall();
}

bool ServerConfig::save(const QString &fileName) const
{
  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return false;

  save(file);
  file.close();

  return true;
}

bool ServerConfig::operator==(const ServerConfig &sc) const
{
  return m_computers == sc.m_computers && //
         m_Hotkeys == sc.m_Hotkeys;       //
}

void ServerConfig::save(QFile &file) const
{
  QTextStream outStream(&file);
  outStream << *this;
}

void ServerConfig::commit()
{
  qDebug("committing server config");

  settings().beginGroup("internalConfig");
  settings().remove("");

  settings().beginWriteArray("screens");
  int index = 0;
  for (const auto &computer : std::as_const(computers())) {
    if (computer.isNull())
      continue;
    settings().setArrayIndex(index++);
    computer.saveSettings(settings());
    auto computerName = Settings::value(Settings::Core::ComputerName).toString();
    if (computer.isServer() && computerName != computer.name()) {
      Settings::setValue(Settings::Core::ComputerName, computer.name());
    }
  }
  settings().endArray();

  settings().beginWriteArray("hotkeys");
  for (int i = 0; i < hotkeys().size(); i++) {
    settings().setArrayIndex(i);
    hotkeys()[i].saveSettings(settings().get());
  }
  settings().endArray();

  settings().endGroup();
}

void ServerConfig::recall()
{
  qDebug("recalling server config");

  settings().beginGroup("internalConfig");

  computers().clear();
  hotkeys().clear();

  // Configs written before the free layout stored a fixed grid of cells, empty cells included. Their position in the
  // array is turned into canvas coordinates so the old neighbours stay neighbours.
  const int gridWidth = std::max(1, Settings::value(Settings::Server::GridWidth).toInt());
  const auto cellSize = Computer::kDefaultResolution;

  int numComputers = settings().beginReadArray("screens");
  for (int i = 0; i < numComputers; i++) {
    settings().setArrayIndex(i);
    Computer computer;
    computer.loadSettings(settings());
    if (computer.isNull())
      continue;
    if (!Computer::hasGeometry(settings())) {
      computer.setPosition(QPoint((i % gridWidth) * cellSize.width(), (i / gridWidth) * cellSize.height()));
    }
    if (getServerName() == computer.name()) {
      computer.markAsServer();
    }
    computers().append(computer);
  }
  settings().endArray();

  int numHotkeys = settings().beginReadArray("hotkeys");
  for (int i = 0; i < numHotkeys; i++) {
    settings().setArrayIndex(i);
    Hotkey h;
    h.loadSettings(settings().get());
    hotkeys().append(h);
  }
  settings().endArray();

  settings().endGroup();
}

QTextStream &operator<<(QTextStream &outStream, const ServerConfig &config)
{
  outStream << "section: links" << Qt::endl;

  const auto links = deskflow::gui::computeLinks(config.computers());
  for (const auto &computer : config.computers()) {
    if (computer.isNull())
      continue;
    outStream << "\t" << computer.name() << ":\n";
    for (const auto &link : links) {
      if (link.source == computer.name())
        outStream << "\t\t" << link.toString() << Qt::endl;
    }
  }

  outStream << "end" << Qt::endl << Qt::endl;

  outStream << "section: options" << Qt::endl;

  for (const Hotkey &hotkey : config.hotkeys())
    outStream << hotkey;

  outStream << "end" << Qt::endl << Qt::endl;

  return outStream;
}

int ServerConfig::numComputers() const
{
  int rval = 0;

  for (const Computer &s : computers()) {
    if (!s.isNull())
      rval++;
  }

  return rval;
}

QString ServerConfig::getServerName() const
{
  return Settings::value(Settings::Core::ComputerName).toString();
}

void ServerConfig::updateServerName()
{
  for (auto &computer : computers()) {
    if (computer.isServer()) {
      computer.setName(Settings::value(Settings::Core::ComputerName).toString());
      break;
    }
  }
}

QString ServerConfig::configFile() const
{
  return Settings::value(Settings::Server::ExternalConfigFile).toString();
}

bool ServerConfig::useExternalConfig() const
{
  return Settings::value(Settings::Server::ExternalConfig).toBool();
}

bool ServerConfig::computerExists(const QString &computerName) const
{
  bool isExists = false;

  for (const auto &computer : computers()) {
    if (!computer.isNull() && computer.name() == computerName) {
      isExists = true;
      break;
    }
  }

  return isExists;
}

void ServerConfig::addClient(const QString &clientName)
{
  ensureServer();
  if (!computerExists(clientName))
    m_computers.appendToRight(Computer(clientName));
}

void ServerConfig::ensureServer(const QSize &desktopSize)
{
  const auto serverName = getServerName();
  auto index = m_computers.indexOfName(serverName);
  if (index == -1) {
    Computer server(serverName);
    if (desktopSize.isValid())
      server.setResolution(desktopSize);
    index = m_computers.appendToRight(server);
  } else if (desktopSize.isValid() && m_computers[index].resolution() != desktopSize) {
    // Keep the width the server occupies on the canvas so its left and right neighbours stay attached.
    auto &server = m_computers[index];
    const auto canvasWidth = server.rect().width();
    server.setResolution(desktopSize);
    server.setScale(canvasWidth / desktopSize.width());
  }
  m_computers[index].markAsServer();
}

void ServerConfig::setConfigFile(const QString &configFile) const
{
  Settings::setValue(Settings::Server::ExternalConfigFile, configFile);
}

void ServerConfig::setUseExternalConfig(bool useExternalConfig) const
{
  Settings::setValue(Settings::Server::ExternalConfig, useExternalConfig);
}

QSettingsProxy &ServerConfig::settings()
{
  return Settings::proxy();
}
