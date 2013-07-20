#ifndef _KERN_DEV_VIDEO_H_
#define _KERN_DEV_VIDEO_H_

#ifdef _KERN_

#define CRT_ROWS	25
#define CRT_COLS	80
#define CRT_SIZE	(CRT_ROWS * CRT_COLS)

void video_init(void);
void video_putc(int c);

#endif /* _KERN_ */

#endif /* !_KERN_DEV_VIDEO_H_ */
