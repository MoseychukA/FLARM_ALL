#ifndef DATA_STRUCTURES_HH_
#define DATA_STRUCTURES_HH_

#include <stdint.h>

#include <algorithm>  // For std::copy.

template <class T>
class PFBQueue {
   public:
    struct PFBQueueConfig {
        uint16_t buf_len_num_elements = 0;
        T *buffer = nullptr;
        bool overwrite_when_full = false;
    };

    /**
    * .
    * :      !    ,      ,
    *    PFBQueues,      ,     ,  
    * .
    * @param[in] config_in         buf_len_num_elements+1, 
    * PFBQueue      .  config_in.buffer   nullptr,  
    *    buf_len_num_elements * sizeof(T).
    * @retval  PFBQueue.
    */
    PFBQueue(PFBQueueConfig config_in) : config_(config_in), buffer_length_(config_in.buf_len_num_elements) 
    {
        if (config_.buffer == nullptr) 
        {
            config_.buffer = (T *)malloc(sizeof(T) * buffer_length_);
            buffer_was_dynamically_allocated_ = true;
        }
    }

    /**
    * .   ,     .
    */
    ~PFBQueue() 
    {
        if (buffer_was_dynamically_allocated_ && config_.buffer != nullptr) 
        {
            free(config_.buffer);
            config_.buffer = nullptr;  // Prevent double free in case of shallow copy.
        }
    }
    /**
    *    .
    * @param[in] element      .
    * @retval True   , false,   .
    */
    bool Push(T element) 
    {
        uint16_t next_tail = IncrementIndex(tail_);
        if (next_tail == head_) 
        {
            if (config_.overwrite_when_full) 
            {
                // Overwriting allowed; nudge the head to overwrite the first enqueued element.
                head_++;
            }
            else 
            {
                // Overwriting not allowed; this push will result in an error.
                return false;
            }
        }
        config_.buffer[tail_] = element;
        tail_ = next_tail;
        return true;
    }
    /**
    *     .
    * @param[out] element   ,      .
    * @retval True   , false,   .
    */
    bool Pop(T &element) 
    {
        if (head_ == tail_)
        {
            return false;
        }
        element = config_.buffer[head_];
        head_ = IncrementIndex(head_);
        return true;
    }

    /**
    *     ,     .
    * @param[out] element   ,      .
    * @param[in] index      .   0 ( ).
    * @retval True   , false,     index   .
    */
    bool Peek(T &element, uint16_t index = 0) 
    {
        if (index >= Length()) {
            return false;
        }
        element = config_.buffer[IncrementIndex(head_, index)];
        return true;
    }

    /**
   *        .
   * @retval    .
   */
    uint16_t Length()
    {
        if (head_ == tail_) 
        {
            return 0;  // Empty.
        }
        else if (head_ > tail_) 
        {
            return buffer_length_ - (head_ - tail_);  // Wrapped.
        }
        else 
        {
            return tail_ - head_;  // Not wrapped.
        }
    }
    /**
    *    ,     .     
    * .
    * @retval  ,     .
    */
    inline uint16_t MaxNumElements() { return config_.buf_len_num_elements - 1; }

    /**
     * Empty out the buffer by setting the head equal to the tail.
     */
    void Clear() { head_ = tail_; }

   private:
       /**
       *     .    < 2*(config_.buf_len_num_elements+1)!
       * @param[in] index     .
       * @param[in] increment    .   1.
       * @retval    .
       */
    uint16_t IncrementIndex(uint16_t index, uint16_t increment = 1) 
    {
        index += increment;
        return index >= buffer_length_ ? index - buffer_length_ : index;
    }

    PFBQueueConfig config_;
    bool buffer_was_dynamically_allocated_ = false;
    uint16_t buffer_length_;
    uint16_t head_ = 0;
    uint16_t tail_ = 0;
};

#endif