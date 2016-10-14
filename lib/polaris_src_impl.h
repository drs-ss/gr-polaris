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
#include <boost/thread/mutex.hpp>
#include <boost/asio.hpp>
#include "complex_manager.h"
#include "task.h"
#include "flex_fft_manager.h"

/**
 * @def FLEX_RATE_TAG 
 * @brief The string used to identify the flex streams sample 
 *        rate.
 *  
 * FLEX_RATE_TAG defines a string used for identifying the flex 
 * fft stream tag for sample rate. 
 */
#define FLEX_RATE_TAG "flex_rate"

/**
 * @def FLEX_SIZE_TAG 
 * @brief The string used to identify the flex streams fft size.
 *  
 * FLEX_SIZE_TAG defines a string used for identifying the flex 
 * fft stream tag for fft size.
 */
#define FLEX_SIZE_TAG "flex_size"

/**
 * @def FLEX_REF_TAG 
 * @brief The string used to identify the flex streams reference 
 *        level.
 *  
 * FLEX_REF_TAG defines a string used for identifying the flex 
 * fft stream tag for reference level.
 */
#define FLEX_REF_TAG "flex_rlvl"

/**
 * @def FLEX_AVE_TAG 
 * @brief The string used to identify the flex streams 
 *        averaging.
 *  
 * FLEX_AVE_TAG defines a string used for identifying the flex 
 * fft stream tag for averaging. 
 */
#define FLEX_AVE_TAG "flex_nave"

/**
 * @def FLEX_FREQ_TAG 
 * @brief The string used to identify the flex streams 
 *        frequency.
 *  
 * FLEX_FREQ_TAG defines a string used for identifying the flex 
 * fft stream tag for frequency.
 */
#define FLEX_FREQ_TAG "flex_freq"

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
 * @def MAX_FLEX_STREAMS 
 * @brief The maximum number of flex fft streams from this 
 *        block
 *  
 * MAX_FLEX_STREAMS defines the maximum number of flex fft 
 * streams this block can support.  At the time, this we support 
 * at most 4 flex fft streams. 
 */
#define MAX_FLEX_STREAMS 4

/**
 * @def HIGHEST_FLEX_SOURCE 
 * @brief The largest value for a flex source_id. 
 *  
 * HIGHEST_FLEX_SOURCE defines the largest value a flex 
 * source_id can be. 
 */
#define HIGHEST_FLEX_SOURCE 11

/**
 * @def MAX_FLEX_SR 
 * @brief Maximum supported update rate for flex streams. 
 *  
 * MAX_FLEX_SR defines the maximum update rate for flex fft 
 * streams from the Polaris in Hz.
 */
 #define MAX_FLEX_SR 1000.0

 /**
 * @def MIN_FLEX_SR 
 * @brief Minimum supported update rate for flex streams. 
 *  
 * MIN_FLEX_SR defines the minimum update rate for flex fft 
 * streams from the Polaris in Hz.
 */
 #define MIN_FLEX_SR 0.033

/**
 * @def MIN_FLEX_AVE 
 * @brief Minimum amount of averaging for a given flex stream. 
 *  
 * MIN_FLEX_AVE defines the minimum amount of averaging that can 
 * be applied to a flex fft stream. 
 */
 #define MIN_FLEX_AVE 1

/**
 * @def MAX_FLEX_AVE 
 * @brief Maximum amount of averaging for a given flex stream. 
 *  
 * MAX_FLEX_AVE defines the maximum amount of averaging that can
 * be applied to a flex fft stream. 
 */
 #define MAX_FLEX_AVE 1024

/**
 * @def MIN_FLEX_SIZE_ADC 
 * @brief Minimum fft size for a flex stream with an ADC source.
 *  
 * MIN_FLEX_SIZE_ADC defines the minimum fft size for a flex fft 
 * stream with an ADC source. 
 */
#define MIN_FLEX_SIZE_ADC 32

/**
 * @def MAX_FLEX_SIZE_ADC 
 * @brief Maximum fft size for a flex stream with an ADC source.
 *  
 * MAX_FLEX_SIZE_ADC defines the maximum fft size for a flex fft
 * stream with an ADC source. 
 */
#define MAX_FLEX_SIZE_ADC 4096

/**
 * @def MIN_FLEX_SIZE_DDC 
 * @brief Minimum fft size for a flex stream with a DDC source.
 *  
 * MIN_FLEX_SIZE_DDC defines the minimum fft size for a flex fft
 * stream with a DDC source. 
 */
#define MIN_FLEX_SIZE_DDC 64

/**
 * @def MAX_FLEX_SIZE_DDC 
 * @brief Maximum fft size for a flex stream with a DDC source.
 *  
 * MAX_FLEX_SIZE_DDC defines the maximum fft size for a flex fft
 * stream with a DDC source. 
 */
#define MAX_FLEX_SIZE_DDC 8192

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
 * @def FLEX_SAMPLE_RATE_CMD 
 * @brief Mnemonic command to set the sample rate for the flex 
 *        stream.
 *  
 * FLEX_SAMPLE_RATE_CMD prepares a formatted string for a 
 * stringstream object to create.  This mnemonic will set the 
 * flex fft update rate for a specified flex stream. 
 */
#define FLEX_SAMPLE_RATE_CMD(stream,sr) "FXS" << stream << "," << sr\
                                            << ";"

/**
 * @def FLEX_CONFIG_CMD 
 * @brief Mnemonic command to configure a flex stream. 
 *  
 * FLEX_CONFIG_CMD prepares a formatted string for a 
 * stringstream object to create.  This mnemonic will configure 
 * a flex fft processor's source, fft size, and stream_id. 
 */
#define FLEX_CONFIG_CMD(stream,src,srcid,size,strid) "FXC" << \
                                            stream << "," << src << \
                                            "," << srcid << "," << \
                                            size << "," << strid << \
                                            ";"

/**
 * @def FLEX_DECIMATION_CMD 
 * @brief Mnemonic command to set the decimation for a flex 
 *        stream.
 *  
 * FLEX_DECIMATION_CMD prepares a formatted string for a 
 * stringstream object to create.  The mnemonic will configure a 
 * flex fft processor's decimation rate. 
 */
#define FLEX_DECIMATION_CMD(stream,dec) "FXD" << stream << "," << \
                                            dec << ";"

/**
 * @def FLEX_AVERAGES_CMD 
 * @brief Mnemonic command to set the number of averages for a 
 *        flex stream.
 *  
 * FLEX_AVERAGES_CMD prepares a formatted string for a 
 * stringstream object to create.  This mnemonic will configure 
 * a flex fft processor's averaging constant. 
 */
#define FLEX_AVERAGES_CMD(stream,ave) "FXA" << stream << "," << \
                                            ave << ";"

/**
 * @def FLEX_STREAM_CMD 
 * @brief Mnemonic command to enable/disable a given flex 
 *        stream.
 *  
 * FLEX_STREAM_CMD prepares a formatted string for a 
 * stringstream object to create.  This mnemonic will 
 * enable/disable a flex processor's streaming. 
 */
#define FLEX_STREAM_CMD(stream,enable) "FXE" << stream << "," << \
                                            enable << ";"

/**
 * @def FLEX_LOAD_QRY 
 * @brief Mnemonic query to check the load in the FPGA when 
 *        working with flex fft streams.
 *  
 * FLEX_LOAD_QRY prepares a formatted string for a stringstream 
 * object to create.  The mnemonic will return the current load 
 * that the FPGA is under (this value is updated once per 
 * second). 
 */
 #define FLEX_LOAD_QRY                 "FXL?;"

/**
 * @def FLEX_DEST_CMD 
 * @brief Mnemonic command to set destination address for flex 
 *        stream.
 *  
 * FLEX_DEST_CMD prepares a formatted string for a stringstream 
 * object to create.  The mnemonic will set the networking 
 * destination address for the flex fft stream. 
 */
#define FLEX_DEST_CMD(stream,addr,port,mac) "FXI" << stream << "," \
                                            << addr << "," << port \
                                            << "," << mac << ";"

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
 * @def SHUTDOWN_FLEX_CMD 
 * @brief Mnemonic command to set disable all flex streams.
 *  
 *  SHUTDOWN_FLEX_CMD a prepared string of mnemonics which turns
 *  off all flex data streams.
 */
#define SHUTDOWN_FLEX_CMD   "RCH0;FXE1,0;FXE2,0;FXE3,0;FXE4,0"

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

    struct flex_stream_struct{
        // Stream_id (1-4) maps to output "tabs"
        int stream_id;
        // Source_id (0-11) maps to a flex source
        int source_id;
        bool disable_complex;
        double sample_rate;
        int averaging;
        int fft_size;
        int enabled;

        std::string get_config_string(){
            std::string source = "";
            int translated_src_id = 0;
            if ((source_id % 3 == 0)) {
                source = "ADC";
                translated_src_id = source_id / 3;
                translated_src_id += 1;
            }else{
                source = "DDC";
                translated_src_id = (source_id) - 
                    (int)(source_id / 3);
            }
            std::stringstream s;
            s << FLEX_CONFIG_CMD(stream_id, source, translated_src_id,
                                 fft_size, stream_id);
            return s.str();
        };

        bool is_adc_stream(){
            return (source_id % 3) == 0;
        };
    };

    // Networking stuff
    volatile int m_socket;
    struct sockaddr_in m_server;
    boost::mutex m_network_mutex;

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
    // Port to recv flex packets on
    int m_flex_port;
    // Which physcial port should the polaris use to stream data
    int m_phys_port;
    // How many tuners have at least 1 active DDC
    int m_num_output_groups;
    // Pointer to the complex manager that processes our data for us.
    boost::scoped_ptr<complex_manager> m_complex_manager;
    // Pointer to the flex_fft_manager that handles flex data for us.
    boost::scoped_ptr<flex_fft_manager> m_flex_manager;
    // Function scheduler for delayed checks
    boost::scoped_ptr<task_impl> m_load_check;

    // Independent operation
    bool m_i_op;
    // Whether a setup problem was detected
    bool m_setup_problem;
    // Number of complex outputs on this block
    int m_num_output_streams;
    // Number of flex stream outputs on this block
    int m_num_flex_streams;
    // Whether the work call needs to check FPGA load
    volatile bool m_check_load;
    // Time that check load flag was set
    boost::posix_time::ptime m_request_time;
    // Whether the block has been started or not
    bool m_started;

    int m_request_amounts[MAX_STREAMS];
    boost::scoped_array<int> m_tuners;

    // Local copies of grouping information
    boost::scoped_array<group_data_struct> m_group_data;
    flex_stream_struct m_flex_stream_data[MAX_FLEX_STREAMS];

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
    bool is_complex_enabled(int tuner, int ddc);
    void schedule_load_check();
    void check_flex_load();
    bool is_power_of_two(int x);
public:
    polaris_src_impl(std::string ip, std::string streamip, 
                     std::string fibip, int port, int mne_port, 
                     int flex_port, int num_outputs, int num_groups, 
                     int num_flex_outputs, bool indpendent_operation, 
                     int phys);
    ~polaris_src_impl();

    // GNURadio callbacks
    /**
     * Update's the preamp for a specific stream (1-4).  Accessable 
     * from python. 
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
     * @param freq - The frequency to tune to.
     * @param group - Which output group to change. 
     * @param ddc - Which DDC to change. 
     */
    void update_tuner_freq(double freq, int group, int ddc);

     /**
     * Update's the frequency offset for this tuners DDC.  Accessable
     * from python. 
     * 
     * @param offset - The offset from the tuner frequency.
     * @param group - Which group we are ofset from. 
     * @param ddc - The ddc number for that tuner. 
     */
    void update_ddc_offset(double offset, int group, int ddc);

    /**
     * Set the samplerate for all streams.  Accessable from python.
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
     * @param atten - The attenuation to apply.
     * @param group - Which group you apply attenuation to. 
     *  
     * @note Setting attenuation disables preamp. 
     */
    void update_atten(double atten, int group);

    /**
     * Set the given flex stream to display flex fft data from a 
     * given source id.  To disable a flex stream, pass -1 as the 
     * source id.  If the id of the flex stream is greater than or 
     * equal to the number of expected streams, this function will 
     * be ignored. 
     * 
     * @param stream_id - The flex stream you are addressing.
     * @param source_id - The source of this flex stream's data.
     * @param disable_complex - Disable complex output for this 
     *                        stream (removes a complex output).
     */
    void update_flex_stream(int stream_id, int source_id,
                            int disable_complex);

    /**
     * Set the update rate for the flex fft data for the requested 
     * stream. 
     * 
     * @param stream_id - The flex stream you are addressing.
     * @param sr - The update rate for the stream.
     */
    void update_flex_rate(int stream_id, double sr);
    
    /**
     * Set the number of averages to perform on this flex streams 
     * data. 
     * 
     * @param stream_id - The flex stream you are addressing.
     * @param ave - Number of averages this flex stream should use.
     */
    void update_flex_ave(int stream_id, int ave);

    /**
     * Set the FFT size for the given flex stream.
     * 
     * @param stream_id - The flex stream you are addressing.
     * @param size - The FFT size for the stream.
     */
    void update_flex_size(int stream_id, int size);

    // gr_block extended functions
    bool check_topology(int ninputs, int noutputs);
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

