// copied from wikipedia
#include "circular_buffer.h"

// void circ_buffer_print(circ_buffer_t *cb) {
//     printf("size=0x%x, start=%d, end=%d\n", cb->size, cb->start, cb->end);
// }

int circ_buffer_is_full(circ_buffer_t *cb)
{
    return cb->end == (cb->start ^ cb->size); /* This inverts the most significant bit of start before comparison */
}

int circ_buffer_is_empty(circ_buffer_t *cb)
{
    return cb->end == cb->start;
}

int circ_buffer_incr(circ_buffer_t *cb, int p)
{
    return (p + 1) & (2 * cb->size - 1); /* start and end pointers incrementation is done modulo 2*size */
}

int circ_buffer_add(circ_buffer_t *cb)
{
    int index = cb->end & (cb->size-1);
    if (circ_buffer_is_full(cb)) /* full, overwrite moves start pointer */
        cb->start = circ_buffer_incr(cb, cb->start);
    cb->end = circ_buffer_incr(cb, cb->end);
    return index;
}

int circ_buffer_get(circ_buffer_t *cb)
{
    int index = cb->start & (cb->size-1);
    cb->start = circ_buffer_incr(cb, cb->start);
    return index;
}