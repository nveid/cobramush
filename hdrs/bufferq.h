#ifndef _BUFFERQ_H_
#define _BUFFERQ_H_
/**
 * \file bufferq.h
 *
 * \brief Headers for managing queues of buffers, a handy data structure.
 *
 *
 */


#define BUFFERQLINEOVERHEAD     (2*sizeof(int)+sizeof(time_t)+sizeof(dbref))

typedef struct bufferq BUFFERQ;

struct bufferq {
  char *buffer;		/**< Pointer to start of buffer */
  char *buffer_end;	/**< Pointer to insertion point in buffer */
  int buffer_size;	/**< Size allocated to buffer, in bytes */
  int num_buffered;	/**< Number of strings in the buffer */
  char last_string[BUFFER_LEN];	/**< Cache of last string inserted */
  char last_type;	/**< Cache of type of last string inserted */
};

#define BufferQSize(b) ((b)->buffer_size)
#define BufferQNum(b) ((b)->num_buffered)
#define BufferQLast(b) ((b)->last_string)
#define BufferQLastType(b) ((b)->last_type)

extern BUFFERQ *allocate_bufferq(int lines);
extern BUFFERQ *reallocate_bufferq(BUFFERQ * bq, int lines);
extern void free_bufferq(BUFFERQ * bq);
extern void add_to_bufferq(BUFFERQ * bq, int type, dbref player,
			   const char *msg);
extern char *iter_bufferq(BUFFERQ * bq, char **p, dbref *player, int *type,
			  time_t * timestamp);
extern int bufferq_lines(BUFFERQ * bq);
extern int isempty_bufferq(BUFFERQ * bq);
#endif
