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
                  int flex_port, std::string streamip,
                  std::string fibip, int num_outputs, int num_groups,
                  int num_flex_outputs, bool i_op, int phys)
{
    return gnuradio::get_initial_sptr
           (new polaris_src_impl(ip, streamip, fibip, port, mne_port,
                                 flex_port, num_outputs, num_groups,
                                 num_flex_outputs, i_op, phys));
}

/**
 * The private constructor
 */
polaris_src_impl::polaris_src_impl(std::string ip,
                                   std::string streamip,
                                   std::string fibip,
                                   int port,
                                   int mne_port,
                                   int flex_port,
                                   int num_outputs,
                                   int num_groups,
                                   int num_flex_outputs,
                                   bool independent_operation,
                                   int phys)
    : gr::sync_block("POLSRC",
                     gr::io_signature::make(0, 0, 0),
                     gr::io_signature::make(0,
                                            2,
                                            sizeof(gr_complex)))
{
    // Fix our io_signature for output.
    std::vector<int> output_types;
    if (num_outputs < 0) {
        num_outputs = 0;
    }
    int total_out = num_outputs + num_flex_outputs;
    if (total_out == 0) {
        std::cout << "=====================================" <<
            std::endl;
        std::cout << "ERROR:" << std::endl;
        std::cout << "Please ensure that you have at least" <<
            " one valid data stream enabled." << std::endl;
        std::cout << "Be sure you have either at least 1 DDC in" <<
            " group 1 or a flex fft adc stream enabled." << std::endl;
        std::cout << "=====================================" <<
            std::endl;
    }
    for (int i = 0; i < total_out; i++) {
        if (i < num_outputs) {
            output_types.push_back(sizeof(gr_complex));
        }else{
            output_types.push_back(sizeof(float));
        }
    }
    set_output_signature(gr::io_signature::makev(0,
                                                 total_out,
                                                 output_types));
    m_i_op = independent_operation;
    m_num_output_streams = num_outputs;
    if (m_num_output_streams < 0) {
        m_num_output_streams = 0;
    }
    m_num_output_groups = num_groups;
    m_num_flex_streams = num_flex_outputs;
    m_address = ip;
    m_stream_address = streamip;
    m_rec_port = port;
    m_mne_port = mne_port;
    m_flex_port = flex_port;
    m_fiber_address = fibip;
    m_check_load = false;
    m_setup_problem = false;
    m_started = false;

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

    if (m_num_flex_streams > MAX_FLEX_STREAMS) {
        m_num_flex_streams = MAX_FLEX_STREAMS;
    }

    for (int i = 0; i < MAX_FLEX_STREAMS; i++) {
        m_flex_stream_data[i].stream_id = i + 1;
        m_flex_stream_data[i].source_id = -1;
        m_flex_stream_data[i].disable_complex = 0;
        m_flex_stream_data[i].sample_rate = MIN_FLEX_SR;
        m_flex_stream_data[i].averaging = MIN_FLEX_AVE;
        m_flex_stream_data[i].fft_size = MIN_FLEX_SIZE_DDC;
        if (num_flex_outputs > i) {
            m_flex_stream_data[i].enabled = 1;
        } else {
            m_flex_stream_data[i].enabled = 0;
        }
    }

    for (int i = 0; i < MAX_STREAMS; i++) {
        m_tuners[i] = -1;
        m_request_amounts[i] = 0;
    }

    m_phys_port = phys;
    if (m_phys_port < 0 || m_phys_port > 1) {
        std::cout << "Setting physical output port to right"
            << std::endl;
        m_phys_port = 0;
    }

    m_sr_index = 0;
    m_tuner_index = 0;

    // Setup the Polaris...
    try_connect();
    if (m_connected) {
        setup_polaris();
    }

    m_load_check.reset(new task_impl(
        boost::bind(&polaris_src_impl::check_flex_load, this)));
}

/**
 * Our virtual destructor.
 */
polaris_src_impl::~polaris_src_impl()
{
}

bool
polaris_src_impl::check_topology(int ninputs, int noutputs)
{
    return true;
}

bool
polaris_src_impl::start()
{
    try {
        if (m_connected) {
            std::stringstream s;
            // Before we boot anything else up, let's do some 
            // sanity checks.
            // If they are using flex streams
            bool found_control = false;
            bool streams_match = false;
            if (m_num_flex_streams > 0) {
                for (int i = 0; i < m_num_flex_streams; i++) {
                    int source_id = m_flex_stream_data[i].source_id;
                    for (int j = 0; j < m_num_flex_streams; j++) {
                        int temp_source_id = 
                            m_flex_stream_data[j].source_id;
                        if (temp_source_id == source_id && i != j) {
                            streams_match = true;
                            std::cout << std::endl << std::endl;
                            std::cout << "ERROR:" << std::endl;
                            std::cout << "Flex streams " << 
                                (i + 1) << " and " << (j + 1) <<
                                " source's match." << std::endl;
                            std::cout << "Please select unique "
                                << "source id's." << std::endl;
                            break;
                        }
                    }
                }
                for (int i = 0; i < m_num_flex_streams; i++) {
                    flex_stream_struct &temp_flex = 
                        m_flex_stream_data[i];
                    int flex_tuner = temp_flex.source_id / 3;
                    flex_tuner += 1;
                    int flex_ddc = temp_flex.source_id % 3;
                    found_control = false;
                    for (int j = 0; j < m_num_output_groups; j++) {
                        group_data_struct &temp_group =
                            m_group_data[j];
                        if (flex_tuner == temp_group.tuner) {
                            if (temp_flex.is_adc_stream()) {
                                found_control = true;
                                break;
                            }else if(flex_ddc == 1 &&
                                     temp_group.num_ddcs > 0){
                                found_control = true;
                                break;
                            }else if(flex_ddc == 2 && 
                                     temp_group.num_ddcs > 1){
                                found_control = true;
                                break;
                            }
                        }
                    }
                    if (!found_control) {
                        std::cout << std::endl << std::endl << 
                            "ERROR: " << std::endl;
                        std::cout << "Missing RF control for flex" <<
                            " stream " << (i + 1) << " using RF " <<
                            "source of [Tuner " << flex_tuner << 
                            "] ";
                        if (temp_flex.is_adc_stream()) {
                            std::cout << "ADC." << std::endl;
                        }else{
                            std::cout << "DDC " << flex_ddc << "." <<
                                std::endl;
                        }
                        std::cout << "Please make sure a group is " <<
                            "setup to use that tuner";
                        if (temp_flex.is_adc_stream()) {
                            std::cout << "." << std::endl;
                        }else{
                            std::cout << " and has enough DDC " <<
                                "outputs assigned to it." <<
                                std::endl;
                        }
                        break;
                    }
                }
                if (!found_control) {
                    m_setup_problem = true;
                    return false;
                }
                if (streams_match) {
                    m_setup_problem = true;
                    return false;
                }
            }

            if (m_num_output_streams > 0) {
                m_complex_manager.reset(
                    new complex_manager(m_fiber_address, m_rec_port));
                m_complex_manager->update_tuners(m_tuners.get(),
                                                m_num_output_streams);
            }

            if (m_num_flex_streams > 0) {
                m_flex_manager.reset(
                    new flex_fft_manager(
                        m_fiber_address,m_flex_port));
                for (int i = 0; i < m_num_flex_streams; i++) {
                    if (m_flex_stream_data[i].enabled) {
                        m_flex_manager->add_stream(
                            m_flex_stream_data[i].stream_id);
                        SEND_STR(s, 
                                 FLEX_STREAM_CMD(
                                     m_flex_stream_data[i].stream_id, 
                                     1));
                    }
                }
                schedule_load_check();
            }

            // Enable data streams.
            SEND_STR(s, TOGGLE_STREAMING_CMD(0));
        }
    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
    }
    m_started = true;
    return true;
}

bool
polaris_src_impl::stop()
{
    // Tell the POLARIS to stop transmitting
    std::stringstream s;
    SEND_STR(s, SHUTDOWN_STREAMING_CMD);
    SEND_STR(s, SHUTDOWN_FLEX_CMD);
    if (m_complex_manager) {
        m_complex_manager->stop();
    }
    m_started = false;
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
            m_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (m_socket < 0) {
                std::cout << "TCP Connection Failed" << std::endl;
                return;
            }
            m_server.sin_addr.s_addr = inet_addr(m_address.c_str());
            m_server.sin_family = AF_INET;
            m_server.sin_port = htons(m_mne_port);
            if (connect(m_socket, 
                        (struct sockaddr *)&m_server, 
                        sizeof(m_server)) < 0) {
                std::cout << "TCP Connection Failed" << std::endl;
                return;
            }
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
    if (!m_i_op) {
        SEND_STR(s, TOGGLE_STREAMING_CMD(1));
    }
    if (m_connected) {
        if (group > 0 && group <= m_num_output_groups) {
            int tuner;
            m_group_data[group - 1].tuner_freq = freq;
            tuner = m_group_data[group - 1].tuner;

            if (tuner < 0) return;

            if (freq) {
                freq = freq / MHZ_SCALE;
                if (freq >= MIN_FREQ_MHZ && freq <= MAX_FREQ_MHZ) {
                    std::stringstream s;
                    s << std::fixed;
                    SEND_STR(s, TUNER_FREQUENCY_CMD(tuner,ddc,freq));
                } else {
                    std::cout << "Please select a frequency between"<<
                        " " << MIN_FREQ_MHZ << "MHz and " <<
                        MAX_FREQ_MHZ << "MHz." << std::endl;
                }
            }
        }
    }
    if (!m_i_op) {
        SEND_STR(s, TOGGLE_STREAMING_CMD(0));
    }
}

void
polaris_src_impl::set_ddc_offset(double offset, int group, int ddc)
{
    std::stringstream s;
    if (m_connected) {
        if (group > 0 && group <= m_num_output_groups) {
            int tuner;
            tuner = m_group_data[group - 1].tuner;

            if (tuner < 0) return;

            std::ostringstream s;
            if (offset >= -MAX_DDC_OFFSET && offset <= MAX_DDC_OFFSET) {
                double off = offset / MHZ_SCALE;
                s << std::fixed;
                SEND_STR(s, DDC_FREQUENCY_CMD(tuner, ddc, off));
            } else {
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

            if (tuner < 0) return;

            if (atten >= MIN_ATTEN && atten <= MAX_ATTEN) {
                std::stringstream s;
                SEND_STR(s, ATTENUATION_CMD(tuner, atten));
            } else {
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
            if (tuner == 1 && num_ddcs > 0) {
                m_tuners[m_tuner_index] = 1;
                m_tuner_index++;
                if (num_ddcs > 1) {
                    m_tuners[m_tuner_index] = 2;
                    m_tuner_index++;
                }
            }
            if (tuner == 2 && num_ddcs > 0) {
                m_tuners[m_tuner_index] = 3;
                m_tuner_index++;
                if (num_ddcs > 1) {
                    m_tuners[m_tuner_index] = 4;
                    m_tuner_index++;
                }
            }
            if (tuner == 3 && num_ddcs > 0) {
                m_tuners[m_tuner_index] = 5;
                m_tuner_index++;
                if (num_ddcs > 1) {
                    m_tuners[m_tuner_index] = 6;
                    m_tuner_index++;
                }
            }
            if (tuner == 4 && num_ddcs > 0) {
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
            m_group_data[group - 1].ddc_sr[ddc - 1] = sr;
            if (m_group_data[group - 1].sr_ind[ddc - 1] < 0) {
                m_group_data[group - 1].sr_ind[ddc - 1] = m_sr_index;
                m_sr_index++;
            }
            if (tuner < 0) return;

            sr = sr / MHZ_SCALE;
            if (sr >= MIN_SAMP_RATE_MHZ && sr <= MAX_SAMP_RATE_MHZ) {
                s << std::fixed;
                SEND_STR(s, SAMPLE_RATE_CMD(tuner, ddc, sr));
            } else {
                std::cout << "Please select a sample rate " <<
                    "between " << MIN_SAMP_RATE_MHZ << "MHz" <<
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
    if (tuner > 0 && group >= 1 && group <= m_num_output_groups) {
        for (int i = 0; i < m_num_output_groups; i++) {
            if (tuner == m_group_data[i].tuner && (i + 1) != group) {
                std::cout << "No two tuners may match. "
                    "Another tuner is already set to "
                    << tuner << std::endl;
                m_group_data[i].active = false;
                m_tuners[i] = -1;
                return;
            }
        }
        if (num_ddcs > 0) {
            m_group_data[group - 1].active = true;
        }else{
            m_group_data[group - 1].active = false;
        }
        set_tuner(tuner, group, num_ddcs);
        // Now that we've set it's active state, we need to 
        // check if this streams complex output was expected
        // to be disabled (via a flex_stream setting).
        for (int i = 0; i < MAX_FLEX_STREAMS; i++) {
            int flex_tuner = m_flex_stream_data[i].source_id / 3;
            flex_tuner += 1;
            if (tuner == flex_tuner && num_ddcs > 0 &&
                m_flex_stream_data[i].disable_complex) {
                m_tuner_index -= 1;
                if (num_ddcs == 1) {
                    break;
                }
            }
        }
    }
    for (int i = 1; i <= NUM_TUNERS; i++) {
        for (int j = 0; j < m_num_output_groups; j++) {
            if (m_group_data[j].tuner == i) {
                update_atten(
                    m_group_data[j].atten, group);
                update_tuner_freq(
                    m_group_data[j].tuner_freq, group, 1);
                update_tuner_freq(
                    m_group_data[j].tuner_freq, group, 2);
                update_ddc_offset(
                    m_group_data[j].ddc_freq[0], group, 1);
                update_ddc_offset(
                    m_group_data[j].ddc_freq[1], group, 2);
                update_preamp(
                    m_group_data[j].preamp, group);
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
    for (int i = 0; i < m_num_output_groups; i++) {
        if (m_group_data[i].active == true
            && m_group_data[i].tuner > 0) {
            if (is_complex_enabled(m_group_data[i].tuner, 1)) {
                SEND_STR(s, DATA_STREAM_CMD(m_group_data[i].tuner,
                                            1, 1));
            }
            if (m_group_data[i].num_ddcs > 1) {
                if (is_complex_enabled(m_group_data[i].tuner, 2)) {
                    SEND_STR(s, DATA_STREAM_CMD(m_group_data[i].tuner,
                                                2, 1));
                }
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
    // If the flex_manager is running, and they are streaming from
    // the ddc in question, we need to check the flex load to warn
    // them of potential missing data.
    if (m_flex_manager.get() != NULL && m_num_flex_streams > 0) {
        if (group > 0 && group <= m_num_output_groups) {
            int tuner = m_group_data[group - 1].tuner;
            for (int i = 0; i < MAX_FLEX_STREAMS; i++) {
                int flex_tuner=(m_flex_stream_data[i].source_id/3)+1;
                int flex_ddc=(m_flex_stream_data[i].source_id%3);
                if (flex_tuner == tuner) {
                    if (flex_ddc == ddc) {
                        schedule_load_check();
                    }
                }
            }
        }
    }
}

void
polaris_src_impl::update_atten(double atten, int group)
{
    set_atten(atten, group);
}

void
polaris_src_impl::update_flex_stream(int stream_id, int source_id,
                                     int disable_complex)
{
    // Sanity checks...
    if (stream_id <= 0 || stream_id > MAX_FLEX_STREAMS) {
        // Invalid input...we expect a range of (1,MAX_FLEX_STREAMS)
        std::cout << "Invalid stream_id to update_flex_stream: "
            << stream_id << std::endl;
        std::cout << "Expected stream_id to be between : 1 and "
            << MAX_FLEX_STREAMS << std::endl;
        return;
    }
    if (stream_id > m_num_flex_streams) {
        // Don't update, this stream isn't active.
        return;
    }
    if (source_id < 0 || source_id > HIGHEST_FLEX_SOURCE) {
        // Invalid input...we expect a range of
        // (0,HIGHEST_FLEX_SOURCE)
        std::cout << "Invalid source_id to update_flex_stream: "
            << source_id << std::endl;
        std::cout << "Expected source_id to be between : 0 and "
            << HIGHEST_FLEX_SOURCE << std::endl;
        return;
    }
    if (disable_complex < 0 || disable_complex > 1) {
        std::cout << "Warning: treating non-zero value as true "
            << "for disable_complex." << std::endl;
        disable_complex = 1;
    }

    // If we got here, we can save this information.
    int stream_index = stream_id - 1;
    flex_stream_struct *stream_struct =
        &m_flex_stream_data[stream_index];
    stream_struct->source_id = source_id;
    if (stream_struct->is_adc_stream()) {
        stream_struct->disable_complex = false;
    }else{
        stream_struct->disable_complex = disable_complex;
    }

    // We can also update the actual stream.
    std::stringstream s;
    SEND_STR(s, FLEX_STREAM_CMD(stream_id, 0));
    // Also just setup the networking stuff here.
    SEND_STR(s, FLEX_DEST_CMD(stream_id,
                              m_fiber_address,
                              m_flex_port,
                              "FF:FF:FF:FF:FF:FF"));
    send_message(stream_struct->get_config_string());
    if (m_started) {
        SEND_STR(s, FLEX_STREAM_CMD(stream_id,
                                    m_flex_stream_data[stream_index].
                                    enabled));
    }

    int tuner = source_id / 3;
    tuner += 1;
    int ddc = source_id % 3;
    if (disable_complex) {
        SEND_STR(s, DATA_STREAM_CMD(tuner, ddc, 0));
    }else{
        SEND_STR(s, DATA_STREAM_CMD(tuner, ddc, 1));
    }
    schedule_load_check();
}

void
polaris_src_impl::update_flex_rate(int stream_id, double sr)
{
    // Sanity checks...
    if (stream_id <= 0 || stream_id > MAX_FLEX_STREAMS) {
        // Invalid input...we expect a range of (1,MAX_FLEX_STREAMS)
        std::cout << "Invalid stream_id to update_flex_rate: "
            << stream_id << std::endl;
        std::cout << "Expected stream_id to be between : 1 and "
            << MAX_FLEX_STREAMS << std::endl;
        return;
    }
    if (stream_id > m_num_flex_streams) {
        // Don't update, this stream isn't active.
        return;
    }
    if (sr < MIN_FLEX_SR || sr > MAX_FLEX_SR) {
        // Invalid input...we expect a range of
        // (MIN_FLEX_SR, MAX_FLEX_SR)
        std::cout << "Invalid update_rate to update_flex_rate: "
            << sr << std::endl;
        std::cout << "Expected update_rate to be between : "
            << MIN_FLEX_SR << " and " << MAX_FLEX_SR << std::endl;
        return;
    }

    // If we got here, save the option.
    int stream_index = stream_id - 1;
    m_flex_stream_data[stream_index].sample_rate = sr;

    // Now update the actual stream.
    std::stringstream s;
    SEND_STR(s, FLEX_STREAM_CMD(stream_id, 0));
    SEND_STR(s, FLEX_SAMPLE_RATE_CMD(stream_id, sr));
    if (m_started) {
        SEND_STR(s,
                 FLEX_STREAM_CMD(stream_id,
                                 m_flex_stream_data[stream_index].
                                 enabled));
    }
    schedule_load_check();
}

void
polaris_src_impl::update_flex_ave(int stream_id, int ave)
{
    // Sanity checks...
    if (stream_id <= 0 || stream_id > MAX_FLEX_STREAMS) {
        // Invalid input...we expect a range of (1,MAX_FLEX_STREAMS)
        std::cout << "Invalid stream_id to update_flex_rate: "
            << stream_id << std::endl;
        std::cout << "Expected stream_id to be between : 1 and "
            << MAX_FLEX_STREAMS << std::endl;
        return;
    }
    if (stream_id > m_num_flex_streams) {
        // Don't update, this stream isn't active.
        return;
    }
    if (ave < MIN_FLEX_AVE || ave > MAX_FLEX_AVE) {
        // Invalid input...we expect a range of
        // (MIN_FLEX_AVE,MAX_FLEX_AVE)
        std::cout << "Invalid averaging constant to update_flex_ave :"
            << ave << std::endl;
        std::cout << "Expected averaging constant to be between : "
            << MIN_FLEX_AVE << " and " << MAX_FLEX_AVE << std::endl;
        return;
    }else if(!is_power_of_two(ave)){
        std::cout << "Invalid averaging constant to update_flex_ave :"
            << ave << std::endl;
        std::cout << "Expected averaging constant to be a power of" <<
            " two." << std::endl;
    }

    // Save the value...
    int stream_index = stream_id - 1;
    m_flex_stream_data[stream_index].averaging = ave;

    // Update the stream...
    std::stringstream s;
    SEND_STR(s, FLEX_AVERAGES_CMD(stream_id, ave));
    schedule_load_check();
}

bool
polaris_src_impl::is_power_of_two(int x)
{
    int num_bits = 0;
    while (x > 0 && num_bits <= 1) {
        if ((x & 1) == 1) {
            num_bits += 1;
        }
        x >>= 1;
    }
    return (num_bits == 1);
}

void
polaris_src_impl::update_flex_size(int stream_id, int size)
{
    // Sanity checks...
    if (stream_id <= 0 || stream_id > MAX_FLEX_STREAMS) {
        // Invalid input...we expect a range of (1,MAX_FLEX_STREAMS)
        std::cout << "Invalid stream_id to update_flex_rate: "
            << stream_id << std::endl;
        std::cout << "Expected stream_id to be between : 1 and "
            << MAX_FLEX_STREAMS << std::endl;
        return;
    }
    if (stream_id > m_num_flex_streams) {
        // Don't update, this stream isn't active.
        return;
    }
    int stream_index = stream_id - 1;
    if (m_flex_stream_data[stream_index].is_adc_stream()) {
        if (size < MIN_FLEX_SIZE_ADC || size > MAX_FLEX_SIZE_ADC) {
            std::cout << "Invalid size to update_flex_size: "
                << size << std::endl;
            std::cout << "Expected fft size to be between : "
                << MIN_FLEX_SIZE_ADC << " and " << MAX_FLEX_SIZE_ADC
                << " for ADC streams." << std::endl;
            return;
        }else if(!is_power_of_two(size)){
            std::cout << "Invalid size to update_flex_size: "
                << size << std::endl;
            std::cout << "Expected fft size to be a power of 2." <<
                std::endl;
        }
    } else {
        if (size < MIN_FLEX_SIZE_DDC || size > MAX_FLEX_SIZE_DDC) {
            std::cout << "Invalid size to update_flex_size: "
                << size << std::endl;
            std::cout << "Expected fft size to be between : "
                << MIN_FLEX_SIZE_ADC << " and " << MAX_FLEX_SIZE_ADC
                << " for DDC streams." << std::endl;
            return;
        }else if(!is_power_of_two(size)){
            std::cout << "Invalid size to update_flex_size: "
                << size << std::endl;
            std::cout << "Expected fft size to be a power of 2." <<
                std::endl;
        }
    }

    // Set our stored value...
    m_flex_stream_data[stream_index].fft_size = size;

    // Update our stream...
    std::stringstream s;
    SEND_STR(s, FLEX_STREAM_CMD(stream_id, 0));
    send_message(
        m_flex_stream_data[stream_index].get_config_string());
    if (m_started) {
        SEND_STR(s,
                 FLEX_STREAM_CMD(stream_id,
                                 m_flex_stream_data[stream_index].
                                 enabled));
    }
    schedule_load_check();
}

void
polaris_src_impl::schedule_load_check()
{
    m_check_load = true;
    m_request_time = boost::posix_time::microsec_clock::local_time();
}

bool
polaris_src_impl::is_complex_enabled(int tuner, int ddc)
{
    for (int i = 0; i < m_num_flex_streams; i++) {
        int flex_tuner = m_flex_stream_data[i].source_id / 3;
        flex_tuner += 1;
        int flex_ddc = m_flex_stream_data[i].source_id % 3;
        if (flex_tuner == tuner && flex_ddc == ddc && 
            m_flex_stream_data[i].enabled) {
            if (m_flex_stream_data[i].disable_complex) {
                return false;
            }
        }
    }
    return true;
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
    if (!m_connected || m_setup_problem) {
        // No TCP connection was established.  Don't do anything.
        return 0;
    } else {
        // Check if they have any active complex streams.
        if (m_num_output_streams > 0) {
            for (int i = 0; i < m_num_output_streams; i++) {
                m_request_amounts[i] = noutput_items;
            }

            // Request the ComplexManager fill our buffers
            m_complex_manager->fill_buffers(
                output_items, m_tuners.get(), m_request_amounts);
            // Return how many values we were able to get for GNURadio
            // Call 1 produce per stream
            for (int i = 0; i < m_num_output_streams; i++) {
                produce(i, m_request_amounts[i]);
            }
        }
        // Check if they have any active flex fft streams.
        if (m_num_flex_streams > 0) {
            std::vector<int> copied_amounts(m_num_flex_streams);
            std::vector<std::vector<flex_fft_manager::stream_change> >
                detected_changes(m_num_flex_streams);
            m_flex_manager->copy_data(
                &output_items[m_num_output_streams], noutput_items,
                                      copied_amounts, detected_changes
                                      );
            for (int i = 0; i < m_num_flex_streams; i++) {
                produce(i + m_num_output_streams, copied_amounts[i]);
            }
            for (int i = 0; i < detected_changes.size(); i++) {
                for (int j = 0; j < detected_changes[i].size(); j++) {
                    flex_fft_manager::stream_change& sc = 
                        detected_changes[i][j];
                    boost::uint64_t num_written = 
                        nitems_written(i + m_num_output_streams);
                    add_item_tag(i + m_num_output_streams, 
                                 num_written + sc.starting_sample,
                                 pmt::string_to_symbol(FLEX_RATE_TAG),
                                 pmt::mp(sc.sample_rate));
                    add_item_tag(i + m_num_output_streams, 
                                 num_written + sc.starting_sample,
                                 pmt::string_to_symbol(FLEX_SIZE_TAG),
                                 pmt::mp(sc.fft_size));
                    add_item_tag(i + m_num_output_streams,
                                 num_written + sc.starting_sample,
                                 pmt::string_to_symbol(FLEX_REF_TAG),
                                 pmt::mp(sc.reference_level));
                    add_item_tag(i + m_num_output_streams, 
                                 num_written + sc.starting_sample,
                                 pmt::string_to_symbol(FLEX_AVE_TAG),
                                 pmt::mp(sc.num_ave));
                    add_item_tag(i + m_num_output_streams, 
                                 num_written + sc.starting_sample,
                                 pmt::string_to_symbol(FLEX_FREQ_TAG),
                                 pmt::mp(sc.frequency));
                }
            }
            // Move check load to another thread for timing's sake.
            if (m_check_load) {
                boost::posix_time::time_duration dur = 
                    boost::posix_time::microsec_clock::local_time()
                    - m_request_time;
                if (dur.total_milliseconds() > 1500) {
                    if (!m_load_check->is_running()) {
                        m_load_check->wake_up_thread();
                    }
                    m_request_time = 
                        boost::posix_time::microsec_clock::local_time();
                }
            }
        }
        return WORK_CALLED_PRODUCE;
    }
}

void
polaris_src_impl::check_flex_load()
{
    std::string flex_response = send_message(FLEX_LOAD_QRY, 1);
    // We expect "FXL ###.##%"
    if (flex_response.empty()) {
        return;
    }
    int chop_index = flex_response.find("FXL ");
    if (chop_index < 0) {
        return;
    }
    chop_index += 4;
    if (chop_index >= flex_response.size()) {
        return;
    }
    if ((chop_index + 6) > flex_response.size()) {
        return;
    }
    flex_response = flex_response.substr(chop_index, 6);
    try {
        float percent = std::atof(flex_response.c_str());
        if (percent == 100.00) {
            std::cout << flex_response << std::endl;
            std::cout << "=========================================="
                << "=============" << std::endl;
            std::cout << "WARNING: FLEX LOAD IS AT 100%.  YOU MAY BE"
                << " MISSING DATA" << std::endl;
            std::cout << "=========================================="
                << "=============" << std::endl;
        }
    } catch (std::exception &e) {
        return;
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
                                       m_rec_port, "FF:FF:FF:FF:FF:FF"
                                       ));
            SEND_STR(s, STREAM_DEST_CMD(i, j,
                                        m_fiber_address, m_rec_port,
                                        "FF:FF:FF:FF:FF:FF"));
            SEND_STR(s, DATA_STREAM_CMD(i, j, 0));
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
    boost::mutex::scoped_lock lck(m_network_mutex);
    if (send(m_socket, s.c_str(), strlen(s.c_str()), 0) < 0) {
        return "";
    }
    if (timeout < 0) return "";

    char buff[256];
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    int check = setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, 
                           (char *)&tv, sizeof(struct timeval));
    if (check < 0) {
        std::cout << "Error setting socket options." << std::endl;
        return "";
    }
    int err = 2000;
    if ((err = recv(m_socket, buff, 256, 0)) < 0) {
        return "";
    }
    buff[err + 1] = '\0';
    std::string retVal(buff);
    return retVal;
}

} /* namespace polaris */
} /* namespace gr */

