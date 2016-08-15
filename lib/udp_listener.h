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
/** @file udp_listener.h
 *  @brief Contains udp_listener class and defines used in
 *  packet collection.
 *  
 *  This file holds the class declaration for the
 *  udp_listener class.  It also contains defines
 *  for tweaking packet collection. */

#ifndef UDPTHREAD_H
#define UDPTHREAD_H

#include <string>
#include <stdlib.h>
#include <iostream>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <iostream>
#include <boost/thread/thread.hpp>
#include <unistd.h>
#include <sched.h>
#include <cstdio>
#include <boost/atomic.hpp>
#include <boost/scoped_ptr.hpp>

#include "aligned_buffer.h"

/** 
 * @def NUM_BUFFS 
 * @brief Determines how many packets to buffer in udp_listener. 
 *  
 * NUM_BUFFS is the amount of space this listener has to store 
 * captured data in.  Each buff is 1 packet.  The total amount 
 * of allocated memory will be twice this value times the 
 * PACKET_SIZE. 
  */  
#define NUM_BUFFS 64000

/** 
 * @def PACKET_SIZE 
 * @brief Determines the size in bytes of packets to collect. 
 *  
 * PACKET_SIZE is the size (in bytes) of the UDP packets to 
 * receive. 
*/
#define PACKET_SIZE 4000

/** 
 * @def RECV_BUFF_SIZE 
 * @brief Size of the systems available recieve buffer. 
 *  
 * RECV_BUFF_SIZE is the size of the system's recv buffer 
 * (net.core.rmem_max value in sysctl.conf) 
*/
#define RECV_BUFF_SIZE 50000000

/** 
 * @def LONG_USLEEP 
 * @brief Defines the wait time for non time critical waiting
 *  
 * LONG_USLEEP is the amount of time for a thread to sleep when 
 * waiting for non critical functions to be processed. 
*/
#define LONG_USLEEP 100

/** 
 * @def SHORT_USLEEP 
 * @brief Defines the wait time for time critical requests
 *  
 * SHORT_USLEEP is the amount of time in microseconds for a 
 * thread to sleep when waiting for a time sensitive request to 
 * be processed. 
*/
#define SHORT_USLEEP 5


/**
 * UDPListener class listens for UDP packets to come in and 
 * holds them in a buffer until requested.
 */
class udp_listener
{
private:
    int m_port;
    std::string m_ip;
    bool m_run;
    aligned_buffer::sptr m_buffers[2];
    int m_buff_pointer;
    int m_safe_index;
    int m_safe_side;
    bool m_connected;
    bool m_do_switch;
    int m_last_sequence_number;
    boost::uint32_t m_last_sec;
    boost::uint64_t m_last_frac;
    int m_sock;
    boost::scoped_ptr<boost::thread> m_thread;
public:
    /**
     * Constructs a UDPListener which listens to the specified 
     * address. 
     * 
     * @param ip - IP address to listen to
     * @param port - Port to listen to
     */
    udp_listener(std::string ip, int port);

    ~udp_listener();

    /**
     * Prepares the output buffer to be read outside of this 
     * functor's thread and returns it. 
     * 
     * @param index_count - The length of buffer list.
     * 
     * @return AlignedBuffer::sptr - The list of aligned buffers 
     */
    aligned_buffer::sptr get_buffer_list(int &index_count);

    /**
     * Stops the main loop for this UDPThread.
     */
    void stop();

    /**
     * This is what actually gets run in the boost thread.  It is 
     * essentially an infinite loop of recieve calls. 
     * 
     */
    void operator ()();
};

#endif
