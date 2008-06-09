#include "mutexutils.h"
#include "logger.h"
#include <signal.h>

Condition::Condition() { pthread_cond_init(&cond, NULL); }
Condition::~Condition() { pthread_cond_destroy(&cond); }
void Condition::signal() { pthread_cond_signal(&cond); }

Mutex::Mutex() : lock_owner( 0 ) { pthread_mutex_init(&m, NULL); }
Mutex::~Mutex() { pthread_mutex_destroy(&m); }
void Mutex::lock() {
	// check for deadlock, if this occurs raise an abort signal to stop the 
	// debugger
	if( ThreadID::getID() == lock_owner )
		raise( SIGABRT );
	pthread_mutex_lock(&m);
	lock_owner = ThreadID::getID(); 
} 
void Mutex::unlock() {
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
