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
polaris_src::make(std::string ip, int port, int mne_port, 
                  std::string streamip, std::string fibip, 
                  int num_outputs, int num_groups, 
                  bool i_op, int phys)
{
    return gnuradio::get_initial_sptr
           (new polaris_src_impl(ip, streamip, fibip, port, mne_port,
                               num_outputs, num_groups, i_op, phys));
}

/**
 * The private constructor
 */
polaris_src_impl::polaris_src_impl(std::string ip, 
                                   std::string streamip, 
                                   std::string fibip, 
                                   int port, 
                                   int mne_port,
                                   int num_outputs,
                                   int num_groups, 
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
    m_num_output_groups = num_groups;
    m_address = ip;
    m_stream_address = streamip;
    m_rec_port = port;
    m_mne_port = mne_port;
    m_fiber_address = fibip;

    m_tuners.reset(new int[MAX_STREAMS]);
    m_group_data.reset(new group_data_struct[m_num_output_groups]);
    std::stringstream s;

    for (int i = 0; i < m_num_output_groups; i++) {
        m_group_data[i].num_ddcs = 0;
        m_group_data[i].ddc_freq[0] = 0;
        m_group_data[i].ddc_freq[1] = 0;
        m_group_data[i].ddc_sr[0] = 0;
        m_group_data[i].ddc_sr[1] = 0;
        m_group_data[i].tuner_freq = 0;
        m_group_data[i].atten = 0;
        m_group_data[i].tuner = -1;
        m_group_data[i].preamp = false;
        m_group_data[i].active = false;
        m_group_data[i].sr_ind[0] = -1;
        m_group_data[i].sr_ind[1] = -1;
    }

    for (int i = 0; i < MAX_STREAMS; i++) {
        m_tuners[i] = -1;
        m_sample_rates[i] = 0;
    }

    m_phys_port = phys;
    if (m_phys_port < 0 || m_phys_port > 1) {
        std::cout << "Setting physical output port to 0" 
            << std::endl;
        m_phys_port = 0;
    }

    m_sr_index =0;
    m_tuner_index = 0;

    // Setup the Polaris...
    try_connect();
    if (m_connected) {
        setup_polaris();
    }
}

/**
 * Our virtual destructor.
 */
polaris_src_impl::~polaris_src_impl()
{
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
            std::stringstream s;
            SEND_STR(s, TOGGLE_STREAMING_CMD(0));
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
    std::stringstream s;
    SEND_STR(s, SHUTDOWN_STREAMING_CMD); 
    if (m_complex_manager) {
        m_complex_manager->stop();
    }
    return true;
}

void 
polaris_src_impl::try_connect()
{
    m_connected = false;
    std::string s_port = (boost::format("%d") % m_mne_port).str();
    if (m_address.size() > 0) {
        try {
            std::cout << "Attempting to connect to the Polaris..."
                    << std::endl;
            boost::asio::ip::tcp::resolver resolver(
                    m_mne_io_service);
            boost::asio::ip::tcp::resolver::query query(m_address,
                    s_port,
                    boost::asio::ip::resolver_query_base::passive);
            boost::asio::ip::tcp::resolver::iterator ter =
                    resolver.resolve(query);

            m_mne_socket.reset(
                    new boost::asio::ip::tcp::socket(
                            m_mne_io_service));

            boost::asio::connect(*m_mne_socket, ter);

            boost::asio::socket_base::reuse_address roption(
                    true);
            m_mne_socket->set_option(roption);

            m_connected = true;
            std::cout << "Connected." << std::endl;
        } catch (std::exception &e) {
            std::cout << "Failed to establish connection: "
                    << std::endl << e.what() << std::endl;
        }
    } else {
        std::cout << "Please enter an IP address for the Polaris."
                << std::endl;
    }
}

void 
polaris_src_impl::set_tuner_freq(double freq, int group, int ddc)
{
    std::ostringstream s;
    if (m_connected) {
        if (group > 0 && group <= m_num_output_groups) {
            int tuner;
            m_group_data[group - 1].tuner_freq = freq;
            tuner = m_group_data[group - 1].tuner;

            if (tuner < 0) 
                return;

            if (freq) {
                freq = freq / MHZ_SCALE;
                if (freq >= MIN_FREQ_MHZ && freq <= MAX_FREQ_MHZ) {
                    std::stringstream s;
                    SEND_STR(s, TUNER_FREQUENCY_CMD(tuner,ddc,freq));
                }else{
                    std::cout << "Please select a frequency between"<<
                        " " << MIN_FREQ_MHZ << "MHz and " <<
                        MAX_FREQ_MHZ << "MHz." << std::endl;
                }
            }
        }
    }
}

void
polaris_src_impl::set_ddc_offset(double offset, int group, int ddc)
{
    std::stringstream s;
    if (m_connected) {
        if (group > 0 && group <= m_num_output_streams) {
            int tuner;
            tuner = m_group_data[group - 1].tuner;

            if (tuner < 0)
                return;

            std::ostringstream s;
            if (offset>= -MAX_DDC_OFFSET && offset<= MAX_DDC_OFFSET){
                double off = offset/MHZ_SCALE;
                SEND_STR(s, DDC_FREQUENCY_CMD(tuner,ddc,off));
            }else{
                std::cout << "Please select a DDC offset between" <<
                    " " << (-MAX_DDC_OFFSET) << "MHz and " <<
                    MAX_DDC_OFFSET << "MHz." << std::endl;
            }
        }
    }
}

void 
polaris_src_impl::set_atten(double atten, int group)
{
    if (m_connected) {
        if (group > 0 && group <= m_num_output_groups) {
            int tuner;
            m_group_data[group - 1].atten = atten;
            tuner = m_group_data[group - 1].tuner;

            if (tuner < 0) 
                return;

            if (atten >= MIN_ATTEN && atten <= MAX_ATTEN) {
                std::stringstream s;
                SEND_STR(s, ATTENUATION_CMD(tuner,atten));
            }else{
                std::cout << "Please select an attenuation" <<
                    " value between " << MIN_ATTEN << "dB " <<
                    " and " << MAX_ATTEN << "dB." << std::endl;
            }
        }
    }
}

void 
polaris_src_impl::set_tuner(int tuner, int group, int num_ddcs)
{
    if (m_connected) {
        if (group > 0 && group <= m_num_output_groups) {
            if (tuner == 1) {
                m_tuners[m_tuner_index] = 1;
                m_tuner_index++;
                if (num_ddcs > 1) {
                    m_tuners[m_tuner_index] = 2;
                    m_tuner_index++;
                }
            }
            if (tuner == 2) {
                m_tuners[m_tuner_index] = 3;
                m_tuner_index++;
                if (num_ddcs > 1) {
                    m_tuners[m_tuner_index] = 4;
                    m_tuner_index++;
                }
            }
            if (tuner == 3) {
                m_tuners[m_tuner_index] = 5;
                m_tuner_index++;
                if (num_ddcs > 1) {
                    m_tuners[m_tuner_index] = 6;
                    m_tuner_index++;
                }
            }
            if (tuner == 4) {
                m_tuners[m_tuner_index] = 7;
                m_tuner_index++;
                if (num_ddcs > 1) {
                    m_tuners[m_tuner_index] = 8;
                    m_tuner_index++;
                }
            }
            m_group_data[group - 1].tuner = tuner;
            m_group_data[group - 1].num_ddcs = num_ddcs;
        }
    }
}

void
polaris_src_impl::set_samp_rate(double sr, int group, int ddc)
{
    std::stringstream s;
    SEND_STR(s, TOGGLE_STREAMING_CMD(1));

    if (m_connected) {
        if (group > 0 && group <= m_num_output_groups) {
            int tuner;
            tuner = m_group_data[group - 1].tuner;
            m_group_data[group-1].ddc_sr[ddc-1]=sr;
            if (m_group_data[group-1].sr_ind[ddc-1] < 0) {
                m_group_data[group-1].sr_ind[ddc-1] = m_sr_index;
                m_sr_index++;
            }
            if (tuner < 0) return;

            sr = sr / MHZ_SCALE;
            if (sr >= MIN_SAMP_RATE_MHZ && sr <= MAX_SAMP_RATE_MHZ) {
                SEND_STR(s, SAMPLE_RATE_CMD(tuner, ddc, sr));
            }else{
                std::cout << "Please select a sample rate " <<
                    "between " << MIN_SAMP_RATE_MHZ << "MHz"<<
                    " and " << MAX_SAMP_RATE_MHZ << "MHz."  <<
                    std::endl;
            }
        }
    }

    SEND_STR(s, TOGGLE_STREAMING_CMD(0));
}

void 
polaris_src_impl::update_preamp(bool pam, int group)
{
    if (group > 0 && group <= m_num_output_groups) {
        m_group_data[group - 1].preamp = pam;
        int tuner = m_group_data[group - 1].tuner;
        if (tuner > 0) {
            std::stringstream s;
            SEND_STR(s, PREAMP_CMD(tuner, (pam ? 1 : 0)));
        }
    }
}

void 
polaris_src_impl::update_groups(int group, int tuner, int num_ddcs)
{
    if (group > m_num_output_groups) {
        tuner = -1;
    }

    if (tuner > 0 && group <= m_num_output_groups) {
        for (int i = 0; i < m_num_output_groups; i++) {
            if (tuner == m_group_data[i].tuner && (i + 1) != group) {
                std::cout << "No two tuners may match. "
                             "Another tuner is already set to "
                             << tuner << std::endl;
                m_group_data[i].active = false;
                m_tuners[i] = -1;
                return;
            }else{
                m_group_data[i].active = true;
            }
        }      
        set_tuner(tuner, group, num_ddcs);
    }
    for (int i = 1; i <= NUM_TUNERS; i++) {
        for (int j = 0; j < m_num_output_groups; j++) {
            if (m_group_data[j].tuner == i) {
                update_atten(
                    m_group_data[j].atten, group);
                update_tuner_freq(
                    m_group_data[j].tuner_freq,group,1);
                update_tuner_freq(
                    m_group_data[j].tuner_freq,group,2);
                update_ddc_offset(
                    m_group_data[j].ddc_freq[0],group,1);
                update_ddc_offset(
                    m_group_data[j].ddc_freq[1],group,2);
                update_preamp(
                    m_group_data[j].preamp,group);
            }
        }
    }
    start_active_groups(); 
}

void
polaris_src_impl::start_active_groups()
{
    std::stringstream s;

    // Turn off all streams 
    SEND_STR(s, TOGGLE_STREAMING_CMD(1));
    SEND_STR(s, SHUTDOWN_STREAMING_CMD); 
    for (int i = 0 ; i < m_num_output_groups; i++) {
        if (m_group_data[i].active == true
              && m_group_data[i].tuner > 0) {
              SEND_STR(s, DATA_STREAM_CMD(m_group_data[i].tuner,1,1));
              if (m_group_data[i].num_ddcs > 1) {
                  SEND_STR(s, DATA_STREAM_CMD(m_group_data[i].tuner, 
                                              2, 1));
              }
        }   
    }

    if (m_complex_manager) {
        m_complex_manager->update_tuners(m_tuners.get(),
                                         m_num_output_streams);
    }
    SEND_STR(s, TOGGLE_STREAMING_CMD(0));
}

void 
polaris_src_impl::update_tuner_freq(double freq, int group, int ddc)
{
    set_tuner_freq(freq, group, ddc);
}

void
polaris_src_impl::update_ddc_offset(double offset, int group, int ddc)
{
    set_ddc_offset(offset, group, ddc);
}

void 
polaris_src_impl::update_samp_rate(double sr, int group, int ddc)
{
    set_samp_rate(sr, group, ddc);
}

void 
polaris_src_impl::update_atten(double atten, int group)
{
    set_atten(atten, group);
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
        for (int i = 0; i < m_num_output_streams; i++) {
            m_sample_rates[i] = noutput_items; 
        }

        // Request the ComplexManager fill our buffers
        m_complex_manager->fill_buffers(
             output_items, m_tuners.get(), m_sample_rates);
        // Return how many values we were able to get for GNURadio.
        // Call 1 produce per stream         
        for (int i = 0; i < m_num_output_streams; i++) {
             produce(i, m_sample_rates[i]);
        }
        return WORK_CALLED_PRODUCE;    
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
void 
polaris_src_impl::setup_polaris()
{
    if (!m_connected) {
        return;
    }
    std::cout << "Setting up POLARIS..." << std::endl;
    /*
    This loop will setup the requested networking, 
    disabled all output streams, and set the radio 
    to independent operation if requested. 
    */
    std::stringstream s;
    SEND_STR(s, CONFIG_MODE_CMD(1));
    for (int i = 1; i <= NUM_TUNERS; i++) {
        for (int j = 1; j <= DDC_PER_TUNER; j++) {
            SEND_STR(s, STREAM_SRC_CMD(i, j, m_stream_address,
                         m_rec_port,"FF:FF:FF:FF:FF:FF"));
            SEND_STR(s, STREAM_DEST_CMD(i, j,
                         m_fiber_address,m_rec_port,
                         "FF:FF:FF:FF:FF:FF"));
            SEND_STR(s, DATA_STREAM_CMD(i, 1, 0));
            SEND_STR(s, OUTPUT_PORT_CMD(i, j, m_phys_port));
            if (m_i_op) {
                SEND_STR(s, ENABLE_IOP_CMD);
            }
        }
    }
    SEND_STR(s, CONFIG_MODE_CMD(0));

    std::cout << "Setup Complete" << std::endl;
    return;
}

/**
 *  Sends the message to the already connected POLARIS system.
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
polaris_src_impl::send_message(std::string s,
        int timeout)
{
    s.append("\r\n");
    boost::asio::write(*m_mne_socket,
            boost::asio::buffer(
                    reinterpret_cast<const void*>(s.c_str()),
                    strlen(s.c_str())));

    if (timeout < 0)
        return "";

    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    int check = setsockopt(m_mne_socket->native(), SOL_SOCKET,
    SO_RCVTIMEO,reinterpret_cast<char*>(&tv),sizeof(struct timeval));
    if (check < 0) {
        std::cout << "Error setting socket options."
                << std::endl;
        return "";
    }

    char recv_buff[MAX_RECV_SIZE];

    std::size_t amnt_read = m_mne_socket->read_some(
            boost::asio::buffer(recv_buff, MAX_RECV_SIZE));

    if (amnt_read == 0)
        return "";

    recv_buff[amnt_read + 1] = '\0';
    std::string ret_val(recv_buff);
    return ret_val;
}

} /* namespace polaris */
} /* namespace gr */

