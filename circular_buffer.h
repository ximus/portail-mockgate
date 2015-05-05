#ifndef __CIRC_BUFFER_H
#define __CIRC_BUFFER_H

typedef struct {
    int         start;  /* index of oldest element              */
    int         end;    /* index at which to write new element  */
    int         size;   /* maximum number of elements           */
} circ_buffer_t;


#define CIRC_BUFFER_INIT(SIZE) { 0, 0, SIZE }

static inline void circ_buffer_init(circ_buffer_t *__restrict cb, unsigned int size)
{
    circ_buffer_t c = CIRC_BUFFER_INIT(size);
    *cb = c;
}

// void circ_buff_print(circ_buffer_t *cb) {
//     printf("size=0x%x, start=%d, end=%d\n", cb->size, cb->start, cb->end);
// }
int circ_buffer_is_full(circ_buffer_t *cb);
int circ_buffer_is_empty(circ_buffer_t *cb);
int circ_buffer_incr(circ_buffer_t *cb, int p);
int circ_buffer_add(circ_buffer_t *cb);
int circ_buffer_get(circ_buffer_t *cb);

#endif /* __CIRC_BUFFER_H */