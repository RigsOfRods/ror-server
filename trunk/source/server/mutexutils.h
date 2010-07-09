#ifndef MUTEXUTILS_H_
#define MUTEXUTILS_H_

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


#endif /*MUTEXUTILS_HPP_*/
