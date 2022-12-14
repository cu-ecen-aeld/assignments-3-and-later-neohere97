/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/printk.h>
#include <linux/string.h>
#else
#include <string.h>
#include <stdio.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */

struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
                                                                          size_t char_offset, size_t *entry_offset_byte_rtn)
{
    int size = 0;
    int i, j;
    int buffer_index;
    int num_elements;

    // When buffer is empty send null
    if (!buffer->full && (buffer->in_offs == buffer->out_offs))
        return NULL;

    // Making a copy of beginning of buffer index
    buffer_index = buffer->out_offs;

    // Calculate number of elements present in the buffeer
    if (buffer->full)
        num_elements = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    else
        num_elements = buffer->in_offs - buffer->out_offs;

    // Iterate over elements
    for (i = 0; i < num_elements; i++)
    {

        // Iterate over characters
        for (j = 0; j < (buffer->entry[buffer_index]).size; j++)
        {
            if (size == char_offset)
            {
                // When offset is found return the pointer
                *entry_offset_byte_rtn = j;
                return &(buffer->entry[buffer_index]);
            }
            else
                // Update size byte by byte till char offset is reached
                // TODO: Update logic since iterating over byte by byte is inefficient
                size++;
        }
        buffer_index++;
        // rollback buffer index when it reaches the end
        if (buffer_index == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
            buffer_index = 0;
    }
    // When offset is not found send null
    return NULL;
}

/**
 * Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
 * If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
 * new start location.
 * Any necessary locking must be handled by the caller
 * Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
 */
char *aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    char *retval;

    retval = NULL;

    if (buffer->full)
        retval = (char *)buffer->entry[buffer->out_offs].buffptr;
    // Adds the entry at in_offs position
    (buffer->entry[buffer->in_offs]).buffptr = add_entry->buffptr;
    (buffer->entry[buffer->in_offs]).size = add_entry->size;
     buffer->total_buff_size += add_entry->size;
    // Updates in_offs and checks if it needs to rollback
    buffer->in_offs += 1;

    if (buffer->in_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
        buffer->in_offs = 0;

    if (buffer->full)
    {
        
        // Update out_off and check if needs to rollback
        buffer->out_offs += 1;

        if (buffer->out_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
            buffer->out_offs = 0;
        
        buffer->total_buff_size -= buffer->entry[buffer->in_offs].size;
    }

    // Mark the buffer as full when all elements are added
    if (buffer->in_offs == buffer->out_offs)
    {
        buffer->full = true;
    }
    return retval;
}

/**
 * Initializes the circular buffer described by @param buffer to an empty struct
 */
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer, 0, sizeof(struct aesd_circular_buffer));
}
