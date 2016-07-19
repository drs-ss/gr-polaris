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

#include "complex_manager.h"

complex_manager::complex_manager(std::string ip, int port)
{
    // Setup processing threads
    for (int i = 0; i < NUM_THREADS; i++) {
        m_packet_buffer[i] = NULL;
        m_target_buffer[i] = NULL;
        m_start_index[i] = 0;
        m_process_tasks[i].reset(new task_impl(
            boost::bind(&complex_manager::process_packet, this, i)));
    }

    m_request_flip = false;
    m_request_amount = 0;
    m_safety_count = 0;

    for (int i = 0; i < NUM_STREAMS; i++) {
        m_mang[i].out_count[0] = 0;
        m_mang[i].out_count[1] = 0;
        m_mang[i].last_count = -1;
        m_mang[i].read_index[0] = 0;
        m_mang[i].read_index[1] = 0;
        m_mang[i].out_pointer = 0;
        m_mang[i].tuner_valid = false;
        m_mang[i].tuner_valid_safe = false;
        m_mang[i].flip = false;
    }

    m_update_valid_streams  = false;

    m_run = true;

    // Create a pool of aligned buffers
    m_aligned_buffs = aligned_buffer::make(
        NUM_STREAMS * 2, 
        NUM_COMPLEX * sizeof(std::complex<float>));

    // Save these aligned buffer pointers
    for (int i = 0; i < NUM_STREAMS; i++) {
        m_mang[i].out_buff[0] = reinterpret_cast<std::complex<float> *>
            (m_aligned_buffs->at(i));
        m_mang[i].out_buff[1] = reinterpret_cast<std::complex<float> *>
            (m_aligned_buffs->at(i + NUM_STREAMS));
    }

    // Spawn our UDP collection thread.
    m_udp_listener.reset(new udp_listener(ip, port));

    // Create a thread for our own loop to run in.
    m_thread.reset(new boost::thread(boost::ref(*this)));
}

complex_manager::~complex_manager()
{
}

void 
complex_manager::stop()
{
    m_udp_listener->stop();

    m_run = false;
    m_thread->join();

    for (int i = 0; i < NUM_THREADS; i++) {
        m_process_tasks[i]->stop_thread();
    }
}

bool 
complex_manager::threads_active()
{
    for (int i = 0; i < NUM_THREADS; i++) {
        if (m_process_tasks[i]->is_running()) {
            return true;
        }
    }
    return false;
}

void 
complex_manager::update_tuners(int *tuners, int num_tuners)
{
    for (int i = 0; i < NUM_STREAMS; i++) {
        m_mang[i].tuner_valid_safe = false;
        for (int j = 0; j < num_tuners; j++) {
            if ((i + 1) == tuners[j]) {
                m_mang[i].tuner_valid_safe = true;
            }
        }
    }

    m_update_valid_streams = true;
    while (m_update_valid_streams) {
        usleep(LONG_USLEEP);
    }
}

int 
complex_manager::fill_buffers(std::vector<void *> buffs,
                                  int *tuners,
                                  int count)
{
    if (m_update_valid_streams)
        return 0;

    // Prevent infinite recursion.
    m_safety_count += 1;
    /* 
    First we have to know what amount of data we have available to
        ship out. 
    */ 
    int min = count;
    int i = 0;
    for (; i < NUM_STREAMS; i++) {
        m_mang[i].flip = false;
        if (m_mang[i].out_count[!m_mang[i].out_pointer] <= min
            && m_mang[i].tuner_valid) {
            min = m_mang[i].out_count[!m_mang[i].out_pointer];
            if (min == 0) {
                m_mang[i].flip = true;
            }
        }
    }

    if (min == 0) {
        /* 
        If minimum was set to 0 at least 1 of the read buffers
            didn't have any items in it.
        Request the processing loop flip the buffers we set, and
            see if we had any data waiting.
        */
        m_request_flip = true;
        while (m_request_flip) {
            usleep(SHORT_USLEEP);
        }
        if (m_safety_count < NUM_RECURSIVE) {
            // Try to get data again.
            return fill_buffers(buffs, tuners, count);
        } else {
            // Otherwise just return 0 this time, they'll be back.
            m_safety_count = 0;
            return 0;
        }
    }

    // Temporary pointers to copy targets
    void *targets[NUM_STREAMS] = { };
    for (i = 0; i < buffs.size(); i++) {
        if (tuners[i] <= NUM_STREAMS && tuners[i] > 0) {
            targets[tuners[i] - 1] = buffs[i];
        }
    }

    // Remove the minimum amount from each of the buffers.
    int cpy_amnt = min * sizeof(std::complex<float>);
    for (i = 0; i < NUM_STREAMS; i++) {
        if (targets[i] != NULL) {
            memcpy(targets[i], m_mang[i].get_read_buffer(), 
                   cpy_amnt);
            m_mang[i].read_index[!m_mang[i].out_pointer] += min;
            m_mang[i].out_count[!m_mang[i].out_pointer] -= min;
        }
    }

    m_safety_count = 0;
    return min;
}

void 
complex_manager::handle_flip()
{
    // Flip our read/write buffers;  Wait for our threads to finish.
    while (threads_active()) {
        usleep(SHORT_USLEEP);
    }
    for (int i = 0; i < NUM_STREAMS; i++) {
        if (m_mang[i].tuner_valid && m_mang[i].flip) {
            m_mang[i].out_pointer = !m_mang[i].out_pointer;
            m_mang[i].out_count[m_mang[i].out_pointer] = 0;
            m_mang[i].read_index[0] = 0;
            m_mang[i].read_index[1] = 0;
            m_mang[i].flip = false;
        }
    }
}

void 
complex_manager::handle_update()
{
    // Apply updates to our valid streams.  First stop the threads.
    while (threads_active()) {
        usleep(SHORT_USLEEP);
    }
    for (int i = 0; i < NUM_STREAMS; i++) {
        m_mang[i].tuner_valid = m_mang[i].tuner_valid_safe;
        m_mang[i].out_count[0] = 0;
        m_mang[i].out_count[1] = 0;
        m_mang[i].last_count = -1;
        m_mang[i].read_index[0] = 0;
        m_mang[i].read_index[1] = 0;
        m_mang[i].out_pointer = 0;
    }
}

void 
complex_manager::operator()()
{
    int length = 0;
    while (m_run) {
        if (m_request_flip) {
            handle_flip();
            m_request_flip = false;
        }
        if (m_update_valid_streams) {
            handle_update();
            m_update_valid_streams = false;
        }
        /* 
        Make sure we aren't still using packet data before we 
        request new data.
        */
        while (threads_active()) {
            usleep(SHORT_USLEEP);
        }
        // Request a new buffer full of packets.
        length = 0;
        m_saved_packets = m_udp_listener->get_buffer_list(length);
        if (length == 0) {
            // There wasn't anything to process, try again.
            usleep(LONG_USLEEP);
            continue;
        }
        // Process all available saved_packets
        int index = 0;
        while (
               // While we have packets to process...
               index < length &&
               // and should be running...
               m_run) {
            if (m_update_valid_streams) {
                handle_update();
                m_update_valid_streams = false;
            }
            if (m_request_flip) {
                handle_flip();
                m_request_flip = false;
            }
            // Look for a sleeping thread to wake up for processing.
            int setup_thread = -1;
            while (m_run && setup_thread < 0) {
                for (int i = 0; i < NUM_THREADS; i++) {
                    if (!m_process_tasks[i]->is_running()) {
                        setup_thread = i;
                        break;
                    }
                }
            }
            if (setup_thread  < 0) {
                continue;
            }
            m_packet_buffer[setup_thread] = 
                reinterpret_cast<boost::uint32_t *>
                (m_saved_packets->at(index));

            // Grab only what we need from the Vita49 packet.
            boost::uint32_t flipped = 
                ntohl(m_packet_buffer[setup_thread][0]);
            boost::uint32_t got = 
                ((flipped & static_cast<boost::uint32_t>(0xF0000)) >> 16);
            flipped = ntohl(m_packet_buffer[setup_thread][1]);
            boost::uint32_t stream_id = flipped;
            // Map stream_id to an index.
            int expected = 0;
            if (stream_id == 0) {
                stream_id = 0;
            } else if (stream_id == 2) {
                stream_id = 1;
            } else if (stream_id == 4) {
                stream_id = 2;
            } else if (stream_id == 6) {
                stream_id = 3;
            } else {
                std::cout << "Invalid stream ID recieved : " 
                    << stream_id << std::endl;
                index += 1;
                continue;
            }
            if (!m_mang[stream_id].tuner_valid) {
                // This stream isn't valid, ignore this packet.
                index += 1;
                continue;
            }
            // Prepare our target buffer.
            m_target_buffer[setup_thread] = 
                m_mang[stream_id].get_active_buffer(
                    m_start_index[setup_thread]);

            if (m_start_index[setup_thread] + COMPLEX_PER_PACKET
                >= NUM_COMPLEX) {
                // Our buffer is full, wait a second and try again.
                for (int j = 0; j < NUM_STREAMS; j++) {
                    if (m_mang[j].tuner_valid && m_mang[j].out_count[m_mang[j].out_pointer] == 0) {
                        index += 1;
                        std::cout << CAPPING_LOSS_MSG;
                        m_mang[stream_id].last_count = got;
                        break;
                    }
                }
                usleep(SHORT_USLEEP);
                continue;
            }

            // Get packet counter, update our stored value for them.
            expected = m_mang[stream_id].last_count;
            m_mang[stream_id].last_count = got;

            // Update our packet counters for comparison.
            expected += 1;
            if (expected == 0) {
                expected = got;
            } else if (expected > MAX_PACKET_COUNT) {
                expected = 0;
            }

            if (expected != got) {
                // Report packet loss to the user.
                std::cout << PACKET_LOSS_MSG;
            }

            // Increment our counter
            m_mang[stream_id].out_count[m_mang[stream_id].out_pointer]
                += COMPLEX_PER_PACKET;

            // Start the thread.
            m_process_tasks[setup_thread]->wake_up_thread();

            // Do the next packet.
            index += 1;
        }
    }
    // Wait for any active threads to finish before ending.
    while (threads_active()) {
        usleep(LONG_USLEEP);
    }
}

/* 
Takes a packet out of saved_packets at index i and 
processes it into the current write buffer.
*/
void 
inline complex_manager::process_packet(int i)
{
    if (m_packet_buffer[i] == NULL) {
        return;
    }
    // Loop through and create our complex values
    int start = DATA_START_INDEX;
    int end = start + COMPLEX_PER_PACKET;
    for (int j = start; j < end; j++) {
        boost::uint32_t flipped = ntohl(m_packet_buffer[i][j]);
        // Get the left and right original values as int16's.
        boost::int16_t left_original = 
            static_cast<boost::int16_t>
            (((flipped & (boost::uint32_t)0xFFFF0000)) >> 16);
        boost::int16_t right_original =
            static_cast<boost::int16_t>
            (flipped & (boost::uint32_t)0xFFFF);

        // Divide by our scale.
        float left, right;
        left = (static_cast<float>(left_original) / IQ_SCALE_FACTOR);
        right = (static_cast<float>(right_original) / IQ_SCALE_FACTOR);

        // Store our complex value.
        m_target_buffer[i][m_start_index[i] + j - start] = 
            std::complex<float>(left, right);
    }
    m_target_buffer[i] = NULL;
    m_packet_buffer[i] = NULL;
}
