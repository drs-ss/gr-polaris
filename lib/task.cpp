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

#include "task.h"

task_impl::task_impl(const boost::function<void(void)> &task_fcn)
{
    started = false;
    running = true;
    my_thread.reset(
        new boost::thread(
            boost::bind(&task_impl::task_loop, this, task_fcn)));
}

task_impl::~task_impl()
{
}

void 
task_impl::stop_thread()
{
    my_thread->interrupt();
    my_thread->join();
}

bool 
task_impl::is_running()
{
    return started;
}

void 
task_impl::wake_up_thread()
{
    boost::unique_lock<boost::mutex> lck(mtx);
    started = true;
    cv.notify_one();
}

void 
task_impl::task_loop(const boost::function<void(void)> &task_fcn)
{
    try {
        while (running) {
            boost::unique_lock<boost::mutex> lck(mtx);
            started = false;
            while (!started) cv.wait(lck);
            task_fcn();
        }
    } catch (const boost::thread_interrupted&) {
        //this is an ok way to exit the task loop
    } catch (const std::exception &e) {
        std::cout << "An error occured "
                     "in the task loop." << std::endl;
        std::cout << e.what() << std::endl;
    } catch (...) {
        //FIXME
        //Unfortunately, this is also an ok way to end a task,
        //because on some systems boost throws uncatchables.
    }
}

