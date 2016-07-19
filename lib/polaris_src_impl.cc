/* -*- c++ -*- */
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "polaris_src_impl.h"

namespace gr
{
namespace polaris
{

/**
 * Called by GNURadio to construct our POLSRC_impl.
 * 
 * @param ip - IP of POLARIS to connect to for TCP commands 
 * @param port - UDP port for commands
 * @param streamip - IP of UDP stream
 * @param fibip - IP of the fiber connection 
 * @param num_outputs - Number of output streams this block has 
 * @param i_op - Turns on independent operation for tuners 
 * 
 * @return POLSRC::sptr 
 */
polaris_src::sptr
polaris_src::make(std::string ip, int port, std::string streamip,
                  std::string fibip, int num_outputs, bool i_op, 
                  int phys)
{
    return gnuradio::get_initial_sptr
           (new polaris_src_impl(ip, streamip, fibip, port, 
                                 num_outputs, i_op, phys));
}

/**
 * The private constructor
 */
polaris_src_impl::polaris_src_impl(std::string ip, 
                                   std::string streamip, 
                                   std::string fibip, 
                                   int port, 
                                   int num_outputs, 
                                   bool independent_operation, 
                                   int phys)
    : gr::sync_block("POLSRC",
                     gr::io_signature::make(0, 0, 0),
                     gr::io_signature::make(num_outputs, 
                                            num_outputs, 
                                            sizeof(gr_complex)))
{
    m_i_op = independent_operation;
    m_num_output_streams = num_outputs;
    m_address = ip;
    m_stream_address = streamip;
    m_rec_port = port;
    m_port = TCP_PORT;
    m_fiber_address = fibip;

    m_samp_rate = DEFAULT_SAMP_RATE;
    m_freqs.reset(new double[m_num_output_streams]);
    m_attens.reset(new double[m_num_output_streams]);
    m_tuners.reset(new int[m_num_output_streams]);
    m_preamps.reset(new bool[m_num_output_streams]);

    for (int i = 0; i < m_num_output_streams; i++) {
        m_freqs[i] = DEFAULT_FREQ;
        m_attens[i] = 0;
        m_tuners[i] = -1;
        m_preamps[i] = false;
    }

    m_phys_port = phys;
    if (m_phys_port < 0 || m_phys_port > 1) {
        std::cout << "Setting physical output port to 0" 
            << std::endl;
        m_phys_port = 0;
    }

    // Begin network setup.
    try_connect();
    send_message(DISABLE_OUTPUT_MNE, -1);
}

/**
 * Our virtual destructor.
 */
polaris_src_impl::~polaris_src_impl()
{
    close(m_sock);
}

bool 
polaris_src_impl::start()
{
    try {
        if (m_connected) {
            m_complex_manager.reset(
                new complex_manager(m_fiber_address,m_rec_port));
            m_complex_manager->update_tuners(m_tuners.get(), 
                                              m_num_output_streams);
            // Enable data streams.
            send_message(ENABLE_OUTPUT_MNE, -1);
        }
    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
    }
    return true;
}

bool 
polaris_src_impl::stop()
{
    // Tell the POLARIS to stop transmitting
    send_message("RCH0;STE1,1,0;STE2,1,0;STE3,1,0;STE4,1,0;CFG0",-1);
    if (m_complex_manager) {
        m_complex_manager->stop();
    }
    return true;
}

void 
polaris_src_impl::try_connect()
{
    std::cout << "Attempting to connect to Polaris..." << std::endl;
    // Let's attempt to establish a TCP connection to POLARIS
    m_connected = connect_polaris(m_address, m_port);
}

void 
polaris_src_impl::set_freq(double freq, int stream)
{
    if (m_connected) {
        if (stream > 0 && stream <= m_num_output_streams) {
            int tuner;
            m_freqs[stream - 1] = freq;
            tuner = m_tuners[stream - 1];

            if (tuner < 0) return;

            freq = freq / MHZ_SCALE;
            std::ostringstream s;
            if (freq >= MIN_FREQ_MHZ && freq <= MAX_FREQ_MHZ) {
                s.str("");
                s << FRQ_MNE(tuner, 1, freq);
                send_message(s.str(), -1);
            }
        }
    }
}

void 
polaris_src_impl::set_atten(double atten, int stream)
{
    if (m_connected) {
        if (stream > 0 && stream <= m_num_output_streams) {
            int tuner;
            m_attens[stream - 1] = atten;
            tuner = m_tuners[stream - 1];

            if (tuner < 0) return;

            if (atten >= MIN_ATTEN && atten <= MAX_ATTEN) {
                std::ostringstream s;
                s.str("");
                s << ATN_MNE(tuner, atten);
                send_message(s.str(), -1);
            }
        }
    }
}

void 
polaris_src_impl::set_tuner(int tuner, int stream)
{
    if (m_connected) {
        if (stream > 0 && stream <= m_num_output_streams) {
            m_tuners[stream - 1] = tuner;
        }
    }
}

void 
polaris_src_impl::update_preamp(bool pam, int stream)
{
    if (stream > 0 && stream <= m_num_output_streams) {
        m_preamps[stream - 1] = pam;
        if (m_tuners[stream - 1] > 0) {
            std::stringstream s;
            s << PAM_MNE(m_tuners[stream - 1], (pam ? 1 : 0));
            send_message(s.str(), -1);
        }
    }
}

void 
polaris_src_impl::update_tuners(int tuner, int stream)
{
    
    if (stream > m_num_output_streams) {
        tuner = -1;
    }
    if (tuner > 0 && stream <= m_num_output_streams) {
        for (int i = 0; i < m_num_output_streams; i++) {
            if (tuner == m_tuners[i] && (i + 1) != stream) {
                std::cout << "No two tuners may match. "
                             "Another tuner is already set to "
                             << tuner << std::endl;
                return;
            }
        }
    }
    send_message(DISABLE_OUTPUT_MNE, -1);
    set_tuner(tuner, stream);

    std::stringstream s;
    s << STE_MNE(1,1,0) << STE_MNE(2,1,0) << STE_MNE(3,1,0) << STE_MNE(4,1,0);
    send_message(s.str(), -1);
    for (int i = 1; i <= 4; i++) {
        for (int j = 0; j < m_num_output_streams; j++) {
            if (m_tuners[j] == i) {
                s.str("");
                s << STE_MNE(i, 1, 1);
                send_message(s.str(), -1);
                update_atten(m_attens[j],j + 1);
                update_freq(m_freqs[j],j + 1);
                update_preamp(m_preamps[j],j + 1);
            }
        }
    }

    if (m_complex_manager) {
        m_complex_manager->update_tuners(m_tuners.get(),
                                          m_num_output_streams);
    }
    send_message(ENABLE_OUTPUT_MNE, -1);
}

void 
polaris_src_impl::update_freq(double freq, int stream)
{
    set_freq(freq, stream);
}

void 
polaris_src_impl::update_samprate(double sr)
{
    send_message(DISABLE_OUTPUT_MNE, -1);
    sr = sr / 1000000;
    std::ostringstream s;
    for (int i = 1; i <= NUM_STREAMS; i++) {
        if (sr >= MIN_SAMP_RATE_MHZ && sr <= MAX_SAMP_RATE_MHZ) {
            s.str("");
            s << SPR_MNE(i, 1, sr);
            send_message(s.str(), -1);
        }
    }
    send_message(ENABLE_OUTPUT_MNE, -1);
}

void 
polaris_src_impl::update_atten(double atten, int stream)
{
    set_atten(atten, stream);
}

void
polaris_src_impl::forecast(int noutput_items,
                           gr_vector_int &ninput_items_required)
{
    // No input required, we are a source block.
    ninput_items_required[0] = 0;
}

int
polaris_src_impl::work(int noutput_items,
                  gr_vector_const_void_star &input_items,
                  gr_vector_void_star &output_items)
{
    if (!m_connected) {
        // No TCP connection was established.  Don't do anything.
        return 0;
    } else {
        // Request the ComplexManager fill our buffers
        int num_copied = m_complex_manager->fill_buffers(
            output_items, m_tuners.get(), noutput_items);
        // Return how many values we were able to get for GNURadio.
        return num_copied;
    }
}

/**
 * Attempts to connect to POLARIS and send the setup commands 
 * for the data stream. 
 * 
 * @param ip 
 * @param port 
 * 
 * @return bool 
 */
bool 
polaris_src_impl::connect_polaris(std::string ip, int port)
{
    m_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sock < 0) {
        std::cout << "TCP Connection Failed" << std::endl;
        return false;
    }
    m_server.sin_addr.s_addr = inet_addr(ip.c_str());
    m_server.sin_family = AF_INET;
    m_server.sin_port = htons(port);
    if (connect(m_sock, (struct sockaddr *)&m_server, sizeof(m_server))
         < 0) {
        std::cout << "TCP Connection Failed" << std::endl;
        return false;
    }
    std::cout << "Connection Established" << std::endl;
    std::cout << "Setting up POLARIS..." << std::endl;
    /*
    This loop will setup the requested networking, 
    disabled all output streams, and set the radio 
    to independent operation if requested. 
    */
    std::stringstream s;
    s << CFG_MNE(1);
    send_message(s.str(), -1);
    for (int i = 1; i <= 4; i++) {
        for (int j = 1; j <= 2; j++) {
            s.str("");
            s << UDP_MNE(i,j,m_stream_address,m_rec_port,"FF:FF:FF:FF:FF:FF");
            send_message(s.str(), -1);
            s.str("");
            s << SIP_MNE(i,j,m_fiber_address,m_rec_port,"FF:FF:FF:FF:FF:FF");
            send_message(s.str(), -1);
            s.str("");
            s << STE_MNE(i, 1, 0);
            send_message(s.str(), -1);
            if (m_i_op) {
                s.str("");
                s << ENABLE_IOP;
                send_message(s.str(), -1);
            }
        }
    }
    s.str("");
    s << CFG_MNE(0);
    send_message(s.str(), -1);

    s.str("");
    s << STO_MNE(1,1,m_phys_port);
    send_message(s.str(), -1);
    s.str("");
    s << STO_MNE(2,1,m_phys_port);
    send_message(s.str(), -1);
    s.str("");
    s << STO_MNE(3,1,m_phys_port);
    send_message(s.str(), -1);
    s.str("");
    s << STO_MNE(4,1,m_phys_port);
    send_message(s.str(), -1);

    std::cout << "Setup Complete" << std::endl;
    return true;
}

/**
 * Sends the message to the already connected POLARIS system.
 *  If timeout is >=0 it will wait for a response back from
 *  POLARIS and return it. If nothing is recieved before timeout
 *  is reached an empty string is returned. If timeout is <0
 *  empty string will be returned.
 * 
 * @param s 
 * @param timeout 
 * 
 * @return std::string 
 */
std::string 
polaris_src_impl::send_message(std::string s, int timeout)
{
    s.append("\r\n");
    if (send(m_sock, s.c_str(), strlen(s.c_str()), 0) < 0) {
        return "";
    }
    if (timeout < 0) return "";

    char buff[256];
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    int check = setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, 
                           (char *)&tv, sizeof(struct timeval));
    if (check < 0) {
        std::cout << "Error setting socket options." << std::endl;
        return "";
    }

    int err = 2000;
    if ((err = recv(m_sock, buff, sizeof(buff), 0)) < 0) {
        return "";
    }

    buff[err + 1] = '\0';
    std::string retVal(buff);

    return retVal;
}

} /* namespace polaris */
} /* namespace gr */

