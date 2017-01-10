#include <pthread.h>
#include <stdio.h>

class Thread {
private:
    pthread_t _thread;
    bool _valid;
    bool _stopped;

    static void * start_routine(void * param)
    {
        Thread * thread = (Thread*)param;
        thread->run();
        thread->_stopped = true;

        return NULL;
    }
protected:
    virtual void run()
    {
    }
public: 
    virtual void start()
    {   
        _valid = (pthread_create(&_thread, NULL, &start_routine, this) == 0);

    }
    bool stopped()
    {
        return _stopped;
    }
    void join()
    {
        if ( _valid ) {
            void * res;
            pthread_join( _thread, NULL );
            _valid = false;
        }
    }
    void exit(void *ptr)
    {
        if ( _valid ) {
            void * res;
            pthread_exit(ptr);
            _valid = false;
        }
    }
    void cancel()
    {
        if ( _valid ) {
            void * res;
            pthread_cancel(_thread);
            _valid = false;
        }
    }
    Thread()
    : _valid(false), _stopped(false)
    {
    }
    virtual ~Thread()
    {
        if ( _valid ) {
            pthread_detach( _thread );
        }
    }
};