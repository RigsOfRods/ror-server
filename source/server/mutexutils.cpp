/*
This file is part of "Rigs of Rods Server" (Relay mode)

Copyright 2007   Pierre-Michel Ricordel
Copyright 2014+  Rigs of Rods Community

"Rigs of Rods Server" is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

"Rigs of Rods Server" is distributed in the hope that it will
be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar. If not, see <http://www.gnu.org/licenses/>.
*/

#include "mutexutils.h"

#include "logger.h"

#include <signal.h>
#include <assert.h>

Condition::Condition() { pthread_cond_init(&cond, NULL); }
Condition::~Condition() { pthread_cond_destroy(&cond); }
void Condition::signal() { pthread_cond_signal(&cond); }

Mutex::Mutex() : lock_owner( 0 ) { pthread_mutex_init(&m, NULL); }
Mutex::~Mutex() { pthread_mutex_destroy(&m); }
void Mutex::lock()
{
    // check for deadlock, if this occurs raise an abort signal to stop the
    // debugger

    if( ThreadID::getID() == lock_owner )
        raise( SIGABRT );
    pthread_mutex_lock(&m);
    lock_owner = ThreadID::getID(); 
} 
void Mutex::unlock()
{
    pthread_mutex_unlock(&m);
    lock_owner = 0;
} 
void Mutex::wait(Condition &c) {  pthread_cond_wait(&(c.cond), &m);	}

MutexLocker::MutexLocker(Mutex& pm): m(pm) { m.lock(); }
MutexLocker::~MutexLocker() { m.unlock(); }


pthread_key_t ThreadID::key;
pthread_once_t ThreadID::key_once = PTHREAD_ONCE_INIT;
unsigned int ThreadID::tuid = 1;

unsigned int ThreadID::getID()
{
    ThreadID *ptr = NULL;
    pthread_once(&key_once, ThreadID::make_key);
    ptr = (ThreadID*)pthread_getspecific(key);
    
    if( !ptr ) {
        ptr = new ThreadID();
        pthread_setspecific(key, (void*)ptr);
    }
    
    return ptr->thread_id;
}

ThreadID::ThreadID() : thread_id( tuid ) { tuid++; }
void ThreadID::make_key() { pthread_key_create(&key, NULL); }

namespace Threading {

bool SimpleCond::Initialize()
{
    assert(m_value == INACTIVE);

    int result = pthread_mutex_init(&m_mutex, nullptr);
    if (result != 0)
    {
        Logger::Log(LOG_ERROR, "Internal error: Failed to initialize mutex, error code: %d [in SimpleCond::Initialize()]", result);
        return false;
    }
    int cond_result = pthread_cond_init(&m_cond, nullptr);
    if (cond_result != 0)
    {
        Logger::Log(LOG_ERROR, "Internal error: Failed to initialize condition variable, error code: %d [in SimpleCond::Initialize()]", cond_result);
        return false;
    }
    m_value = 0;
    return true;
}

bool SimpleCond::Destroy()
{
    assert(m_value != INACTIVE);

    int result = pthread_mutex_destroy(&m_mutex);
    if (result != 0)
    {
        Logger::Log(LOG_WARN, "Internal: Failed to destroy mutex, error code: %d [in SimpleCond::Destroy()]", result);
        return false;
    }
    int cond_result = pthread_cond_destroy(&m_cond);
    if (cond_result != 0)
    {
        Logger::Log(LOG_WARN, "Internal: Failed to destroy condition variable, error code: %d [in SimpleCond::Destroy()]", cond_result);
        return false;
    }
    m_value = INACTIVE;
    return true;
}

bool SimpleCond::Wait(int* out_value)
{
    assert(out_value != nullptr);

    if (!this->Lock("SimpleCond::Wait()"))
    {
        return false;
    }

    while (m_value == 0)
    {
        int wait_result = pthread_cond_wait(&m_cond, &m_mutex);
        if (wait_result != 0)
        {
            Logger::Log(LOG_ERROR, "Internal error: Failed to wait on condition variable, error code: %d [in SimpleCond::Wait()]", wait_result);
            pthread_mutex_unlock(&m_mutex);
            return false;
        }
    }
    *out_value = m_value;
    
    if (!this->Unlock("SimpleCond::Wait()"))
    {
        return false;
    }
    return true;
}

bool SimpleCond::Signal(int value)
{
    assert(m_value != INACTIVE);

    if (!this->Lock("SimpleCond::Signal()"))
    {
        return false;
    }

    m_value = value;
    int signal_result = pthread_cond_signal(&m_cond);
    if (signal_result != 0)
    {
        Logger::Log(LOG_ERROR, "Internal error: Failed to signal condition variable, error code: %d [in SimpleCond::Signal()]", signal_result);
        pthread_mutex_unlock(&m_mutex);
        return false;
    }

    if (!this->Unlock("SimpleCond::Signal()"))
    {
        return false;
    }
    return true;
}

bool SimpleCond::Lock(const char* log_location)
{
    int lock_result = pthread_mutex_lock(&m_mutex);
    if (lock_result != 0)
    {
        Logger::Log(LOG_ERROR, "Internal: Failed to acquire lock, error code: %d [in %s]", lock_result, log_location);
        return false;
    }
    return true;
}

bool SimpleCond::Unlock(const char* log_location)
{
    int unlock_result = pthread_mutex_unlock(&m_mutex);
    if (unlock_result != 0)
    {
        Logger::Log(LOG_ERROR, "Internal: Failed to remove lock, error code: %d [in %s]", unlock_result, log_location);
        return false;
    }
    return true;
}

}

