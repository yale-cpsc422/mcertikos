#ifndef _KERN_DEV_VIDEO_H_
#define _KERN_DEV_VIDEO_H_

#ifndef _KERN_
#error "This is a kernel header file; do not include it in userspace programs."
#endif /* !_KERN_ */

#define MONO_BASE	0x3B4
#define MONO_BUF	0xB0000
#define CGA_BASE	0x3D4
#define CGA_BUF		0xB8000

#define CRT_ROWS	25
#define CRT_COLS	80
#define CRT_SIZE	(CRT_ROWS * CRT_COLS)

void video_init(void);
void video_putc(int c);

#endif /* !_KERN_DEV_VIDEO_H_ */
