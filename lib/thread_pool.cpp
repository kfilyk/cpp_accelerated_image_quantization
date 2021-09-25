#include "../include/ra/thread_pool.hpp"

#include <condition_variable>
#include <utility>

using Thread = std::thread;
using Mutex = std::mutex;
using Lock = std::unique_lock<std::mutex>;
using FQ = ra::concurrency::queue<std::function<void()>>;  // queue of functions
using size_type = std::size_t;
using IQ = ra::concurrency::queue<size_type>;  // queue of indices
using CV = std::condition_variable;
namespace ra::concurrency {

// An unsigned integral type used to represent sizes.

// Creates a thread pool with the number of threads equal to the
// hardware concurrency level (if known); otherwise the number of
// threads is set to 2.
thread_pool::thread_pool() {
    //std::cout << "THREAD POOL INIT...\n";
    unsigned int n = std::thread::hardware_concurrency();
    if (n == 0) {
        n = 2;
    }
    size_ = n;
    threads_ = new Thread[n];
    mutexes_ = new Mutex[n];
    cvs_ = new CV[n];
    idle_ = new IQ(n);
    /*
    for (size_type i = 0; i < n; i++) {
        idle_->push(std::move(i));
    }*/
    start_threads(n);
}

// Creates a thread pool with num_threads threads.
// Precondition: num_threads > 0
thread_pool::thread_pool(size_type num_threads) {
    //std::cout << "THREAD POOL INIT...\n";
    size_ = num_threads;
    threads_ = new Thread[num_threads];
    mutexes_ = new Mutex[num_threads];
    cvs_ = new CV[num_threads];
    idle_ = new IQ(num_threads);
    /*
    for (size_type i = 0; i < num_threads; i++) {
        idle_->push(std::move(i));
    }*/
    start_threads(num_threads);
}

// Destroys a thread pool, shutting down the thread pool first
// (if not already shutdown).

thread_pool::~thread_pool() {
    //std::cout << "DESTRUCT...\n";

    shutdown();  // call shutdown - ensures all tasks completed before

    terminate_ = 0;  // cv signal for threads to self terminate
    for (size_type i = 0; i < size_; i++) {
        size_type t;
        idle_->pop(t);  // run all n threads for last time
        cvs_[t].notify_one();
        threads_[t].join();  // get rid of threads
    }
    //std::cout << "TERMINATE DONE...\n";

    delete[] threads_;  // delete thread container
    delete[] mutexes_;
    delete[] cvs_;
    delete idle_;
    // queues gets deleted on their own- call their own destructors .~FQ();
}


size_type thread_pool::size() const { return size_; }

void thread_pool::schedule(std::function<void()> &&func) {
    tpm_.lock();
    if (state_ == 0) { // non-shutdown state
        tpm_.unlock();
        tasks_.push(std::move(func)); 
        size_type i;
        idle_->pop(i); // thread safe - get index of idle thread
        Lock lk(mutexes_[i]); // guaranteed only locks when thread is waiting
        cvs_[i].notify_one(); // this should be fine to notify waiting thread now
    } else {
        tpm_.unlock();
    }
    //std::cout << "TP SCHEDULE COMPLETED...\n";
}

void thread_pool::shutdown() {
    //std::cout << "SHUTDOWN: STARTING...\n";
    tpm_.lock();
    if (state_ == 0) { // only if shutdown has not been called before
        state_ = 1;  
        tpm_.unlock();
        // should only be called once
        //std::cout << "FLAG1...\n";
        tpm_.lock();
        if (!idle_->is_full() || !tasks_.is_empty()) {  // wait for all threads to become idle
            tpm_.unlock();
            // std::cout << "BUSY IS NOT FULL\n";
            Lock lk(shutdown_m_);
            tpcv_.wait(lk, [this] {
                // std::cout << "LAMBDA CALLED\n";
                // std::cout << "BUSY: " << idle_->is_full() << "\n";
                return (idle_->is_full() && tasks_.is_empty());
            });
            // std::cout << "OK BUSY IS FULL DONE...\n";
        } else {
            tpm_.unlock();
        }
        //std::cout << "SHUTDOWN: ALL THREADS IDLE & NO MORE TASKS!...\n";
    } else {
        tpm_.unlock();
    }

    state_ = 2;

}

// returns and unblocks only when the thread pool has no busy threads (all idle).
void thread_pool::block_until_idle() { 
    Lock lk(tpm_);
    tpcv_.wait(lk, [this] {
        // now locked
        return (idle_->is_full() && tasks_.is_empty());
    });
}


// Tests if the thread pool has been shutdown.
// This function is not thread safe.
bool thread_pool::is_shutdown() const { return state_; }

void thread_pool::start_threads(size_type n) {

    for (size_type i = 0; i < n; i++) {

        threads_[i] = Thread(
            [this](size_type i) {
                Lock lk(mutexes_[i]); 
                while (true) {
                    idle_->push(std::move(i));
                    tpm_.lock();

                    // always notify tpcv 
                    tpcv_.notify_one();

                    tpm_.unlock();
                    cvs_[i].wait(lk); // lock is released + thread blocked
                    if (terminate_ > -1) {
                        break;
                    }

                    std::function<void()> f; // retrieve function here
                    tasks_.pop(f); // if tasks_ is empty, will wait for tasks to have at least one. 

                    // std::cout << "QUEUE EMPTY: " << tasks_.is_empty() << '\n';
                    f();  // execute the function
                    //tpm_.lock();

                    //cvs_[i].notify_one();  // always signal that this is
                                           // done. If shutdown/destructor
                                           // isn't waiting, nothing bad gonna
                                           // happen
                    // lock lk falls out of scope here
                    //std::cout << "THREAD "<<i<<" COMPLETED...\n";
                }
                tpm_.lock();
                terminate_++;  // increment terminate (starts at 0)
                tpm_.unlock();
                // std::cout << "THREAD TERMINATING...\n";
            },
            i);
    }
    // after all threads have been started: keep the thread pool alive until
    // shutdown called
}

// thread pool cond var
}  // namespace ra::concurrency