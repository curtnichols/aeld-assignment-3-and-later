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
#include <linux/string.h>
#else
#include <string.h>
#include <stdio.h>
#endif

#include "aesd-circular-buffer.h"

static inline void log_find(
    struct aesd_circular_buffer *buffer,
    size_t char_offset,
    struct aesd_buffer_entry *rtn,
    size_t entry_offset_byte_rtn)
{
#ifdef AESD_LOG_WORK
    printf("$$$ char_offset=[%lu]; ptr=[%s]; offset=[%lu]\n", char_offset,rtn->buffptr, entry_offset_byte_rtn);
#endif
}

static inline void log_find_idx(int idx) {
#ifdef AESD_LOG_WORK    
    printf("((( add_entry looking at idx=[%d]\n", idx);
#endif    
}

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
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(
    struct aesd_circular_buffer *buffer,
    size_t char_offset,
    size_t *entry_offset_byte_rtn )
{
    int entries_available =
        buffer->full
            ? AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED
            : ((buffer->in_offs + AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) - buffer->out_offs)
                % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    
    size_t entry_offset = 0;
    size_t remaining_char_offset = char_offset;

    while (entry_offset < entries_available) {
        int idx = (buffer->out_offs + entry_offset) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        log_find_idx(idx);
        struct aesd_buffer_entry *pentry = &(buffer->entry[idx]);

        if (remaining_char_offset < pentry->size) {
            *entry_offset_byte_rtn = remaining_char_offset;
            log_find(buffer, char_offset, pentry, *entry_offset_byte_rtn);
            return pentry;
        }

        remaining_char_offset -= pentry->size;
        entry_offset += 1;
    }

    return NULL;
}

static inline void log_add_entry(
    struct aesd_circular_buffer *buffer,
    const struct aesd_buffer_entry *add_entry)
{
#ifdef AESD_LOG_WORK    
    printf("### buffer->full=[%d]\n", buffer->full);
    printf("###     entry len=[%lu]; value=[%s]\n", add_entry->size, add_entry->buffptr);
#endif    
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    struct aesd_buffer_entry *entries = buffer->entry;

    memcpy(&entries[buffer->in_offs], add_entry, sizeof(*add_entry));

    // Always advance "in"
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    if (buffer->full) {
        // in & out offsets are equal when the buffer is full; advance the output pointer
        buffer->out_offs = buffer->in_offs;
    }
    else {
        buffer->full = buffer->in_offs == buffer->out_offs;
    }

    log_add_entry(buffer, add_entry);
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
