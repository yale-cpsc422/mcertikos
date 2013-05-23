#ifndef _USER_VDEV_VIDEO_H_
#define _USER_VDEV_VIDEO_H_

#include "../../vmm_dev.h"

struct vvideo {
	struct vdev *vdev;
};

int vvideo_init(struct vdev *vdev, struct vvideo *dev);

#endif /* !_USER_VDEV_VIDEO_H_ */
