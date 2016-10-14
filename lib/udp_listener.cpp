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

#include "udp_listener.h"

udp_listener::udp_listener(std::string ip, int port)
{
    m_sock = 0;
    m_port = port;
    m_ip = ip;
    m_buff_pointer = 0;
    m_last_sequence_number = -1;
    m_run = true;
    m_do_switch = false;
    m_safe_index = 0;
    m_last_sec = 0;
    m_last_frac = 0;
    m_safe_side = m_buff_pointer;
    m_connected = false;

    m_buffers[0] = aligned_buffer::make(NUM_BUFFS, PACKET_SIZE);
    m_buffers[1] = aligned_buffer::make(NUM_BUFFS, PACKET_SIZE);

    m_thread.reset(new boost::thread(boost::ref(*this)));
}

udp_listener::~udp_listener()
{
    close(m_sock);
}

void 
udp_listener::stop()
{
    m_run = false;
    m_thread->join();
}

aligned_buffer::sptr 
udp_listener::get_buffer_list(int &index_count)
{
    // If we aren't running, return empty.
    if (!m_run || !m_connected) {
        index_count = 0;
        return m_buffers[m_safe_side];
    }

    // Send our request to the other thread.
    m_do_switch = true;

    // Wait until our request is processed...
    while (m_do_switch && m_run) usleep(SHORT_USLEEP);

    // Check if we broke out of the wait due to the thread halting.
    if (!m_run) {
        index_count = 0;
        return m_buffers[m_safe_side];
    }

    // Let them know how many we've done since this was last called.
    index_count = m_safe_index;

    // Return back the new buffer data.
    return m_buffers[m_safe_side];
}

void 
udp_listener::operator()()
{
    try {
        m_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (m_sock < 0) {
            std::cout << "Failed to bind socket..." << std::endl;
            return;
        }
        struct sockaddr_in myaddr;
        memset((char *)&myaddr, 0, sizeof(myaddr));
        myaddr.sin_family = AF_INET;
        myaddr.sin_addr.s_addr = inet_addr(m_ip.c_str());
        myaddr.sin_port = htons(m_port);

        if (bind(m_sock, (struct sockaddr *)&myaddr, 
                 sizeof(myaddr)) < 0) {
            m_connected = false;
            std::cout << "Failed to bind socket..." << std::endl;
        }

        int n = RECV_BUFF_SIZE;
        if (setsockopt(m_sock, SOL_SOCKET, SO_RCVBUF, &n,
                       sizeof(n)) == -1) {
            std::cout << "Failed to set socket buffer size to " 
                << n << "." << std::endl;
        }
        m_connected = true;
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        std::cout << "Failed to setup socket for UDP connection."
            << std::endl;
        m_connected = false;
    }

    if (m_connected) {
        int overflow = 0;
        int reclen = 0;
        int cur_index = 0;
        boost::uint32_t* cur_buff = NULL;
        while (m_run) {
            cur_buff = reinterpret_cast<boost::uint32_t *>
                (m_buffers[m_buff_pointer]->at(cur_index));
            if (cur_buff == NULL)
                break;
            while (
               (reclen = 
                recv(m_sock, cur_buff, PACKET_SIZE, MSG_DONTWAIT)) < 0
               && m_run 
               && !m_do_switch);
            if (reclen == PACKET_SIZE) {
                cur_index += 1;
                if (cur_index >= NUM_BUFFS) {
                    overflow = 1;
                    cur_index = 0;
                }
            }
            if (m_do_switch) {
                if (overflow > 0) {
                    std::cout << OVERFLOW_MSG;
                    overflow = 0;
                }
                if (cur_index > 0) {
                    m_safe_side = m_buff_pointer;
                    m_buff_pointer = !m_buff_pointer;
                    m_safe_index = cur_index;
                    cur_index = 0;
                } else {
                    m_safe_index = cur_index;
                }
                m_do_switch = false;
            }
        }
    }
}
