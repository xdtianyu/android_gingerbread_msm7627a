/**
 * @file
 * Router is responsible for taking inbound messages and routing them
 * to an appropriate set of endpoints.
 */

/******************************************************************************
 * Copyright 2009-2011, Qualcomm Innovation Center, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 ******************************************************************************/

#include <qcc/platform.h>

#include <assert.h>

#include <qcc/Debug.h>
#include <qcc/Logger.h>
#include <qcc/String.h>
#include <qcc/Util.h>

#include <Status.h>

#include "BusController.h"
#include "BusEndpoint.h"
#include "ConfigDB.h"
#include "DaemonRouter.h"

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;


namespace ajn {


class DeferredMsg : public ServiceStartListener {

  public:
    DeferredMsg(Message& msg, const qcc::String senderUniqueName, DaemonRouter& router) :
        msg(msg), senderUniqueName(senderUniqueName), router(router) { }

  private:
    void ServiceStarted(const qcc::String& serviceName, QStatus result);

    Message msg;
    const qcc::String senderUniqueName;
    DaemonRouter& router;
};

void DeferredMsg::ServiceStarted(const qcc::String& serviceName, QStatus result)
{

    if (result == ER_OK) {
        BusEndpoint* sender(router.FindEndpoint(senderUniqueName));
        // Need to pass it though the policy checks agains (since not all
        // policy checks were done).
        router.PushMessage(msg, *sender);
    } else {
        BusEndpoint* destEndpoint(router.FindEndpoint(msg->GetSender()));
        qcc::String description("Failed to start service for bus name: ");
        description += msg->GetDestination();
        msg->ErrorMsg("org.freedesktop.DBus.Error.ServiceUnknown", description.c_str());
        destEndpoint->PushMessage(msg);
    }


    delete this;
}


DaemonRouter::DaemonRouter() : localEndpoint(NULL), ruleTable(), nameTable(), busController(NULL)
{
    AddBusNameListener(ConfigDB::GetConfigDB());
}

static QStatus SendThroughEndpoint(Message& msg, BusEndpoint& ep, SessionId sessionId)
{
    QStatus status;
    if ((sessionId != 0) && (ep.GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL)) {
        status = static_cast<VirtualEndpoint&>(ep).PushMessage(msg, sessionId);
    } else {
        status = ep.PushMessage(msg);
    }
    if (status != ER_OK) {
        QCC_LogError(status, ("SendThroughEndpoint(dest=%s, ep=%s, id=%u) failed", msg->GetDestination(), ep.GetUniqueName().c_str(), sessionId));
    }
    return status;
}

QStatus DaemonRouter::PushMessage(Message& msg, BusEndpoint& origSender)
{
    QStatus status(ER_OK);
    ConfigDB* configDB(ConfigDB::GetConfigDB());
    PolicyDB policydb(configDB->GetPolicyDB());
    NormalizedMsgHdr nmh(msg, policydb);
    BusEndpoint* sender = &origSender;
    bool replyExpected = (msg->GetType() == MESSAGE_METHOD_CALL) && ((msg->GetFlags() & ALLJOYN_FLAG_NO_REPLY_EXPECTED) == 0);

    const char* destination = msg->GetDestination();
    SessionId sessionId = msg->GetSessionId();

    if (sender != localEndpoint) {
        ALLJOYN_POLICY_DEBUG(Log(LOG_DEBUG, "Checking if OK for %s to send %s.%s to %s...\n",
                                 msg->GetSender(),
                                 msg->GetInterface(),
                                 msg->GetMemberName() ? msg->GetMemberName() : msg->GetErrorName(),
                                 destination));
        bool allow = policydb->OKToSend(nmh, sender->GetUserId(), sender->GetGroupId());
        ALLJOYN_POLICY_DEBUG(Log(LOG_INFO, "%s %s (uid:%d gid:%d) %s %s.%s %s message to %s.\n",
                                 allow ? "Allowing" : "Denying",
                                 msg->GetSender(), sender->GetUserId(), sender->GetGroupId(),
                                 allow ? "to send" : "from sending",
                                 msg->GetInterface(), msg->GetMemberName(),
                                 (msg->GetType() == MESSAGE_SIGNAL ? "signal" :
                                  (msg->GetType() == MESSAGE_METHOD_CALL ? "method call" :
                                   (msg->GetType() == MESSAGE_METHOD_RET ? "method reply" : "error reply"))),
                                 (destination[0] == '\0' ? "<all>" : destination)));

        if (!allow) {
            // TODO - Should eavesdroppers be allowed to see a message that
            // was sender was denied delivery due to policy violations?
            return ER_BUS_POLICY_VIOLATION;
        }
    }

    bool destinationEmpty = destination[0] == '\0';
    if (!destinationEmpty) {
        BusEndpoint* destEndpoint = nameTable.FindEndpoint(destination);
        if (destEndpoint) {
            if (destEndpoint != localEndpoint) {
                ALLJOYN_POLICY_DEBUG(Log(LOG_DEBUG, "Checking OK for %s to receive %s.%s from %s\n",
                                         destEndpoint->GetUniqueName().c_str(),
                                         msg->GetInterface(),
                                         msg->GetMemberName() ? msg->GetMemberName() : msg->GetErrorName(),
                                         msg->GetSender()));

                bool allow = policydb->OKToReceive(nmh, destEndpoint->GetUserId(), destEndpoint->GetGroupId());

                ALLJOYN_POLICY_DEBUG(Log(LOG_INFO, "%s %s (uid:%d gid:%d) %s %s.%s %s message from %s.\n",
                                         allow ? "Allowing" : "Denying",
                                         destEndpoint->GetUniqueName().c_str(),
                                         destEndpoint->GetUserId(), destEndpoint->GetGroupId(),
                                         allow ? "to receive" : "from receiving",
                                         msg->GetInterface(), msg->GetMemberName(),
                                         (msg->GetType() == MESSAGE_SIGNAL ? "signal" :
                                          (msg->GetType() == MESSAGE_METHOD_CALL ? "method call" :
                                           (msg->GetType() == MESSAGE_METHOD_RET ? "method reply" : "error reply"))),
                                         msg->GetSender()));

                if (!allow) {
                    // TODO - Should eavesdroppers be allowed to see a message
                    // that was denied it's intended destination due to policy
                    // violations?
                    status = ER_BUS_POLICY_VIOLATION;
                }
            }

            if (ER_OK == status) {
                /* If this message is coming from a bus-to-bus ep, make sure the receiver is willing to receive it */
                if (!((sender->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_BUS2BUS) && !destEndpoint->AllowRemoteMessages())) {
                    /*
                     * If the sender doesn't allow remote messages reject method calls that go off
                     * device and require a reply because the reply will be blocked and this is most
                     * definitely not what the sender expects.
                     */
                    if ((destEndpoint->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL) && replyExpected && !sender->AllowRemoteMessages()) {
                        QCC_DbgPrintf(("Blocking method call from %s to %s (serial=%d) because caller does not allow remote messages",
                                       msg->GetSender(),
                                       destEndpoint->GetUniqueName().c_str(),
                                       msg->GetCallSerial()));
                        msg->ErrorMsg("org.alljoyn.Bus.Blocked", "Method reply would be blocked because caller does not allow remote messages");
                        PushMessage(msg, *localEndpoint);
                    } else {
                        status = SendThroughEndpoint(msg, *destEndpoint, sessionId);
                    }
                } else {
                    QCC_DbgPrintf(("Blocking message from %s to %s (serial=%d) because receiver does not allow remote messages",
                                   msg->GetSender(),
                                   destEndpoint->GetUniqueName().c_str(),
                                   msg->GetCallSerial()));
                    /* If caller is expecting a response return an error indicating the method call was blocked */
                    if (replyExpected) {
                        qcc::String description("Remote method calls blocked for bus name: ");
                        description += destination;
                        msg->ErrorMsg("org.alljoyn.Bus.Blocked", description.c_str());
                        PushMessage(msg, *localEndpoint);
                    }
                }
                if ((ER_OK != status) && (ER_BUS_ENDPOINT_CLOSING != status)) {
                    QCC_LogError(status, ("BusEndpoint::PushMessage failed"));
                }
            }
        } else {
            if ((msg->GetFlags() & ALLJOYN_FLAG_AUTO_START) && (sender->GetEndpointType() != BusEndpoint::ENDPOINT_TYPE_BUS2BUS)) {
                /* Need to auto start the service targeted by the message and postpone delivery of the message. */
                Bus& bus(reinterpret_cast<Bus&>(msg->bus));
                DeferredMsg* dm(new DeferredMsg(msg, sender->GetUniqueName(), *this));
                ServiceDB serviceDB(configDB->GetServiceDB());
                status = serviceDB->BusStartService(destination, dm, &bus);

            } else if (replyExpected) {
                QCC_LogError(ER_BUS_NO_ROUTE, ("Returning error %s no route to %s", msg->Description().c_str(), destination));
                /* Need to let the sender know its reply message cannot be passed on. */
                qcc::String description("Unknown bus name: ");
                description += destination;
                msg->ErrorMsg("org.freedesktop.DBus.Error.ServiceUnknown", description.c_str());
                PushMessage(msg, *localEndpoint);
            } else {
                QCC_LogError(ER_BUS_NO_ROUTE, ("Discarding %s no route to %s:%d", msg->Description().c_str(), destination, sessionId));
            }
        }
    }

    /* Forward broadcast to endpoints (local or remote) whose rules allow it */
    if ((destinationEmpty && (sessionId == 0)) || policydb->EavesdropEnabled()) {
        ruleTable.Lock();
        RuleIterator it = ruleTable.Begin();
        while (it != ruleTable.End()) {
            if (it->second.IsMatch(msg)) {
                BusEndpoint* dest = it->first;
                bool allow;
                QCC_DbgPrintf(("Routing %s (%d) to %s",
                               msg->Description().c_str(),
                               msg->GetCallSerial(),
                               dest->GetUniqueName().c_str()));
                if (dest == localEndpoint) {
                    allow = true;
                } else {
                    ALLJOYN_POLICY_DEBUG(Log(LOG_DEBUG, "Checking OK for %s to receive %s.%s from %s\n",
                                             dest->GetUniqueName().c_str(),
                                             msg->GetInterface(),
                                             msg->GetMemberName() ? msg->GetMemberName() : msg->GetErrorName(),
                                             msg->GetSender()));

                    allow = (policydb->OKToReceive(nmh, dest->GetUserId(), dest->GetGroupId()) ||
                             (policydb->EavesdropEnabled() &&
                              policydb->OKToEavesdrop(nmh,
                                                      sender->GetUserId(), sender->GetGroupId(),
                                                      dest->GetUserId(), dest->GetGroupId())));

                    ALLJOYN_POLICY_DEBUG(Log(LOG_INFO, "%s %s (uid:%d gid:%d) %s %s.%s %s message from %s.\n",
                                             allow ? "Allowing" : "Denying",
                                             dest->GetUniqueName().c_str(),
                                             dest->GetUserId(), dest->GetGroupId(),
                                             allow ? "to receive" : "from receiving",
                                             msg->GetInterface(), msg->GetMemberName(),
                                             (msg->GetType() == MESSAGE_SIGNAL ? "signal" :
                                              (msg->GetType() == MESSAGE_METHOD_CALL ? "method call" :
                                               (msg->GetType() == MESSAGE_METHOD_RET ? "method reply" : "error reply"))),
                                             msg->GetSender()));
                }
                if (allow) {
                    // Broadcast status must not trump directed message
                    // status, especially for eavesdropped messages.
                    if (policydb->EavesdropEnabled() || !((sender->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_BUS2BUS) && !dest->AllowRemoteMessages())) {
                        QStatus tStatus = SendThroughEndpoint(msg, *dest, sessionId);
                        status = (status == ER_OK) ? tStatus : status;
                    }
                }
                ruleTable.AdvanceToNextEndpoint(it);
            } else {
                ++it;
            }
        }
        ruleTable.Unlock();
    }

    /* Send global broadcast to all busToBus endpoints that aren't the sender of the message */
    if (destinationEmpty && (sessionId == 0) && msg->IsGlobalBroadcast()) {
        m_b2bEndpointsLock.Lock();
        vector<RemoteEndpoint*>::const_iterator it = m_b2bEndpoints.begin();
        while (it != m_b2bEndpoints.end()) {
            if ((*it) != &origSender) {
                QStatus tStatus = SendThroughEndpoint(msg, **it, sessionId);
                status = (status == ER_OK) ? tStatus : status;
            }
            ++it;
        }
        m_b2bEndpointsLock.Unlock();
    }

    /* Send session multicast messages */
    if (destinationEmpty && (sessionId != 0)) {
        sessionCastSetLock.Lock();
        RemoteEndpoint* lastB2b = NULL;
        SessionCastEntry sce(sessionId, msg->GetSender(), NULL, NULL);
        set<SessionCastEntry>::iterator sit = sessionCastSet.lower_bound(sce);
        while ((sit != sessionCastSet.end()) && (sit->id == sce.id) && (sit->src == sce.src)) {
            if (!sit->b2bEp || (sit->b2bEp != lastB2b)) {
                QStatus tStatus = SendThroughEndpoint(msg, *sit->destEp, sessionId);
                status = (status == ER_OK) ? tStatus : status;
                lastB2b = sit->b2bEp;
            }
            ++sit;
        }
        sessionCastSetLock.Unlock();
    }
    return status;
}

void DaemonRouter::GetBusNames(vector<qcc::String>& names) const
{
    nameTable.GetBusNames(names);
}


BusEndpoint* DaemonRouter::FindEndpoint(const qcc::String& busName)
{
    BusEndpoint* ep = nameTable.FindEndpoint(busName);
    if (!ep) {
        m_b2bEndpointsLock.Lock();
        for (vector<RemoteEndpoint*>::const_iterator it = m_b2bEndpoints.begin(); it != m_b2bEndpoints.end(); ++it) {
            if ((*it)->GetUniqueName() == busName) {
                ep = *it;
                break;
            }
        }
        m_b2bEndpointsLock.Unlock();
    }
    return ep;
}

QStatus DaemonRouter::RegisterEndpoint(BusEndpoint& endpoint, bool isLocal)
{
    QStatus status = ER_OK;

    /* Keep track of local endpoint */
    if (isLocal) {
        localEndpoint = static_cast<LocalEndpoint*>(&endpoint);
    }

    if (endpoint.GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_BUS2BUS) {
        /* AllJoynObj is in charge of managing bus-to-bus endpoints and their names */
        RemoteEndpoint* busToBusEndpoint = static_cast<RemoteEndpoint*>(&endpoint);
        status = busController->GetAllJoynObj().AddBusToBusEndpoint(*busToBusEndpoint);

        /* Add to list of bus-to-bus endpoints */
        m_b2bEndpointsLock.Lock();
        m_b2bEndpoints.push_back(busToBusEndpoint);
        m_b2bEndpointsLock.Unlock();
    } else {
        /* Bus-to-client endpoints appear directly on the bus */
        nameTable.AddUniqueName(endpoint);
    }

    /* Notify local endpoint that it is connected */
    if (&endpoint == localEndpoint) {
        localEndpoint->BusIsConnected();
    }

    return status;
}

void DaemonRouter::UnregisterEndpoint(BusEndpoint& endpoint)
{
    QCC_DbgTrace(("UnregisterEndpoint: %s (type=%d)", endpoint.GetUniqueName().c_str(), endpoint.GetEndpointType()));

    if (BusEndpoint::ENDPOINT_TYPE_BUS2BUS == endpoint.GetEndpointType()) {
        /* Inform bus controller of bus-to-bus endpoint removal */
        assert(busController);
        RemoteEndpoint* busToBusEndpoint = static_cast<RemoteEndpoint*>(&endpoint);
        busController->GetAllJoynObj().RemoveBusToBusEndpoint(*busToBusEndpoint);

        /* Remove the bus2bus endpoint from the list */
        m_b2bEndpointsLock.Lock();
        vector<RemoteEndpoint*>::iterator it = m_b2bEndpoints.begin();
        while (it != m_b2bEndpoints.end()) {
            if (*it == busToBusEndpoint) {
                m_b2bEndpoints.erase(it);
                break;
            }
            ++it;
        }
        m_b2bEndpointsLock.Unlock();
    } else {
        /* Remove any session routes */
        qcc::String uniqueName = endpoint.GetUniqueName();
        RemoveSessionRoutes(uniqueName.c_str(), 0);

        /* Remove endpoint from names and rules */
        nameTable.RemoveUniqueName(uniqueName);
        RemoveAllRules(endpoint);
    }

    /* Unregister static endpoints */
    if (&endpoint == localEndpoint) {
        localEndpoint = NULL;
    }
}

QStatus DaemonRouter::AddSessionRoute(const char* src, SessionId id, BusEndpoint& destEp, RemoteEndpoint*& b2bEp, SessionOpts* optsHint)
{
    QStatus status = ER_OK;
    if (id == 0) {
        status = ER_BUS_NO_SESSION;
    } else if (destEp.GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL) {
        if (b2bEp) {
            status = static_cast<VirtualEndpoint&>(destEp).AddSessionRef(id, *b2bEp);
        } else if (optsHint) {
            status = static_cast<VirtualEndpoint&>(destEp).AddSessionRef(id, optsHint, b2bEp);
        }
    }

    if (status == ER_OK) {
        String srcStr = src;
        sessionCastSetLock.Lock();
        sessionCastSet.insert(SessionCastEntry(id, srcStr, b2bEp, &destEp));
        sessionCastSetLock.Unlock();
    }
    return status;
}

QStatus DaemonRouter::RemoveSessionRoute(const char* src, SessionId id, BusEndpoint& destEp)
{
    QStatus status = ER_OK;
    RemoteEndpoint*b2bEp = NULL;
    if (id == 0) {
        status = ER_BUS_NO_SESSION;
    } else if (destEp.GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL) {
        VirtualEndpoint& vDestEp = static_cast<VirtualEndpoint&>(destEp);
        b2bEp = vDestEp.GetBusToBusEndpoint(id);
        vDestEp.RemoveSessionRef(id);
    }

    if (status == ER_OK) {
        String srcStr = src;
        sessionCastSetLock.Lock();
        set<SessionCastEntry>::iterator it = sessionCastSet.find(SessionCastEntry(id, srcStr, b2bEp, &destEp));
        if (it != sessionCastSet.end()) {
            sessionCastSet.erase(it);
        }
        sessionCastSetLock.Unlock();
    }
    return status;
}

void DaemonRouter::RemoveSessionRoutes(const char* src, SessionId id)
{
    BusEndpoint* ep = FindEndpoint(src);
    if (!ep) {
        QCC_LogError(ER_BUS_NO_ENDPOINT, ("Cannot find %s", src));
    }

    sessionCastSetLock.Lock();
    String srcStr = src;
    set<SessionCastEntry>::iterator it = sessionCastSet.begin();
    while (it != sessionCastSet.end()) {
        if (((it->id == id) || (id == 0)) && ((it->src == src) || (it->destEp == ep))) {
            if ((id != 0) && (it->destEp->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL)) {
                static_cast<VirtualEndpoint*>(it->destEp)->RemoveSessionRef(id);
            }
            sessionCastSet.erase(it++);
        } else {
            ++it;
        }
    }
    sessionCastSetLock.Unlock();
}

}
