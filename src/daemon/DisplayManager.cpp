/***************************************************************************
 * SPDX-FileCopyrightText: 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 ***************************************************************************/

#include "DisplayManager.h"

#include "DaemonApp.h"
#include "SeatManager.h"

#include "displaymanageradaptor.h"
#include "seatadaptor.h"
#include "sessionadaptor.h"

#include <QRegularExpression>

namespace
{
using namespace Qt::StringLiterals;

inline constexpr auto DisplayManagerService = "org.freedesktop.DisplayManager"_L1;
inline constexpr auto KdeDisplayManagerService = "org.kde.DisplayManager"_L1;
inline constexpr auto DisplayManagerPath = "/org/freedesktop/DisplayManager"_L1;
inline constexpr auto KdeDisplayManagerPath = "/org/kde/DisplayManager"_L1;
inline constexpr auto DisplayManagerSeatPath = "/org/freedesktop/DisplayManager/Seat"_L1;
inline constexpr auto DisplayManagerSessionPath = "/org/freedesktop/DisplayManager/Session"_L1;

inline constexpr auto StdSeatPattern = "^seat(\\d+)$"_L1;
inline constexpr qsizetype SessionPrefixLength = 7; // length of "Session"
}

namespace PLASMALOGIN
{
DisplayManager::DisplayManager(QObject *parent)
    : QObject(parent)
{
    // create adaptor
    new DisplayManagerAdaptor(this);

    // register object
    QDBusConnection connection = QDBusConnection::systemBus();
    connection.registerService(DisplayManagerService.toString());
    connection.registerService(KdeDisplayManagerService.toString());
    connection.registerObject(DisplayManagerPath.toString(), this);
    connection.registerObject(KdeDisplayManagerPath.toString(), this);
}

QString DisplayManager::seatPath(const QString &seatName)
{
    // Standard logind seats match seat\d+ (e.g. seat0, seat1).
    // Strip the "seat" prefix so they get paths like /Seat0, /Seat1.
    // All other seat names (e.g. seat_remote_<id>) get a leading underscore
    // so they become /Seat_seat_remote_<id> — unambiguous and collision-free.
    static const QRegularExpression stdSeatRe(StdSeatPattern.toString());
    const QRegularExpressionMatch m = stdSeatRe.match(seatName);
    if (m.hasMatch()) {
        return DisplayManagerSeatPath.toString() + m.captured(1);
    }
    return DisplayManagerSeatPath.toString() + u'_' + seatName;
}

QString DisplayManager::sessionPath(const QString &sessionName)
{
    return DisplayManagerSessionPath.toString() + sessionName.mid(SessionPrefixLength);
}

ObjectPathList DisplayManager::Seats() const
{
    ObjectPathList seats;

    for (DisplayManagerSeat *seat : m_seats) {
        seats << ObjectPath(seat->Path());
    }

    return seats;
}

ObjectPathList DisplayManager::Sessions(DisplayManagerSeat *seat) const
{
    ObjectPathList sessions;

    for (DisplayManagerSession *session : m_sessions) {
        if (seat == nullptr || seat->Name() == session->Seat()) {
            sessions << ObjectPath(session->Path());
        }
    }

    return sessions;
}

void DisplayManager::AddSeat(const QString &name)
{
    // create seat object
    DisplayManagerSeat *seat = new DisplayManagerSeat(name, this);

    // add to the list
    m_seats << seat;

    // emit signal
    emit SeatAdded(ObjectPath(seat->Path()));
}

void DisplayManager::RemoveSeat(const QString &name)
{
    // find seat
    for (DisplayManagerSeat *seat : m_seats) {
        if (seat->Name() == name) {
            // remove from list
            m_seats.removeAll(seat);

            // get object path
            ObjectPath path = ObjectPath(seat->Path());

            // delete seat
            seat->deleteLater();

            // emit signal
            emit SeatRemoved(path);
        }
    }
}

void DisplayManager::AddSession(const QString &name, const QString &seat, const QString &user)
{
    // create session object
    DisplayManagerSession *session = new DisplayManagerSession(name, seat, user, this);

    // add to the list
    m_sessions << session;

    // emit signal
    emit SessionAdded(ObjectPath(session->Path()));
}

void DisplayManager::RemoveSession(const QString &name)
{
    // find session
    for (DisplayManagerSession *session : m_sessions) {
        if (session->Name() == name) {
            // remove from list
            m_sessions.removeAll(session);

            // get object path
            ObjectPath path = ObjectPath(session->Path());

            // delete session
            session->deleteLater();

            // emit signal
            emit SessionRemoved(path);
        }
    }
}

DisplayManagerSeat::DisplayManagerSeat(const QString &name, QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_path(daemonApp->displayManager()->seatPath(name))
{
    // create adaptor
    new SeatAdaptor(this);

    // register object
    QDBusConnection connection = QDBusConnection::systemBus();
    connection.registerService(DisplayManagerService.toString());
    connection.registerObject(m_path, this);
}

const QString &DisplayManagerSeat::Name() const
{
    return m_name;
}

const QString &DisplayManagerSeat::Path() const
{
    return m_path;
}

void DisplayManagerSeat::SwitchToGreeter()
{
    daemonApp->seatManager()->switchToGreeter(m_name);
}

void DisplayManagerSeat::SwitchToGuest(const QString & /*session*/)
{
    // TODO: IMPLEMENT
}

void DisplayManagerSeat::SwitchToUser(const QString & /*user*/, const QString & /*session*/)
{
    // TODO: IMPLEMENT
}

void DisplayManagerSeat::Lock()
{
    // TODO: IMPLEMENT
}

ObjectPathList DisplayManagerSeat::Sessions()
{
    return daemonApp->displayManager()->Sessions(this);
}

DisplayManagerSession::DisplayManagerSession(const QString &name, const QString &seat, const QString &user, QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_path(DisplayManagerSessionPath.toString() + name.mid(SessionPrefixLength))
    , m_seat(seat)
    , m_user(user)
{
    // create adaptor
    new SessionAdaptor(this);

    // register object
    QDBusConnection connection = QDBusConnection::systemBus();
    connection.registerService(DisplayManagerService.toString());
    connection.registerObject(m_path, this);
}

const QString &DisplayManagerSession::Name() const
{
    return m_name;
}

const QString &DisplayManagerSession::Path() const
{
    return m_path;
}

const QString &DisplayManagerSession::Seat() const
{
    return m_seat;
}

void DisplayManagerSession::Lock()
{
    // TODO: IMPLEMENT
}

ObjectPath DisplayManagerSession::SeatPath() const
{
    return ObjectPath(daemonApp->displayManager()->seatPath(m_seat));
}

const QString &DisplayManagerSession::User() const
{
    return m_user;
}
}

#include "moc_DisplayManager.cpp"
