#include "dispatcher_enet.h"

#include "logger.h"
#include "config.h"
#include "messaging.h"
#include "sequencer.h"

#include <cassert>

void DispatcherENet::Initialize()
{
    // Make sure it's not started twice
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_thread_state != NOT_RUNNING)
    {
        return;
    }

    // Initialize ENet globally
    if (enet_initialize() != 0)
    {
        Logger::Log(LOG_ERROR, "Failed to initialize ENet library.");
        return;
    }

    // Fill address
    if (enet_address_set_host(&m_addr, Config::getIPAddr().c_str()) != 0)
    {
        Logger::Log(LOG_ERROR, "Failed to fill ENet host IP.");
        return;
    }

    // Fill port
    m_addr.port = static_cast<enet_uint16>(Config::getListenPort() + 1);

    // Create ENet host
    m_host = enet_host_create (&m_addr /* the address to bind the server host to */, 
                             32      /* allow up to 32 clients and/or outgoing connections */,
                              2      /* allow up to 2 channels to be used, 0 and 1 */,
                              0      /* assume any amount of incoming bandwidth */,
                              0      /* assume any amount of outgoing bandwidth */);
    if (m_host == nullptr)
    {
        Logger::Log(LOG_ERROR, "Failed to create ENet server host.");
        return;
    }
    else
    {
        m_thread = std::thread(&DispatcherENet::ThreadMain, this);
        m_thread_state = RUNNING;
    }
}

void DispatcherENet::Shutdown()
{
    // Make sure it's not shut down twice
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_thread_state != RUNNING)
    {
        return;
    }

    m_thread_state = PENDING_SHUTDOWN;
    m_thread.join();
}

void DispatcherENet::ThreadMain()
{
    Logger::Log(LOG_DEBUG, "ENet dispatch thread is running...");
    while (GetThreadState() == RUNNING)
    {
        ENetEvent ev;
        enet_uint32 timeout_milisec = 500;
        enet_host_service(m_host, &ev, timeout_milisec);
        switch (ev.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
                Logger::Log(LOG_DEBUG, "ENet: received event CONNECT");
                this->ThreadClientConnected(ev.peer);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                Logger::Log(LOG_DEBUG, "ENet: received event DISCONNECT");
                this->ThreadClientDisconnected(ev.peer);
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                this->ThreadPacketReceived(ev);
                break;

            default:;
        }
    }
}

void DispatcherENet::ThreadClientConnected(ENetPeer* peer)
{
    // Create a dummy Client to track RoRnet validation progress.
    // It will not be submitted to Sequencer until proven valid.
    Client* client = new Client(m_sequencer, peer, this);
    client->SetProgress(Client::AWAITING_HELLO);

    // Let ENet carry the object as "user data".
    peer->data = client;
}

void DispatcherENet::ThreadClientDisconnected(ENetPeer* peer)
{
    // Sanity checks

    if (peer->data == nullptr)
    {
        Logger::Log(LOG_ERROR, "(Internal error!) Received disconnect event for unregistered client!");
        enet_peer_reset(peer);
        return;
    }

    // Process the client

    Client* client = reinterpret_cast<Client*>(peer->data);
    if (client->GetProgress() == Client::DISCONNECTING)
    {
        Logger::Log(LOG_DEBUG, "ENet: This was clean disconnect (initiated by player or server).");
        // The Client object was already unregistered from Sequencer.
        delete client;
    }
    else
    {
        Logger::Log(LOG_DEBUG, "ENet: This was unexpected disconnect (possibly a connection error)");
        // Notify other clients and unregister from Sequencer.
        m_sequencer->disconnectClient(client->GetUserId(), "Connection error", /*isError:*/true);
        delete client;
    }
}

void DispatcherENet::ThreadPacketReceived(ENetEvent ev)
{    
    // Sanity checks

    if (ev.peer->data == nullptr)
    {
        Logger::Log(LOG_ERROR, "(Internal error!) Received packet from unregistered client! Disconnecting the peer.");
        enet_peer_disconnect(ev.peer, 0);
        return;
    }

    if (ev.packet->dataLength < sizeof(RoRnet::Header))
    {
        Logger::Log(LOG_ERROR, "Received invalid packet (too short)! Disconnecting the peer.");
        enet_peer_disconnect(ev.peer, 0);
        return;
    }

    // Process the packet

    Client* client = reinterpret_cast<Client*>(ev.peer->data);
    client->MessageReceived(ev.packet);
    Messaging::StatsAddIncoming(static_cast<int>(ev.packet->dataLength));
}

void DispatcherENet::SetThreadState(ThreadState s)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_thread_state = s;
}

DispatcherENet::ThreadState DispatcherENet::GetThreadState()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_thread_state;
}

void DispatcherENet::QueueMessage(ENetPeer *peer, int type, int source, unsigned int streamid, unsigned int len,
                const char *content) {
    assert(peer != nullptr);

    RoRnet::Header head;

    const int msgsize = sizeof(RoRnet::Header) + len;

    if (msgsize >= RORNET_MAX_MESSAGE_LENGTH) {
        Logger::Log(LOG_ERROR, "UID: %d - attempt to send too long message", source);
        return;
    }

    char buffer[RORNET_MAX_MESSAGE_LENGTH];

    memset(&head, 0, sizeof(RoRnet::Header));
    head.command = type;
    head.source = source;
    head.size = len;
    head.streamid = streamid;

    // construct buffer
    memset(buffer, 0, RORNET_MAX_MESSAGE_LENGTH);
    memcpy(buffer, (char *) &head, sizeof(RoRnet::Header));
    memcpy(buffer + sizeof(RoRnet::Header), content, len);

    // queue ENet packet
    enet_uint32 packet_flags = ENET_PACKET_FLAG_RELIABLE;
    ENetPacket* packet = enet_packet_create(buffer, msgsize, packet_flags);
    enet_peer_send(peer, 0, packet);
    Messaging::StatsAddOutgoing(msgsize);
}