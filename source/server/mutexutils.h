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

#pragma once

#include <pthread.h>

#define USE_THREADID

class Condition
{
    friend class Mutex;
public:
    Condition();
    ~Condition();
    void signal();
private:
    pthread_cond_t cond;
};


class Mutex
{
public:
    Mutex();
    ~Mutex();
    void lock();
    void unlock();
    void wait(Condition &c);

    pthread_mutex_t *getRaw() { return &m; };
    
private:
    pthread_mutex_t m;
    unsigned int lock_owner;
};


class MutexLocker
{
public:
    MutexLocker(Mutex& pm);
    ~MutexLocker() ;
private:
    Mutex& m;
};

class ThreadID
{
    
public:
    static unsigned int getID();
    
private:
    ThreadID();
    static void make_key();
    
    unsigned int thread_id;
    static pthread_key_t key;
    static pthread_once_t key_once;
    static unsigned int tuid;
};
