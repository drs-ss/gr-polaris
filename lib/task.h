/* 
 * Copyright 2016 DRS.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */
 // Original source taken from https://github.com/EttusResearch/uhd

/** @file task.h
 *  @brief Contains the task_impl object.
 *  
 *  This file holds the class definitions for the task_impl
 *  class. */

#include <boost/thread/thread.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/function.hpp>
#include <exception>
#include <iostream>
#include <vector>

/**
 * The task_impl class is a helper class which continously 
 * calls a single function and sleeps in between calls until 
 * waken up again. 
 */
class task_impl
{
public:
    /**
     * Create a new task_impl.  When created, a new thread is 
     * spawned and will wait until wakeUpThread() is called. 
     * 
     * 
     * @param task_fcn - Function pointer to call inside of the 
     *                 task_loop.
     */
    task_impl(const boost::function<void(void)> &task_fcn);
    ~task_impl();

    /**
     * Check if this thread is currently executing the task_fcn.
     *
     * 
     * @return bool - True if the thread is executing the task_fcn,
     *         false otherwise.
     */
    bool is_running();

    /**
     * Wakes up this task's thread so that the task_fcn will be 
     * called one time. 
     * 
     */
    void wake_up_thread();

    /**
     * Stops the threads and joins it.
     * 
     */
    void stop_thread();

private:
    void task_loop(const boost::function<void(void)> &task_fcn);

    volatile bool started;
    boost::scoped_ptr<boost::thread> my_thread;
    volatile bool running;
    boost::mutex mtx;
    boost::condition_variable cv;
};

