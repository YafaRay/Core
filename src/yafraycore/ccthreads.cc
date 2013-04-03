#include <yafraycore/ccthreads.h>
#include <iostream>
#include <stdexcept>

#ifdef __APPLE__
#include <AvailabilityMacros.h>
#endif

namespace yafthreads {

mutex_t::mutex_t() 
{
#if HAVE_PTHREAD
	int error=pthread_mutex_init(&m, NULL);
	switch(error)
	{
		case EINVAL: throw std::runtime_error("pthread_mutex_init error EINVAL"); break;
		case ENOMEM: throw std::runtime_error("pthread_mutex_init error ENOMEM"); break;
		case EAGAIN: throw std::runtime_error("pthread_mutex_init error EAGAIN"); break;
		default: break;
	}
#elif defined( WIN32_THREADS )
	winMutex = CreateMutex(0, FALSE, 0);
#endif
}

void mutex_t::lock() 
{
#if HAVE_PTHREAD
	if(pthread_mutex_lock(&m))
	{
		throw std::runtime_error("Error mutex lock");
	}
#elif defined( WIN32_THREADS )
	WaitForSingleObject(winMutex, INFINITE);
#endif
}

void mutex_t::unlock() 
{
#if HAVE_PTHREAD
	if(pthread_mutex_unlock(&m))
	{
		throw std::runtime_error("Error mutex lock");
	}
#elif defined( WIN32_THREADS )
	ReleaseMutex(winMutex);
#endif
}

mutex_t::~mutex_t() 
{
#if HAVE_PTHREAD
	pthread_mutex_destroy(&m);
#elif defined( WIN32_THREADS )
	CloseHandle(winMutex);
#endif
}

/* read-shared write-exclusive lock */
rwlock_t::rwlock_t() 
{
#if HAVE_PTHREAD
	int error=pthread_rwlock_init(&l, NULL);
	switch(error)
	{
		case EINVAL: throw std::runtime_error("pthread_rwlock_init error EINVAL"); break;
		case ENOMEM: throw std::runtime_error("pthread_rwlock_init error ENOMEM"); break;
		case EAGAIN: throw std::runtime_error("pthread_rwlock_init error EAGAIN"); break;
		default: break;
	}
#endif
}

void rwlock_t::readLock() 
{
#if HAVE_PTHREAD
	if(pthread_rwlock_rdlock(&l))
	{
		throw std::runtime_error("Error rwlock readLock");
	}
#endif
}

void rwlock_t::writeLock() 
{
#if HAVE_PTHREAD
	if(pthread_rwlock_wrlock(&l))
	{
		throw std::runtime_error("Error rwlock writeLock");
	}
#endif
}

void rwlock_t::unlock() 
{
#if HAVE_PTHREAD
	if(pthread_rwlock_unlock(&l))
	{
		throw std::runtime_error("Error rwlock unlock");
	}
#endif
}

rwlock_t::~rwlock_t() 
{
#if HAVE_PTHREAD
	pthread_rwlock_destroy(&l);
#endif
}


/* condition object */

conditionVar_t::conditionVar_t() 
{
#if HAVE_PTHREAD
	int error=pthread_mutex_init(&m, NULL);
	switch(error)
	{
		case EINVAL: throw std::runtime_error("pthread_mutex_init error EINVAL"); break;
		case ENOMEM: throw std::runtime_error("pthread_mutex_init error ENOMEM"); break;
		case EAGAIN: throw std::runtime_error("pthread_mutex_init error EAGAIN"); break;
		default: break;
	}
	error = pthread_cond_init (&c, NULL);
	if(error != 0)
	{
		throw std::runtime_error("pthread_cond_init error\n");
	}
#elif defined( WIN32_THREADS )
	condHandle = CreateEvent(0, FALSE, FALSE, "yafConditionSignal");
	winMutex = CreateMutex(0, FALSE, 0);
#endif
}

void conditionVar_t::lock() 
{
#if HAVE_PTHREAD
	if(pthread_mutex_lock(&m))
	{
		throw std::runtime_error("Error mutex lock");
	}
#elif defined( WIN32_THREADS )
	WaitForSingleObject(winMutex, INFINITE);
#endif
}

void conditionVar_t::unlock() 
{
#if HAVE_PTHREAD
	if(pthread_mutex_unlock(&m))
	{
		throw std::runtime_error("Error mutex lock");
	}
#elif defined( WIN32_THREADS )
	ReleaseMutex(winMutex);
#endif
}

void conditionVar_t::signal()
{
#if HAVE_PTHREAD
	if(pthread_cond_signal(&c))
	{
		throw std::runtime_error("Error condition signal");
	}	
#elif defined( WIN32_THREADS )
	SetEvent(condHandle);
#endif
}

void conditionVar_t::wait()
{
#if HAVE_PTHREAD
	if(pthread_cond_wait(&c, &m))
	{
		throw std::runtime_error("Error condition wait");
	}
#elif defined( WIN32_THREADS )
	unlock();
	SignalObjectAndWait(condHandle, winMutex, INFINITE, FALSE);
#endif
}

conditionVar_t::~conditionVar_t() 
{
#if HAVE_PTHREAD
	pthread_mutex_destroy(&m);
	pthread_cond_destroy(&c);
#elif defined( WIN32_THREADS )
	CloseHandle(condHandle);
	CloseHandle(winMutex);
#endif
}

#if HAVE_PTHREAD
void * wrapper(void *data)
{
	using namespace yafaray;
	thread_t *obj = (thread_t *)data;
	try{ obj->body(); }
	catch(std::exception &e)
	{
		Y_ERROR << "Threads: Exception occured: " << e.what() << yendl;
	}
	obj->running=false;
	pthread_exit(0);
	return NULL;
}

void thread_t::run()
{
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
	pthread_attr_setstacksize(&attr,1048576); //A bigger stack size helps to avoid crashes when you use many simultaneous threads.
	pthread_create(&id,&attr,wrapper,this);
	running=true;
}

void thread_t::wait()
{
	pthread_join(id,NULL);
	running=false;
}

thread_t::~thread_t()
{
	if(running) wait();
}
#elif defined( WIN32_THREADS )
DWORD WINAPI wrapper (void *data)
{
	thread_t *obj=(thread_t *)data;
	obj->body();
	return 0;
}

void thread_t::run()
{
	winThread = CreateThread(0, 0, wrapper, this, 0, &id);
	running = true;
}

void thread_t::wait()
{
	WaitForSingleObject(winThread, INFINITE);
	running = false;
}

thread_t::~thread_t()
{
	if(running) wait();
	CloseHandle(winThread);
}
#endif

} // yafthreads
