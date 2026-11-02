/***************************************************************************
 * SPDX-FileCopyrightText: 2026 Plasma Login Manager Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 ***************************************************************************/

#include "RemoteDisplayFactory.h"

#include "DaemonApp.h"
#include "DisplayManager.h"
#include "Seat.h"
#include "SeatManager.h"

#include "remotedisplayfactoryadaptor.h"

#include <QDBusConnection>
#include <QDebug>
#include <QRegularExpression>

namespace
{
using namespace Qt::StringLiterals;

inline constexpr auto RemoteDisplayFactoryPath = "/org/kde/DisplayManager/RemoteDisplayFactory"_L1;

inline constexpr auto DispatcherPath = "/org/kde/krdp/Dispatcher"_L1;
inline constexpr auto DispatcherInterface = "org.kde.krdp.Dispatcher"_L1;
inline constexpr auto ClientConnectedSignal = "ClientConnected"_L1;
inline constexpr auto ClientDisconnectedSignal = "ClientDisconnected"_L1;

inline constexpr auto RemoteSeatPrefix = "seat_remote_"_L1;
inline constexpr auto SanitizePattern = "[^A-Za-z0-9_]"_L1;
inline constexpr auto SanitizeReplacement = "_"_L1;

// Arbitrary cap to prevent very long D-Bus object paths and log injection via the remoteId field
inline constexpr qsizetype MaxRemoteIdLen = 64;

inline constexpr auto ErrInvalidRemoteId = "remoteId must be non-empty and at most %1 characters"_L1;
inline constexpr auto ErrFailedToCreateSeat = "Failed to create remote seat"_L1;
}

namespace PLASMALOGIN
{
RemoteDisplayFactory::RemoteDisplayFactory(QObject *parent)
    : QObject(parent)
{
    // create adaptor
    new RemoteDisplayFactoryAdaptor(this);

    // Register object on the system bus under the KDE DisplayManager path.
    // The org.freedesktop.DisplayManager service is already owned by DisplayManager.
    QDBusConnection connection = QDBusConnection::systemBus();
    connection.registerObject(RemoteDisplayFactoryPath.toString(), this);

    // Watch seat removal to broadcast RemoteDisplayStopped.
    // StopRemoteDisplay() relies solely on this signal path to avoid double-emission.
    connect(daemonApp->seatManager(), &SeatManager::seatRemoved, this, [this](const QString &seatName) {
        if (seatName.startsWith(RemoteSeatPrefix)) {
            const QString remoteId = seatName.mid(RemoteSeatPrefix.size());
            emit RemoteDisplayStopped(remoteId);
        }
    });

    // Also listen to KRdp Dispatcher signals if krdp-dispatcher is active.
    // Pass an empty service name so the D-Bus match rule survives dispatcher
    // restarts and is independent of service start order.
    connection.connect(QString(),
                       DispatcherPath.toString(),
                       DispatcherInterface.toString(),
                       ClientConnectedSignal.toString(),
                       this,
                       SLOT(CreateRemoteDisplay(QString)));

    connection.connect(QString(),
                       DispatcherPath.toString(),
                       DispatcherInterface.toString(),
                       ClientDisconnectedSignal.toString(),
                       this,
                       SLOT(StopRemoteDisplay(QString)));
}

QString RemoteDisplayFactory::sanitizeRemoteId(const QString &id) const
{
    QString sanitized = id;
    sanitized.replace(QRegularExpression(SanitizePattern.toString()), SanitizeReplacement.toString());
    return sanitized;
}

QDBusObjectPath RemoteDisplayFactory::CreateRemoteDisplay(const QString &remoteId)
{
    if (remoteId.trimmed().isEmpty() || remoteId.size() > MaxRemoteIdLen) {
        sendErrorReply(QDBusError::InvalidArgs, QString(ErrInvalidRemoteId).arg(MaxRemoteIdLen));
        return QDBusObjectPath();
    }

    const QString sanitizedId = sanitizeRemoteId(remoteId);
    const QString seatName = RemoteSeatPrefix.toString() + sanitizedId;

    // Check if a remote seat with this ID already exists
    Seat *existingSeat = daemonApp->seatManager()->findRemoteSeat(remoteId);
    if (existingSeat) {
        const QString seatPath = daemonApp->displayManager()->seatPath(existingSeat->name());
        return QDBusObjectPath(seatPath);
    }

    // Log the sanitized form only — remoteId is untrusted input from the network
    qInfo() << "Creating remote display for remoteId:" << sanitizedId << "seat:" << seatName;
    Seat *seat = daemonApp->seatManager()->createRemoteSeat(seatName, remoteId);
    if (!seat) {
        sendErrorReply(QDBusError::Failed, ErrFailedToCreateSeat.toString());
        return QDBusObjectPath();
    }

    const QString seatPath = daemonApp->displayManager()->seatPath(seatName);
    const QDBusObjectPath objPath(seatPath);

    emit RemoteDisplayCreated(remoteId, objPath);
    return objPath;
}

bool RemoteDisplayFactory::StopRemoteDisplay(const QString &remoteId)
{
    if (remoteId.trimmed().isEmpty()) {
        return false;
    }

    Seat *seat = daemonApp->seatManager()->findRemoteSeat(remoteId);
    if (!seat) {
        return false;
    }

    qInfo() << "Stopping remote display for remoteId:" << sanitizeRemoteId(remoteId) << "seat:" << seat->name();
    // removeSeat() emits seatRemoved, which the constructor's lambda re-emits as RemoteDisplayStopped.
    // Do not emit here again to avoid double-emission.
    daemonApp->seatManager()->removeSeat(seat->name());
    return true;
}

}

#include "moc_RemoteDisplayFactory.cpp"
