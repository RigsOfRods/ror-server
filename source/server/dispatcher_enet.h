#pragma once

#include "prerequisites.h"

#include <enet/enet.h>
#include <mutex>
#include <thread>

class DispatcherENet
{
private:
    enum ThreadState
    {
        NOT_RUNNING,
        RUNNING,
        PENDING_SHUTDOWN
    };
    
    ENetHost*   m_host = nullptr;
    ENetAddress m_addr = {};
    ThreadState m_thread_state = ThreadState::NOT_RUNNING;
    std::mutex  m_mutex;
    std::thread m_thread;
    Sequencer*  m_sequencer = nullptr;

    void  ThreadMain();
    void  ThreadClientConnected(ENetPeer* peer);
    void  ThreadClientDisconnected(ENetPeer* peer);
    void  ThreadPacketReceived(ENetEvent ev);

    void  SetThreadState(ThreadState s);
    ThreadState GetThreadState();

public:
    DispatcherENet(Sequencer* sequencer): m_sequencer(sequencer) {}
    void Initialize();
    void Shutdown();
    void QueueMessage(ENetPeer *peer, int type, int source, unsigned int streamid, unsigned int len,
                const char *content);
};
