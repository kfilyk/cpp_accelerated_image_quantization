#ifndef QUEUE_H
#define QUEUE_H

#include <condition_variable>
#include <iostream>
#include <list>
#include <mutex>
#include <thread>

using Mutex = std::mutex;
using Lock = std::unique_lock<Mutex>;
using CV = std::condition_variable;

namespace ra::concurrency {
// Concurrent bounded FIFO queue class.
template <class T>
class queue {
   public:
    // The type of each of the elements stored in the queue.
    using value_type = T;

    // An unsigned integral type used to represent sizes.
    using size_type = std::size_t;

    // A type for the status of a queue operation.
    enum class status {
        success = 0,  // operation successful
        empty,        // 1; queue is empty (not currently used)
        full,         // 2; queue is full (not currently used)
        closed,       // 3; queue is closed
    };

    // A queue is not default constructible.
    queue() = delete;

    // Constructs a queue with a maximum size of max_size.
    // The queue is marked as open (i.e., not closed).
    // Precondition: The quantity max_size must be greater than
    // zero.
    queue(size_type max_size) {
        if (max_size < 1) {
            throw std::runtime_error("Max_size must be > 0\n");
        } else {
            closed_ = false;
            max_size_ = max_size;
        }
    }

    // A queue is not movable or copyable.
    queue(const queue &) = delete;
    queue &operator=(const queue &) = delete;
    queue(queue &&) = delete;
    queue &operator=(queue &&) = delete;

    // Destroys the queue after closing the queue (if not already
    // closed) and clearing the queue (if not already empty).
    ~queue() {
        // std::cout << "QUEUE DESTRUCTOR...\n";

        Lock l(m_);
        closed_ = true;
        q_.clear();
        q_.~list();  // call destructor

        // lock goes out of scope -> destructs
    }

    // Inserts the value x at the end of the queue, blocking if
    // necessary.
    // If the queue is full, the thread will be blocked until the
    // queue insertion can be completed or the queue is closed.
    // If the value x is successfully inserted on the queue, the
    // function returns status::success.
    // If the value x cannot be inserted on the queue (due to the
    // queue being closed), the function returns with a return
    // value of status::closed.
    // This function is thread safe.
    // Note: The rvalue reference parameter is intentional and
    // implies that the push function is permitted to change
    // the value of x (e.g., by moving from x).
    status push(value_type &&x) {
        //std::cout<<"Q PUSH\n";
        if (is_closed()) {
            return status::closed;  // dont insert anything
        }
        Lock lk(push_m_);        // wait for lock, if possible
        if (!is_full()) { 
            m_.lock();
            q_.push_back(std::move(x));
            qcv_.notify_one();  // alert condition variable
            m_.unlock();
            return status::success;  
        } 

        // if queue full,
        qcv_.wait(lk, [this] {
            if (is_full()) {
                qcv_.notify_one();  // notify pops
            }
            return !is_full();
        });  // unblocks
        m_.lock();
        q_.push_back(std::move(x));
        qcv_.notify_one(); // only notify pop after a push commences
        m_.unlock();
        return status::success;  // alert cond var
    }

    // Removes the value from the front of the queue and places it
    // in x, blocking if necessary.
    // If the queue is empty and not closed, the thread is blocked
    // until: 1) a value can be removed from the queue; or 2) the
    // queue is closed.
    // If the queue is closed, the function does not block and either
    // returns status::closed or status::success, depending on whether
    // a value can be successfully removed from the queue.
    // If a value is successfully removed from the queue, the value
    // is placed in x and the function returns status::success.
    // If a value cannot be successfully removed from the queue (due to
    // the queue being both empty and closed), the function returns
    // status::closed.
    // This function is thread safe.
    status pop(value_type &x) {
        //std::cout<<"Q POP\n";
        if (is_closed() && is_empty()) {
            return status::closed;
        }

        Lock lk(pop_m_);
        if (!is_empty()) { 
            m_.lock();
            x = q_.front();  // call copy constructor
            q_.pop_front();
            qcv_.notify_one();  // alert condition variable
            m_.unlock();
            return status::success;  
        } 

        qcv_.wait(lk, [this] {
            if (is_empty()) {
                qcv_.notify_one();  // notify pops
            }
            // std::cout << "QUEUE LAMBDA POP...\n";
            return !is_empty();
        });  // unlocks lock
        m_.lock();
        x = q_.front();  // call copy constructor
        q_.pop_front();
        qcv_.notify_one();
        m_.unlock();
        //std::cout<<"Q POP DONE\n";

        return status::success;
    }
    // Closes the queue.
    // The queue is placed in the closed state.
    // The closed state prevents more items from being inserted
    // on the queue, but it does not clear the items that are
    // already on the queue.
    // Invoking this function on a closed queue has no effect.
    // This function is thread safe.
    void close() {
        Lock l(m_);  // create a lock to check closed_status
        closed_ = true;
    }

    // Clears the queue.
    // All of the elements on the queue are discarded.
    // This function is thread safe.
    void clear() {
        Lock l(m_);  //
        q_.clear();  // delete all
    }

    // Returns if the queue is currently full (i.e., the number of
    // elements in the queue equals the maximum queue size).
    // This function is not thread safe.
    bool is_full() {
        Lock l(m_);  //
        return q_.size() == max_size_;

    }

    // Returns if the queue is currently empty.
    // This function is not thread safe.
    bool is_empty() { 
        Lock l(m_);  //
        return q_.size() == 0; 
    }

    // Returns if the queue is closed (i.e., in the closed state).
    // This function is not thread safe.
    bool is_closed() const { return closed_; }

    // Returns the maximum number of elements that can be held in
    // the queue.
    // This function is not thread safe.
    size_type max_size() const { return max_size_; }

   private:
    bool closed_;
    size_type max_size_;
    status s_;
    std::list<value_type> q_;
    Mutex m_;  // being used
    Mutex push_m_;  // being used
    Mutex pop_m_;  // being used

    CV qcv_;
};

// want to be able to push and pop without blocking one another

}  // namespace ra::concurrency

#endif