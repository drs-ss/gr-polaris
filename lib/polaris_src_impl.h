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

/** @file polaris_src_impl.h
 *  @brief Contains the polaris_src_impl object.
 *  
 *  Contains the polaris_src_impl class which holds all of the
 *  frontend logic and radio interface.
 */

#ifndef INCLUDED_POLARIS_POLSRC_IMPL_H
#define INCLUDED_POLARIS_POLSRC_IMPL_H

#include <polaris/polaris_src.h>
#include <unistd.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <boost/thread.hpp>
#include <boost/scoped_array.hpp>
#include <boost/asio.hpp>
#include "complex_manager.h"

/**
 * @def MAX_RECV_SIZE
 * @brief The maximum amount of data that can be recieved back
 * from a mnemonic.
 *
 * MAX_RECV_SIZE is the maximum size (in bytes) of a response
 * back from the radio.
 */
#define MAX_RECV_SIZE 1024

/**
 * @def MAX_STREAMS 
 * @brief The maximum number of output streams from this block 
 *  
 * MAX_STREAMS defines the maximum number of data streams this 
 * block can support.  It is expected to be 
 * NUM_TUNERS * DDC_PER_TUNER. 
 */
#define MAX_STREAMS 8

/**
 * @def NUM_TUNERS 
 * @brief The number of tuners attached to the Polaris unit.
 *  
 * NUM_TUNERS defines how many tuner boards are present under 
 * the digital module. 
 */
#define NUM_TUNERS 4

/**
 * @def DDC_PER_TUNER 
 * @brief The number of DDC per tuner board.
 *  
 * DDC_PER_TUNER defines the number of DDC available on each 
 * tuner board. 
 */
#define DDC_PER_TUNER 2

/**
 * @def MIN_FREQ_MHZ 
 * @brief The lowest frequency a tuner can hit.
 *  
 * MIN_FREQ_MHZ defines the lowest frequency a tuner can tune 
 * to (in MHz). 
 */
#define MIN_FREQ_MHZ 2.0

/**
 * @def MAX_FREQ_MHZ 
 * @brief The highest frequency a tuner can hit.
 *  
 * MAX_FREQ_MHZ defines the highest frequency a tuner can tune 
 * to (in MHz). 
 */
#define MAX_FREQ_MHZ 6200.0

/**
 * @def MHZ_SCALE 
 * @brief Scale to convert Hz to MHz.
 *  
 * MHZ_SCALE is defined as 1,000,000 for converting from Hz to 
 * MHz. 
 */
#define MHZ_SCALE 1000000.0

/**
 * @def MIN_ATTEN 
 * @brief The minimum amount of attenuation a tuner supports.
 *  
 * MIN_ATTEN defines the smallest amount of attenuation that can 
 * be put into a tuner. 
 */
#define MIN_ATTEN 0

/**
 * @def MAX_ATTEN 
 * @brief The maximum amount of attenuation a tuner supports.
 *  
 * MAX_ATTEN defines the largest amount of attenuation that can 
 * be put into a tuner. 
 */
#define MAX_ATTEN 46

/**
 * @def MIN_SAMP_RATE_MHZ 
 * @brief The lowest sample rate the Polaris supports 
 *  
 * MIN_SAMP_RATE_MHZ defines the lowest sample rate the Polaris 
 * supports (in MHz). 
 */
#define MIN_SAMP_RATE_MHZ 0.000977

/**
 * @def MAX_SAMP_RATE_MHZ 
 * @brief The highest sample rate the Polaris supports
 *  
 * MAX_SAMP_RATE_MHZ defines the highest sample rate the Polaris 
 * support (in MHz). 
 */
#define MAX_SAMP_RATE_MHZ 128.0

/**
 * @def MAX_DDC_OFFSET 
 * @brief Max offset (in Hz) a DDC can provide. 
 *  
 * MAX_DDC_OFFSET defines the maximum distance a DDC can 
 * be tuned away from the center frequency of a tuner. 
 */
#define MAX_DDC_OFFSET 64000000

/**
 * @def TOGGLE_STREAMING_CMD 
 * @brief Mnemonic command to enable/disable the data streams. 
 *  
 * TOGGLE_STREAMING_CMD prepares a formatted string for a 
 * stringstream object to create.  This mnemonic will turn on or 
 * off the Vita49 streams coming out of the radio.  A value of 1 
 * will stop streams while a value of 0 will start them. 
 */
#define TOGGLE_STREAMING_CMD(b)           "SYN" << b

/** 
 * @def TUNER_FREQUENCY_CMD 
 * @brief Mnemonic command to change the radio's frequency.
 *  
 * TUNER_FREQUENCY_CMD prepares a formatted string for a 
 * stringstream object to create. This mnemonic will tune the 
 * tuner and ddc to get the selected frequency. 
*/
#define TUNER_FREQUENCY_CMD(tuner,ddc,frq)  "FRT" << tuner << "," <<\
                                ddc << "," << frq << ";" 

/** 
 * @def DDC_FREQUENCY_CMD 
 * @brief Mnemonic command to change the DDC's offset.
 *  
 * DDC_FREQUENCY_CMD prepares a formatted string for a 
 * stringstream object to create. This mnemonic will tune the 
 * DDC for the tuner to the requested offset. 
*/
#define DDC_FREQUENCY_CMD(tuner,ddc,off)  "FRD" << tuner << "," << \
                                ddc << "," << off << ";" 

/** 
 * @def SAMPLE_RATE_CMD 
 * @brief Mnemonic command to change the radio's sample rate.
 *  
 * SAMPLE_RATE_CMD prepares a formatted string for a 
 * stringstream object to create. This mnemonic will change the 
 * sample rate for the associated tuner/ddc combo. 
*/
#define SAMPLE_RATE_CMD(tuner,ddc,spr)  "SPR" << tuner << "," << ddc\
                                << "," << spr << ";"

/** 
 * @def DATA_STREAM_CMD 
 * @brief Mnemonic command to enable data streams.
 *  
 * DATA_STREAM_CMD prepares a formatted string for a 
 * stringstream object to create. This mnemonic will enable the 
 * Vita49 data stream for the associated tuner/ddc. 
*/
#define DATA_STREAM_CMD(tuner,ddc,ste)  "STE" << tuner << "," << ddc\
                                << "," << ste << ";"

/** 
 * @def ATTENUATION_CMD 
 * @brief Mnemonic command to change the radio's attenuation.
 *  
 * ATTENUATION_CMD prepares a formatted string for a 
 * stringstream object to create. This mnemonic will change the 
 * attenuation level for the specified tuner. 
*/
#define ATTENUATION_CMD(tuner,atten)    "RCH" << tuner << ";ATN" << \
                                atten << ";RCH0;"

/** 
 * @def PREAMP_CMD 
 * @brief Mnemonic command to enabled the radio's preamp.
 *  
 * PREAMP_CMD prepares a formatted string for a stringstream 
 * object to create. This mnemonic will turn on or off the 
 * preamp for the specified tuner. 
*/
#define PREAMP_CMD(tuner,pam)      "RCH" << tuner << ";PAM" << \
                                pam << ";RCH0;"

/** 
 * @def OUTPUT_PORT_CMD 
 * @brief Mnemonic command to change the 10 gig port for 
 *        streaming.
 *  
 * OUTPUT_PORT_CMD prepares a formatted string for a 
 * stringstream object to create. This mnemonic will allow you 
 * to select which 10 gig port the tuner/ddc combo will stream 
 * out. 
*/
#define OUTPUT_PORT_CMD(tuner,ddc,port) "STO" << tuner << "," << ddc\
                                << "," << port << ";"

/** 
 * @def CONFIG_MODE_CMD 
 * @brief Mnemonic command to enable configuration mode.
 *  
 * CONFIG_MODE_CMD prepares a formatted string for a 
 * stringstream object to create. This mnemonic can 
 * enable/disable configuration mode for the radio. 
*/
#define CONFIG_MODE_CMD(cfg)            "CFG" << cfg << ";"

/** 
 * @def STREAM_SRC_CMD 
 * @brief Mnemonic command to change the radio's 10gig ip 
 *        address.
 *  
 * STREAM_SRC_CMD prepares a formatted string for a stringstream 
 * object to create. This mnemonic will set the ip address of 
 * the specified tuner/ddc for streaming. 
*/
#define STREAM_SRC_CMD(tuner,ddc,addr,port,mac)  "#UDP" << tuner << \
                                          "," << ddc << "," <<\
                                          addr << "," << port \
                                          << "," << mac << ";"

/** 
 * @def STREAM_DEST_CMD 
 * @brief Mnemonic command to change the radio's stream 
 *        destination.
 *  
 * STREAM_DEST_CMD prepares a formatted string for a 
 * stringstream object to create.  This mnemonic will set the 
 * destination for the Vita49 data stream for the specified 
 * tuner/ddc. 
*/
#define STREAM_DEST_CMD(tuner,ddc,addr,port,mac)  "SIP" << tuner << \
                                          "," << ddc << "," <<\
                                          addr << "," << port \
                                          << "," << mac << ";"

/** 
 * @def ENABLE_IOP_CMD 
 * @brief Mnemonic command to change the radio to independent 
 *        operation mode.
 *  
 * ENABLE_IOP_CMD a prepared string of mnemonics which set the 
 * radio to independent operation mode. 
*/
#define ENABLE_IOP_CMD              "RCH1;DFM2;RCH2;DFM2;RCH3;DFM2"\
                                ";RCH4;DFM2;RCH0"

/** 
 * @def SHUTDOWN_STREAMING_CMD 
 * @brief Mnemonic command to set all streams to be off.
 *  
 * SHUTDOWN_STREAMING_CMD a prepared string of mnemonics which 
 * turns off all data streams. 
*/ 
#define SHUTDOWN_STREAMING_CMD      "RCH0;STE1,1,0;STE2,1,0;STE3,1"\
                                ",0;STE4,1,0;STE1,2,0;STE2,2,0;STE"\
                                "3,2,0;STE4,2,0;CFG0"

/**
 * @def SEND_STR
 * @brief Formats a string to be sent to the radio.
 *
 * SEND_STR prepares a command to be sent to the radio by
 * concatenating string together and shipping them out in one
 * stringstream.
 */
#define SEND_STR(s,a) {s.str("");s<<a;send_message(s.str());\
                                        s.str("");}

namespace gr
{
namespace polaris
{

class polaris_src_impl : public polaris_src
{
private:
    struct group_data_struct{
        int num_ddcs;
        double ddc_freq[DDC_PER_TUNER];
        double ddc_sr[DDC_PER_TUNER];
        int tuner_freq;
        double atten;
        int tuner;
        bool preamp;
        bool active;
        int sr_ind[DDC_PER_TUNER];
    };

    // Networking stuff
    boost::shared_ptr<boost::asio::ip::tcp::socket> m_mne_socket;
    boost::asio::io_service m_mne_io_service;

    // Flag for whether or not we have a connection to the unit.
    bool m_connected;
    // The address of the unit
    std::string m_address;
    // The address of the unit's 10Gig connection
    std::string m_stream_address;
    // The address of the target machine's 10Gig
    std::string m_fiber_address;
    // Port to recv data on
    int m_rec_port;
    // Port to connect to mne_app
    int m_mne_port;
    // Which physcial port should the polaris use to stream data
    int m_phys_port;
    // How many tuners have at least 1 active DDC
    int m_num_output_groups;
    // Pointer to the complex manager that processes our data for us.
    boost::scoped_ptr<complex_manager> m_complex_manager;
    // Independent operation
    bool m_i_op;
    // Number of outputs on this block
    int m_num_output_streams;

    int m_sample_rates[MAX_STREAMS];
    boost::scoped_array<int> m_tuners;

    boost::scoped_array<group_data_struct> m_group_data;

    int m_sr_index;
    int m_tuner_index;

    std::string send_message(std::string s, int timeout = -1);
    void set_tuner_freq(double freq, int group, int ddc);
    void set_ddc_offset(double offset, int group, int ddc);
    void set_atten(double atten, int group);
    void set_tuner(int tuner, int group, int num_ddcs);
    void set_samp_rate(double rate, int group, int ddc);
    void try_connect();
    void setup_polaris();
public:
    polaris_src_impl(std::string ip, std::string streamip, 
                     std::string fibip, int port, int mne_port, 
                     int num_outputs, int num_groups, 
                     bool indpendent_operation, int phys);
    ~polaris_src_impl();

    // GNURadio callbacks
    /**
     * Update's the preamp for a specific stream (1-4).  Accessable 
     * from python. 
     * 
     * 
     * @param pam - Whether to turn the preamp on or off.
     * @param group - group number of the DDC. 
     *  
     * @note Enabling the preamp disables attenuation. 
     */
    void update_preamp(bool pam, int group);

    /**
     * Update's a streams output to display the specified tuner's 
     * data. Accessable from python.
     * 
     * @param group - group number (1-4)
     * @param tuner - Which tuner you would like data from.
     * @param num_ddcs - number of ddcs in this group
     */
    void update_groups(int group, int tuner, int num_ddcs);

    /**
     * Starts active grups by looking at the group_data_struct's 
     * members.  
     * 
     */
    void start_active_groups();

    /**
     * Update's the frequency for this stream's tuner.  Accessable 
     * from python. 
     * 
     * 
     * @param freq - The frequency to tune to.
     * @param group - Which output group to change. 
     * @param ddc - Which DDC to change. 
     */
    void update_tuner_freq(double freq, int group, int ddc);

     /**
     * Update's the frequency offset for this tuners DDC.  Accessable
     * from python. 
     * 
     * 
     * @param offset - The offset from the tuner frequency.
     * @param group - Which group we are ofset from. 
     * @param ddc - The ddc number for that tuner. 
     */
    void update_ddc_offset(double offset, int group, int ddc);

    /**
     * Set the samplerate for all streams.  Accessable from python.
     * 
     * 
     * @param sr - Sample rate to set. 
     * @param group - group number of the DDC. 
     * @param ddc - ddc number we are updataing. 
     */
    void update_samp_rate(double sr, int group, int ddc);

    /**
     * Set the attenuation for this stream's tuner.  Accessable from 
     * python. 
     * 
     * 
     * @param atten - The attenuation to apply.
     * @param group - Which group you apply attenuation to. 
     *  
     * @note Setting attenuation disables preamp. 
     */
    void update_atten(double atten, int group);

    // gr_block extended functions
    void forecast(int noutput_items, gr_vector_int 
                  &ninput_items_required);
    bool start();
    bool stop();
    int work(int noutput_items, gr_vector_const_void_star 
             &input_items, gr_vector_void_star &output_items);
};

} // namespace polaris
} // namespace gr

#endif /* INCLUDED_POLARIS_POLSRC_IMPL_H */

