/***************************************************************************
 * SPDX-FileCopyrightText: 2026 Plasma Login Manager Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 ***************************************************************************/

#ifndef PLASMALOGIN_REMOTEDISPLAYFACTORY_H
#define PLASMALOGIN_REMOTEDISPLAYFACTORY_H

#include <QDBusContext>
#include <QDBusObjectPath>
#include <QObject>

namespace PLASMALOGIN
{
class RemoteDisplayFactory : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_DISABLE_COPY(RemoteDisplayFactory)
public:
    explicit RemoteDisplayFactory(QObject *parent = nullptr);

public slots:
    QDBusObjectPath CreateRemoteDisplay(const QString &remoteId);
    bool StopRemoteDisplay(const QString &remoteId);

signals:
    void RemoteDisplayCreated(const QString &remoteId, const QDBusObjectPath &display);
    void RemoteDisplayStopped(const QString &remoteId);

private:
    QString sanitizeRemoteId(const QString &id) const;
};
}

#endif // PLASMALOGIN_REMOTEDISPLAYFACTORY_H
