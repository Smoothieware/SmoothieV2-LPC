//
//  Simple fixed size ring buffer.
//  Manage objects by value.
//  Thread safe for single Producer and single Consumer.
//  By Dennis Lang http://home.comcast.net/~lang.dennis/code/ring/ring.html
//  Slightly modified for naming

#pragma once

#include <stdlib.h>

template <class kind, size_t length=0>
class RingBuffer
{
    public:
        /**
         * @brief   Initialize ringbuffer
         * @param   Nothing
         * @return  Nothing
         * @note    The module is initialized with head and tail indexes
         * pointed to the first element
         */
        RingBuffer()
        {
            size = length;
            tail = 0;
            head = 0;
            if(length != 0) {
                buffer = (kind*) malloc(sizeof(kind) * size);
            }else{
                buffer= nullptr;
            }
        }

        /**
         * @brief   Deinitialize ringbuffer
         * @param   Nothing
         * @return  Nothing
         * @note    Deallocates the buffer from memory
         */
        ~RingBuffer()
        {
            if(length != 0 && buffer != nullptr) {
                free(buffer);
            }
        }

        /**
         * manual allocation of buffer by client, who is also responsible for deallocation
         */
        bool allocate(void *buf, size_t n)
        {
            if(length == 0) {
                size= n;
                buffer= (char *)buf;
                return true;
            }

            return false;
        }

        void deallocate()
        {
            if(length == 0) {
                size= 0;
                buffer= nullptr;
            }
        }

        bool is_ok() const { return buffer != nullptr && size > 0; }

        /**
         * @brief   Get next index of the reference index
         * @param   Reference index
         * @return  Right index of the reference index
         * @note    If reference index is the last of the buffer, returns first index instead
         */
        size_t next(size_t n) const
        {
            return (n + 1) % size;
        }

        /**
         * @brief   Check if the buffer is empty
         * @param   Nothing
         * @return  True if the buffer is empty
         * @return  False if the buffer is not empty
         */
        bool empty() const
        {
            return (tail == head);
        }

        /**
         * @brief   Check if the buffer is full
         * @param   Nothing
         * @return  True if the buffer is full
         * @return  False if the buffer is not full
         */
        bool full() const
        {
            return (next(head) == tail);
        }

        /**
         * @brief   Get actual size of the buffer
         * @param   Nothing
         * @return  The number of elements between the head and tail indexes
         */
        size_t get_size() const
        {
            return (tail > head ? size : 0) + head - tail;
        }

        /**
         * @brief   Store object to the head position
         * @param   New kind object to be stored in the buffer
         * @return  Nothing
         */
        void push_back(const kind object)
        {
            buffer[head] = object;
            head = next(head);
        }

        /**
         * @brief   Get object from the tail position
         * @param   Nothing
         * @return  Object obtained from the tail position
         */
        kind pop_front()
        {
            kind object = buffer[tail];
            tail = next(tail);
            return object;
        }

        /**
         * @brief   Get object from the tail position don't remove it
         * @param   Nothing
         * @return  Object obtained from the tail position
         */
        kind peek_front()
        {
            return buffer[tail];
        }

        void flush()
        {
            tail= head;
        }

    private:
        kind *buffer;
        size_t tail;   //Pointer to the oldest object
        size_t head;   //Pointer to the newest object
        size_t size;   //Fixed size of the buffer
};
