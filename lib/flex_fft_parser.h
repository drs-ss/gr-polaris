#include <vector>
#include <boost/scoped_array.hpp>
#include <boost/lockfree/stack.hpp>
#include <boost/circular_buffer.hpp>
#include <iomanip>
#include <netinet/in.h>
#include "task.h"

/**
 * @def MAX_FLEX_RECV_SIZE 
 * @brief The maximum size of a single UDP payload for a flex 
 *        fft stream.
 *  
 * This value is expected to be at least as big as the largest 
 * size a UDP packet's payload can be.  Note: this is the size 
 * of a UDP packets's payload, not the Vita49 frame size. 
 */
#define MAX_FLEX_RECV_SIZE 9000

/**
 * @def BUFFER_SIZE 
 * @brief The number of frames this parser can handle 
 *        simultaneously.
 *  
 *  BUFFER_SIZE is the number of Vita49 packets we can handle
 *  processing at once.  Total buffer size will be BUFFER_SIZE *
 *  MAX_FRAME_SIZE.
 */
#define BUFFER_SIZE 1000

/**
 * @def MAX_FRAME_SIZE 
 * @brief The maximum size of a single Vita49 frame. 
 *  
 * This is the largest size of a Vita49 frame (even if split 
 * between multiple packets).  It's recommended to set this a 
 * little higher, in case some junk data gets in. 
 */
#define MAX_FRAME_SIZE 20000

/**
 * @def WORD_SIZE 
 * @brief The number of bytes in a word 
 *  
 * This is used for determining the if a buffer of data has the 
 * correct/expected size. 
 */
#define WORD_SIZE 4

typedef struct
{
    //! Total number of 32-bit words in packet
    boost::uint32_t packet_size   : 16; 
    //! Modulo-16 count of packets
    boost::uint32_t packet_count  : 4; 
    //! Fractional timestamp mode
    boost::uint32_t tsf           : 2; 
    //! Integer timestamp mode 
    boost::uint32_t tsi           : 2; 
    //! Reserved
    boost::uint32_t rsvd          : 2; 
    //! Indicates if packet contains trailer (Data packets only)
    boost::uint32_t t             : 1; 
    //! Indicates if a class ID is included
    boost::uint32_t c             : 1; 
    //! Type of packet \sa VITA49Packet::PACKET_TYPE
    boost::uint32_t packet_type   : 4; 
} flex_data_header;

typedef struct
{
    //! Total number of 32-bit words in packet
    boost::uint32_t packet_size   : 16; 
    //! Modulo-16 count of packets
    boost::uint32_t packet_count  : 4; 
    //! Fractional timestamp mode
    boost::uint32_t tsf           : 2; 
    //! Integer timestamp mode 
    boost::uint32_t tsi           : 2; 
    //! Timestamp mode (Context packets only)
    boost::uint32_t tsm           : 1;
    //! Reserved
    boost::uint32_t rsvd          : 2; 
    //! Indicates if a class ID is included
    boost::uint32_t c             : 1; 
    //! Type of packet \sa VITA49Packet::PACKET_TYPE
    boost::uint32_t packet_type   : 4; 
} flex_context_header;

/**
 * The flex_packet class can parse out Flex FFT packets from a 
 * Polaris unit. 
 */
class flex_packet{
public:
    typedef boost::shared_ptr<flex_packet> sptr;

    /**
     * flex_packet constructor 
     *  
     * flex_packet's do not copy data, but instead just hold onto 
     * the pointer.  The data pointer must be valid until try_parse 
     * and is_data_ready returns true. 
     * 
     * @param data - Pointer to a buffer holding packet data
     * @param num_bytes - Size of buffer in bytes
     */
    flex_packet(boost::uint8_t* data, int num_bytes);
    ~flex_packet();

    /**
     * This function will verify if the data provided has sufficient 
     * room to be considered a valid data packet.  If it does have 
     * enough data, then it will spawn a seperate thread for 
     * preparing the fft data samples for use. 
     *  
     * @return bool 
     */
    bool try_parse(int &frame_size);

    /**
     * When try_parse successfully parses a packet, it will begin 
     * processing any samples in that packet.  is_data_ready will 
     * return true when the processing is done and the data has been 
     * copied into a local buffer. 
     * 
     * @return bool 
     */
    bool is_data_ready();

    /**
     * After a packet has been processed and data is ready, this 
     * function will return back a buffer holding all of the 
     * processed values.  You can use get_num_samples to find out 
     * how many values are in the buffer. 
     * 
     * @return float* 
     */
    float* get_fft_data();

    /**
     * Get the stream id (VID value) of this packet.
     * 
     * @return int 
     */
    int get_stream_id();

    /**
     * Return the number of samples in this packet.  The buffer 
     * holding the samples can be referenced using get_fft_data. 
     * 
     * @return int 
     */
    int get_num_samples();

    /**
     * Get the sample rate from the context packet portion of this 
     * flex_packet. 
     * 
     * @return double 
     */
    double get_sample_rate();

    /**
     * Get the fft size.
     * 
     * @return int 
     */
    int get_fft_size();

    /**
     * Get the start index of the data
     * 
     * @return int 
     */
    int get_start_index();

    /**
     * Get the end index of the data
     * 
     * @return int 
     */
    int get_end_index();

    /**
     * Get the number of averages that have been applied to this 
     * data. 
     * 
     * @return double 
     */
    double get_num_averages();

    /**
     * Get the reference level for this packet.
     * 
     * @return double 
     */
    double get_reference_level();

    /**
     * Get the center frequency for this fft data.
     * 
     * @return double 
     */
    double get_frequency();

    /**
     * Get the frame counter.
     * 
     * @return int 
     */
    int get_frame_counter();

    /**
     * Get the packet counter for the data packet.
     * 
     * @return int 
     */
    int get_data_packet_counter();

    /**
     * Get the context packet counter for the context packet.
     * 
     * @return int 
     */
    int get_context_packet_counter();

    /**
     * A helper function used by the flex_fft_parser.  Tell this 
     * packet which buffer location its at. 
     * 
     * @param location 
     */
    void set_location(int location);

    /**
     * A helper function used by the flex_fft_parser.  Request the 
     * stored information about this packets buffer location. 
     * 
     * @return int 
     */
    int get_location();

private:
    boost::uint8_t* m_raw_data;
    int m_raw_data_size;
    boost::scoped_array<float> m_fft_data;
    int m_stream_id;
    int m_num_samples;
    double m_sample_rate;
    int m_fft_size;
    int m_start_index, m_end_index;
    double m_frequency;
    double m_num_averages;
    double m_ref_level;

    int m_frame_counter;
    int m_frame_size;
    int m_data_packet_size;
    int m_context_packet_size;
    int m_data_packet_count;
    int m_context_packet_count;
    volatile boost::uint8_t* m_sample_start;

    boost::scoped_ptr<boost::thread> m_thread;
    volatile bool m_processing;
    void prepare_samples();

    int m_location;
};

/**
 * The flex_fft_parser class takes in raw sets of data that will 
 * be parsed. 
 *  
 */
class flex_fft_parser{
public:
    typedef boost::shared_ptr<flex_fft_parser> sptr;

    flex_fft_parser();
    ~flex_fft_parser();

    /**
     * Copied the given data into it's own buffers for processing. 
     * 
     * 
     * @param data Pointer to the raw packet data.
     * @param size How much data in bytes can be copied
     * 
     * @return bool True if the data was accepted, false if the 
     *         buffer was full.
     */
    bool parse_flex_packet(const unsigned char* data, int size);
    
    /**
     * Expects to be called in the same thread as parse_flex_packet. 
     * This function will return a pointer to the start of the 
     * processed packet's list.  This will only return a packet if 
     * it has completed processing it's data. 
     * 
     * @return flex_packet::sptr 
     */
    flex_packet::sptr get_next_packet();

    /**
     * Returns a pointer to a list of packets that have been added 
     * for processing.  All packets in this list should be checked 
     * with is_data_ready() before use.  This function may be called 
     * in a seperate thread than parse_flex_packet. 
     * 
     * @return std::vector<flex_packet::sptr>* 
     */
    boost::circular_buffer<flex_packet::sptr>* get_packet_list();
private:
    boost::circular_buffer<flex_packet::sptr> m_packet_list[2];
    boost::circular_buffer<flex_packet::sptr>* m_active_list;
    int m_active_index;
    int m_cur_packet;
    boost::uint8_t m_raw_data[BUFFER_SIZE][MAX_FRAME_SIZE];
    int m_required_data[BUFFER_SIZE];
    int m_raw_write_location;
    boost::lockfree::stack<int> m_available_locations;
    int m_current_index;
    
    volatile bool m_flip_lists;
    volatile bool m_processing_request;
    volatile bool m_waiting_for_settle;
};
