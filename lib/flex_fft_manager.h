#include "flex_fft_parser.h"
#include <boost/shared_array.hpp>
#include <boost/asio.hpp>

/**
 * @def FLEX_LOSS_MSG 
 * @brief The message printed out when packet loss is detected. 
 *  
 * This defines the packet loss message for flex data streams. 
 */
#define FLEX_LOSS_MSG "D"

/**
 * @def FLEX_OVERFLOW_MSG 
 * @brief Defines the message that is printed when overflow 
 *        occurs for a flex stream.
 *  
 * Defines the message that is printed when overflow occurs for 
 * a flex stream. 
 */
#define FLEX_OVERFLOW_MSG "Q"

/**
 * @def NUM_FLEX_PACKETS 
 * @brief The number for flex fft packets the manager can store 
 *        before sending data to the parser.
 *  
 * This value is multiplied by the MAX_FLEX_RECV_SIZE value in 
 * the flex_fft_parser.h to get a total buffer size.  It 
 * represents the maximum number of flex packets the manager can 
 * store before sending data to the parser. 
 */
#define NUM_FLEX_PACKETS 10000

/**
 * @def FLEX_RECV_BUFFER_SIZE 
 * @brief The total size of the receive buffers in this manager. 
 *  
 * The total size of the receive buffers in this manager.  There 
 * are two receive buffers in one flex_fft_manager. 
 */
#define FLEX_RECV_BUFFER_SIZE (NUM_FLEX_PACKETS * MAX_FLEX_RECV_SIZE)

class flex_fft_manager{
public:
    flex_fft_manager(std::string address, short port);
    ~flex_fft_manager();

    struct stream_change{
        int starting_sample;
        double sample_rate;
        int fft_size;
        double reference_level;
        double num_ave;
        double frequency;
    };

    /**
     * Add a stream to the system for processing.  The order of 
     * streams added must match the expected output order from 
     * copy_data.  Add 1 stream for each FlexFFT stream id being 
     * used. 
     * 
     * 
     * @param stream_id The stream_id to add to the manager.
     */
    void add_stream(int stream_id);

    /**
     * Return back the number of streams that have been added to 
     * this flex_fft_manager. 
     * 
     * @return unsigned int 
     */
    unsigned int get_num_streams();

    /**
     * Clear all stream data from this flex_fft_manager.
     */
    void clear_streams();

    /**
     * Given a array of buffers (target_buffers), copy the 
     * request_amount into target_buffers.  Return values for each 
     * buffers are saved into amounts_returned.  If any changes in 
     * the stream are detected they are added to the stream_changes 
     * vector. 
     * 
     * 
     * @param target_buffers - An array of buffers
     * @param request_amount - Amount of data requested to fill each 
     *                       buffer.
     * @param amounts_returned - Actual amounts of data returned 
     *                         into each buffer.
     * @param stream_changes - A list of stream changes that 
     *                       occurred for these packet's data.
     */
    void copy_data(void** target_buffers, 
                   int request_amount, 
                   std::vector<int> &amounts_returned,
                   std::vector<std::vector<stream_change> > 
                   &stream_changes);

private:
    struct stream_data{
        int stream_id;
        int packet_counter;
        boost::circular_buffer<flex_packet::sptr> flex_packets;
        stream_change last_stream_change;
    };

    std::string m_address;
    short m_port;

    std::vector<stream_data> m_flex_streams;
    boost::circular_buffer<flex_packet::sptr>* m_packets_to_sort;
    flex_fft_parser::sptr m_flex_parser;

    volatile int m_socket;
    volatile bool m_connected;
    volatile bool m_running;
    volatile bool m_flip_buffers;
    volatile int m_write_side;
    volatile int m_buffer_size;

    boost::uint8_t m_buffer[2][FLEX_RECV_BUFFER_SIZE];
    int m_copy_sizes[2][FLEX_RECV_BUFFER_SIZE];

    boost::scoped_ptr<boost::thread> m_receive_thread;
    boost::scoped_ptr<boost::thread> m_process_thread;

    /*
    Starts our threads for networking.
    */
    void setup_connection();
    /*
    process_loop requests data from the receive_loop thread and 
    passes it to our flex_fft_parser. 
    */
    void process_loop();
    /*
    receive_loop copies data into a buffer to be swapped with 
    the process_loop when requested. 
    */
    void receive_loop();
};
