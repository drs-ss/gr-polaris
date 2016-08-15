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

/** @file complex_manager.h
 *  @brief Contains complex_manager class and defines used in
 *  data processing.
 *  
 *  This file holds the class definitions for the
 *  complex_manager class.  It also contains defines
 *        for tweaking data processing. */

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <volk/volk.h>
#include <sys/time.h>
#include <boost/atomic.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
#include "udp_listener.h"
#include "task.h"

/** 
 * @def NUM_COMPLEX 
 * @brief Defines the number of processed complex samples to 
 *        store.
 *  
 * NUM_COMPLEX is the size of your buffers which hold processed 
 * complex samples.  There are two buffers per stream, which 
 * means the total amount of memory usage is NUM_STREAMS * 2 * 
 * NUM_COMPLEX * sizeof(std::complex<float>). 
*/
#define NUM_COMPLEX 200000000

/** 
 * @def COMPLEX_PER_PACKET 
 * @brief Defines the number of complex samples expected in each 
 *        packet.
 *  
 * COMPLEX_PER_PACKET is the number of complex samples per 
 * packet. 
*/
#define COMPLEX_PER_PACKET 994

/** 
 * @def DATA_START_INDEX 
 * @brief Defines the start index (in 32 bit words) of the 
 *        samples in a packet.
 *  
 * DATA_START_INDEX is the start index of samples in V49 
 * buffers.  The packets are indexed as boost::uint32_t. 
*/
#define DATA_START_INDEX 5

/** 
 * @def NUM_STREAMS 
 * @brief Defines the maximum number of streams this 
 *        complex_manager can collect.
 *  
 * NUM_STREAMS is the maximum number of simultaneous streams the
 * complex manager will be able to collect. 
 *  
*/
#define NUM_STREAMS 8

/**
 * @def NUM_THREADS 
 * @brief Defines the number of threads to be used for data processing. 
 *  
 * NUM_THREADS is the maximum number of threads which can 
 * process packets simultaneously. 
 */
#define NUM_THREADS 10

/** 
 * @def NUM_RECURSIVE 
 * @brief Defines the number of times fill_buffers should try to 
 *        obtain data before giving up.
 *  
 * NUM_RECURSIVE is the number of times fill_buffers should try 
 * to obtain data before returning empty. 
*/
#define NUM_RECURSIVE 10

/** 
 * @def MAX_PACKET_COUNT 
 * @brief Defines the maximum packet counter 
 *  
 * MAX_PACKET_COUNT is the maximum packet counter before the 
 * counter loops back to 0. 
*/
#define MAX_PACKET_COUNT 15

/** 
 * @def PACKET_LOSS_MSG
 * @brief Defines the packet loss message
 *  
 * PACKET_LOSS_MSG is the message that gets displayed to the 
 * user when packet loss occurs.  You get one L per packet lost.
  */ 
#define PACKET_LOSS_MSG "L"

/** 
 * @def IQ_SCALE_FACTOR 
 * @brief Defines the scaler value for IQ data
 *  
 * IQ_SCALE_FACTOR is the scaler value we divide our IQ samples 
 * by.  We do this because GNU radio expects the samples to be 
 * normalized. 
*/
#define IQ_SCALE_FACTOR 32768.0f

/**  
 * complex_manager class for stripping complex values out of UDP
 * data. 
 *  
 * It's buffers are matched to specific tuners on the polaris, 
 * as opposed to the the POLSRC_impl where the buffers are 
 * matched to output streams. 
 */
class complex_manager
{
private:
    /* 
       buffer_manager assists with keeping track of
       variables associated with memory management.
    */
    struct buffer_manager
    {
        std::complex<float> *out_buff[2];
        int out_pointer;
        int out_count[2];
        int last_count;
        int read_index[2];
        bool tuner_valid_safe;
        bool tuner_valid;
        bool flip;
        
        std::complex<float>* get_active_buffer(
            boost::uint32_t &write_index)
        {
            write_index = out_count[out_pointer];
            return out_buff[out_pointer];
        };

        std::complex<float>* get_read_buffer()
        {
            return &out_buff[!out_pointer][read_index[!out_pointer]];
        };
    };

    buffer_manager m_mang[NUM_STREAMS];

    // Pointer to a buffer of packets
    aligned_buffer::sptr m_saved_packets;

    boost::scoped_ptr<udp_listener> m_udp_listener;
    boost::scoped_ptr<boost::thread> m_thread;

    volatile bool m_run;
    volatile bool m_request_flip;
    volatile int  m_request_amount;
    volatile bool m_update_valid_streams;

    // Used to allocated aligned buffers.
    aligned_buffer::sptr m_aligned_buffs;

    // Shared memory between processing threads.
    boost::scoped_ptr<task_impl> m_process_tasks[NUM_THREADS];
    boost::uint32_t *m_packet_buffer[NUM_THREADS];
    std::complex<float> *m_target_buffer[NUM_THREADS];
    boost::uint32_t m_start_index[NUM_THREADS];

    // Helper functions
    bool inline threads_active();
    void inline process_packet(int i);
    void inline handle_flip();
    void inline handle_update();
public:
    /**
     * Construct a new complex_manager, the ip and port is passed 
     * down to a UDPListener which the complex_manager will manage. 
     * 
     * @param ip 
     * @param port 
     */
    complex_manager(std::string ip, int port);

    ~complex_manager();

    /**
     * Stops and shutdown this complex manager, and it's associated 
     * processing threads and UDP listener. 
     * 
     */
    void stop();

    /**
     * Sets given input streams as valid so that data will be 
     * collected.  Passing -1 in any of the tuners will disable that 
     * tuner's collection. 
     * 
     * @param tuners - Array of length n, which lists all enabled 
     *               tuners.
     * @param num_tuners - Length of the tuners array. 
     */
    void update_tuners(int *tuners, int num_tuners);

    /**
     * Fills the requested buffers, with the specified tuner.  Each 
     * buffer recieves the same amount (expected_count). 
     * 
     * @param buffs - A vector of buffers to place the data into.
     * @param tuners - Which tuner should go in each buffer.
     * @param rates - The how many samples should go in each buffer.
     *  
     * @note Amount returned is not guarenteed to be the amount 
     *      requested.  The "rates" parameter is updated to how much
     *      was actually returned into each buffers
     */
    void fill_buffers(std::vector<void *> buffs, int *tuners,
                     int* rates);

    /**
     * This operator will be executed inside of a seperate thread, 
     * which is set off inside of the constructor when you create a 
     * complex_manager. 
     * 
     */
    void operator ()();
};
