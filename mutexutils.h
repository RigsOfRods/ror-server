#ifndef MUTEXUTILS_H_
#define MUTEXUTILS_H_

#include <pthread.h>
#include "messaging.h"

class Condition
{
	friend class Mutex;
public:
	Condition()
	{
		pthread_cond_init(&cond, NULL);
	}
	
	~Condition()
	{
		pthread_cond_destroy(&cond);
	}

	void signal()
	{
		pthread_cond_signal(&cond);
	}
private:
	pthread_cond_t cond;
};

class Mutex
{
public:
	Mutex()
	{
		pthread_mutex_init(&m, NULL);
		// this causes intercyclic calls, do NOT use it:
		//logmsgf(LOG_VVERBOSE,"Mutex: [%x] was created", &m);
	}
	~Mutex()
	{
		pthread_mutex_destroy(&m);
		logmsgf(LOG_VVERBOSE,"Mutex: [%x] was destroyed", &m);
		
	}
 
    void lock()
	{
    	pthread_mutex_lock(&m); 
		// removed due to perfermance issues
		//logmsgf(LOG_VVERBOSE,"Mutex: [%x] was locked", &m);
	}
 
    void unlock()
	{
    	pthread_mutex_unlock(&m);
		// removed due to perfermance issues
    	//logmsgf(LOG_VVERBOSE,"Mutex: [%x] was unlocked", &m);
    }
 
    void wait(Condition &c)
	{
    	logmsgf(LOG_VVERBOSE, "Mutex: [%x] is waiting for condition: [%x]", &m, &c);
    	pthread_cond_wait(&(c.cond), &m);
    	logmsgf(LOG_VVERBOSE, "Condition: [%x] has been met, Mutex: [%x] is free", &c, &m);
    }
private:
    pthread_mutex_t m;
};


class MutexLocker
{
public:
	MutexLocker(Mutex& pm): m(pm)
	{
		m.lock();
	}
	
	~MutexLocker() 
	{
		m.unlock();
	}
private:
	Mutex& m;
};

#endif /*MUTEXUTILS_HPP_*/