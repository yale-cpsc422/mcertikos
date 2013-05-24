#ifndef _KERN_DEV_VIDEO_H_
#define _KERN_DEV_VIDEO_H_

#ifdef _KERN_

#include <sys/types.h>

#define CRT_ROWS	25
#define CRT_COLS	80
#define CRT_SIZE	(CRT_ROWS * CRT_COLS)

/*
 * Initialzie CGA/vGA device.
 */
void video_init(void);

/*
 * Get a character's raw representation in the video buffer.
 */
uint16_t video_getraw(char c);

/*
 * Display a character at the specified position.
 */
void video_putc(int pos, char c);

/*
 * Scroll up the screen by the specified number of lines.
 */
void video_scroll_up(int nlines);

/*
 * Move the cursor to the specificed position.
 */
void video_set_cursor(int pos);

/*
 * Copy a specified number of words to the specified position of the video
 * buffer.
 */
void video_buf_write(int pos, uint16_t *src, size_t n);

/*
 * Clear a speicifed number of words from the specified position in the video
 * buffer.
 */
void video_buf_clear(int pos, size_t n);

#endif /* _KERN */

#endif /* !_KERN_DEV_VIDEO_H_ */
