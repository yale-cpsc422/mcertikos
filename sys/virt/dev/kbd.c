/*
 * QEMU PC keyboard emulation
 *
 * Copyright (c) 2003 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Adapted for CertiKOS by Haozhong Zhang at Yale University.
 */

#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/trap.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>
#include <sys/virt/dev/kbd.h>
#include <sys/virt/dev/ps2.h>

#include <dev/kbd.h>

#ifdef DEUBG_VKBD

#define vkbd_debug vkbd_debug

#else

static gcc_inline void
vkbd_debug_foo(const char *fmt, ...)
{
}

#define vkbd_debug vkbd_debug_foo

#endif

/* Keyboard Controller Commands */
#define KBD_CCMD_READ_MODE	0x20	/* Read mode bits */
#define KBD_CCMD_WRITE_MODE	0x60	/* Write mode bits */
#define KBD_CCMD_GET_VERSION	0xA1	/* Get controller version */
#define KBD_CCMD_MOUSE_DISABLE	0xA7	/* Disable mouse interface */
#define KBD_CCMD_MOUSE_ENABLE	0xA8	/* Enable mouse interface */
#define KBD_CCMD_TEST_MOUSE	0xA9	/* Mouse interface test */
#define KBD_CCMD_SELF_TEST	0xAA	/* Controller self test */
#define KBD_CCMD_KBD_TEST	0xAB	/* Keyboard interface test */
#define KBD_CCMD_KBD_DISABLE	0xAD	/* Keyboard interface disable */
#define KBD_CCMD_KBD_ENABLE	0xAE	/* Keyboard interface enable */
#define KBD_CCMD_READ_INPORT    0xC0    /* read input port */
#define KBD_CCMD_READ_OUTPORT	0xD0    /* read output port */
#define KBD_CCMD_WRITE_OUTPORT	0xD1    /* write output port */
#define KBD_CCMD_WRITE_OBUF	0xD2
#define KBD_CCMD_WRITE_AUX_OBUF	0xD3    /* Write to output buffer as if
					   initiated by the auxiliary device */
#define KBD_CCMD_WRITE_MOUSE	0xD4	/* Write the following byte to the mouse */
#define KBD_CCMD_DISABLE_A20    0xDD    /* HP vectra only ? */
#define KBD_CCMD_ENABLE_A20     0xDF    /* HP vectra only ? */
#define KBD_CCMD_PULSE_BITS_3_0 0xF0    /* Pulse bits 3-0 of the output port P2. */
#define KBD_CCMD_RESET          0xFE    /* Pulse bit 0 of the output port P2 = CPU reset. */
#define KBD_CCMD_NO_OP          0xFF    /* Pulse no bits of the output port P2. */

/* Keyboard Commands */
#define KBD_CMD_SET_LEDS	0xED	/* Set keyboard leds */
#define KBD_CMD_ECHO     	0xEE
#define KBD_CMD_GET_ID 	        0xF2	/* get keyboard ID */
#define KBD_CMD_SET_RATE	0xF3	/* Set typematic rate */
#define KBD_CMD_ENABLE		0xF4	/* Enable scanning */
#define KBD_CMD_RESET_DISABLE	0xF5	/* reset and disable scanning */
#define KBD_CMD_RESET_ENABLE   	0xF6    /* reset and enable scanning */
#define KBD_CMD_RESET		0xFF	/* Reset */

/* Keyboard Replies */
#define KBD_REPLY_POR		0xAA	/* Power on reset */
#define KBD_REPLY_ACK		0xFA	/* Command ACK */
#define KBD_REPLY_RESEND	0xFE	/* Command NACK, send the cmd again */

/* Status Register Bits */
#define KBD_STAT_OBF 		0x01	/* Keyboard output buffer full */
#define KBD_STAT_IBF 		0x02	/* Keyboard input buffer full */
#define KBD_STAT_SELFTEST	0x04	/* Self test successful */
#define KBD_STAT_CMD		0x08	/* Last write was a command write (0=data) */
#define KBD_STAT_UNLOCKED	0x10	/* Zero if keyboard locked */
#define KBD_STAT_MOUSE_OBF	0x20	/* Mouse output buffer full */
#define KBD_STAT_GTO 		0x40	/* General receive/xmit timeout */
#define KBD_STAT_PERR 		0x80	/* Parity error */

/* Controller Mode Register Bits */
#define KBD_MODE_KBD_INT	0x01	/* Keyboard data generate IRQ1 */
#define KBD_MODE_MOUSE_INT	0x02	/* Mouse data generate IRQ12 */
#define KBD_MODE_SYS 		0x04	/* The system flag (?) */
#define KBD_MODE_NO_KEYLOCK	0x08	/* The keylock doesn't affect the keyboard if set */
#define KBD_MODE_DISABLE_KBD	0x10	/* Disable keyboard interface */
#define KBD_MODE_DISABLE_MOUSE	0x20	/* Disable mouse interface */
#define KBD_MODE_KCC 		0x40	/* Scan code conversion to PC format */
#define KBD_MODE_RFU		0x80

/* Output Port Bits */
#define KBD_OUT_RESET           0x01    /* 1=normal mode, 0=reset */
#define KBD_OUT_A20             0x02    /* x86 only */
#define KBD_OUT_OBF             0x10    /* Keyboard output buffer full */
#define KBD_OUT_MOUSE_OBF       0x20    /* Mouse output buffer full */

/* Mouse Commands */
#define AUX_SET_SCALE11		0xE6	/* Set 1:1 scaling */
#define AUX_SET_SCALE21		0xE7	/* Set 2:1 scaling */
#define AUX_SET_RES		0xE8	/* Set resolution */
#define AUX_GET_SCALE		0xE9	/* Get scaling factor */
#define AUX_SET_STREAM		0xEA	/* Set stream mode */
#define AUX_POLL		0xEB	/* Poll */
#define AUX_RESET_WRAP		0xEC	/* Reset wrap mode */
#define AUX_SET_WRAP		0xEE	/* Set wrap mode */
#define AUX_SET_REMOTE		0xF0	/* Set remote mode */
#define AUX_GET_TYPE		0xF2	/* Get type */
#define AUX_SET_SAMPLE		0xF3	/* Set sample rate */
#define AUX_ENABLE_DEV		0xF4	/* Enable aux device */
#define AUX_DISABLE_DEV		0xF5	/* Disable aux device */
#define AUX_SET_DEFAULT		0xF6
#define AUX_RESET		0xFF	/* Reset aux device */
#define AUX_ACK			0xFA	/* Command byte ACK. */

#define MOUSE_STATUS_REMOTE     0x40
#define MOUSE_STATUS_ENABLED    0x20
#define MOUSE_STATUS_SCALE21    0x10

#define KBD_PENDING_KBD         1
#define KBD_PENDING_AUX         2

static void
vkbd_update_irq(struct vkbd *vkbd)
{
	KERN_ASSERT(vkbd != NULL);

	int irq_kbd_level, irq_mouse_level;

	irq_kbd_level = 0;
	irq_mouse_level = 0;

	vkbd->status &= ~(KBD_STAT_OBF | KBD_STAT_MOUSE_OBF);
	vkbd->outport &= ~(KBD_OUT_OBF | KBD_OUT_MOUSE_OBF);
	if (vkbd->pending) {
		vkbd->status |= KBD_STAT_OBF;
		vkbd->outport |= KBD_OUT_OBF;
		/* kbd data takes priority over aux data.  */
		if (vkbd->pending == KBD_PENDING_AUX) {
			vkbd->status |= KBD_STAT_MOUSE_OBF;
			vkbd->outport |= KBD_OUT_MOUSE_OBF;
			if (vkbd->mode & KBD_MODE_MOUSE_INT)
				irq_mouse_level = 1;
		} else {
			if ((vkbd->mode & KBD_MODE_KBD_INT) &&
			    !(vkbd->mode & KBD_MODE_DISABLE_KBD))
				irq_kbd_level = 1;
		}
	}

	/* the interrupts from i8042 are edge-triggered */

	if (irq_kbd_level) {
		vmm_set_vm_irq(vkbd->vm, IRQ_KBD, 0);
		vmm_set_vm_irq(vkbd->vm, IRQ_KBD, 1);
	}

	if (irq_mouse_level) {
		vmm_set_vm_irq(vkbd->vm, IRQ_MOUSE, 0);
		vmm_set_vm_irq(vkbd->vm, IRQ_MOUSE, 1);
	}
}

static void
vkbd_update_kbd_irq(void *opaque, int level)
{
	KERN_ASSERT(opaque != NULL);

	struct vkbd *vkbd = (struct vkbd *) opaque;

	if (level) {
		vkbd->pending |= KBD_PENDING_KBD;
	} else {
		vkbd->pending &= ~KBD_PENDING_KBD;
	}

	vkbd_update_irq(vkbd);
}

static void
vkbd_update_aux_irq(void *opaque, int level)
{
	KERN_ASSERT(opaque != NULL);

	struct vkbd *vkbd = (struct vkbd *) opaque;

	if (level)
		vkbd->pending |= KBD_PENDING_AUX;
	else
		vkbd->pending &= ~KBD_PENDING_AUX;

	vkbd_update_irq(vkbd);
}

static uint8_t
vkbd_read_status(struct vkbd *vkbd)
{
	KERN_ASSERT(vkbd != NULL);
	return vkbd->status;
}

static void
vkbd_queue(struct vkbd *vkbd, int b, int aux)
{
	KERN_ASSERT(vkbd != NULL);

	if (aux) {
		vkbd_debug("Enqueue %x to AUX.\n", (uint8_t) b);
		ps2_queue(&vkbd->mouse.common, b);
	} else {
		vkbd_debug("Enqueue %x to KBD.\n", (uint8_t) b);
		ps2_queue(&vkbd->kbd.common, b);
	}
}

static void
outport_write(struct vkbd *vkbd, uint32_t data)
{
	KERN_ASSERT(vkbd != NULL);

	vkbd->outport = data;

	/*
	 * XXX: Does it need to raise an IRQ related to A20? QEMU seems to
	 *      do so, but I do not know the reason.
	 */
	/* if (vkbd->a20_out) { */
	/* 	qemu_set_irq(*vkbd->a20_out, (data >> 1) & 1); */
	/* } */

	if (!(data & 1)) {
		KERN_WARN("Reset in not implemented yet.\n");
	}
}

static void
vkbd_write_command(struct vkbd *vkbd, uint32_t data)
{
	KERN_ASSERT(vkbd != NULL);

	/* Bits 3-0 of the output port P2 of the keyboard controller may be pulsed
	 * low for approximately 6 micro seconds. Bits 3-0 of the KBD_CCMD_PULSE
	 * command specify the output port bits to be pulsed.
	 * 0: Bit should be pulsed. 1: Bit should not be modified.
	 * The only useful version of this command is pulsing bit 0,
	 * which does a CPU reset.
	 */
	if((data & KBD_CCMD_PULSE_BITS_3_0) == KBD_CCMD_PULSE_BITS_3_0) {
		if(!(data & 1))
			data = KBD_CCMD_RESET;
		else
			data = KBD_CCMD_NO_OP;
	}

	switch(data) {
	case KBD_CCMD_READ_MODE:
		vkbd_queue(vkbd, vkbd->mode, 0);
		break;

	case KBD_CCMD_WRITE_MODE:
	case KBD_CCMD_WRITE_OBUF:
	case KBD_CCMD_WRITE_AUX_OBUF:
	case KBD_CCMD_WRITE_MOUSE:
	case KBD_CCMD_WRITE_OUTPORT:
		vkbd->write_cmd = data;
		break;

	case KBD_CCMD_MOUSE_DISABLE:
		vkbd->mode |= KBD_MODE_DISABLE_MOUSE;
		break;

	case KBD_CCMD_MOUSE_ENABLE:
		vkbd->mode &= ~KBD_MODE_DISABLE_MOUSE;
		break;

	case KBD_CCMD_TEST_MOUSE:
		vkbd_queue(vkbd, 0x00, 0);
		break;

	case KBD_CCMD_SELF_TEST:
		vkbd->status |= KBD_STAT_SELFTEST;
		vkbd_queue(vkbd, 0x55, 0);
		break;

	case KBD_CCMD_KBD_TEST:
		vkbd_queue(vkbd, 0x00, 0);
		break;

	case KBD_CCMD_KBD_DISABLE:
		vkbd->mode |= KBD_MODE_DISABLE_KBD;
		vkbd_update_irq(vkbd);
		break;

	case KBD_CCMD_KBD_ENABLE:
		vkbd->mode &= ~KBD_MODE_DISABLE_KBD;
		vkbd_update_irq(vkbd);
		break;

	case KBD_CCMD_READ_INPORT:
		vkbd_queue(vkbd, 0x00, 0);
		break;

	case KBD_CCMD_READ_OUTPORT:
		vkbd_queue(vkbd, vkbd->outport, 0);
		break;

	case KBD_CCMD_ENABLE_A20:
		/*
		 * XXX: Does it need to raise an IRQ related to A20? QEMU does
		 *      so, but I do not know the reason.
		 */
		/* if (vkbd->a20_out) { */
		/* 	qemu_irq_raise(*vkbd->a20_out); */
		/* } */
		vkbd->outport |= KBD_OUT_A20;
		break;

	case KBD_CCMD_DISABLE_A20:
		/* if (vkbd->a20_out) { */
		/* 	qemu_irq_lower(*vkbd->a20_out); */
		/* } */
		vkbd->outport &= ~KBD_OUT_A20;
		break;

	case KBD_CCMD_RESET:
		KERN_WARN("Reset is not implemented yet.\n");
		break;

	case KBD_CCMD_NO_OP:
		/* ignore that */
		break;

	default:
		KERN_PANIC("Unknown KBD command=%x.\n", data);
	}
}

static uint32_t
vkbd_read_data(struct vkbd *vkbd)
{
	KERN_ASSERT(vkbd != NULL);

	uint32_t data;

	if (vkbd->pending == KBD_PENDING_AUX) {
		data = ps2_read_data(&vkbd->mouse.common);
	} else {
		data = ps2_read_data(&vkbd->kbd.common);
	}

	return data;
}

static void
vkbd_write_data(struct vkbd *vkbd, uint32_t data)
{
	KERN_ASSERT(vkbd != NULL);

	switch(vkbd->write_cmd) {
	case 0:
		ps2_write_keyboard(&vkbd->kbd, data);
		break;

	case KBD_CCMD_WRITE_MODE:
		vkbd->mode = data;
		ps2_keyboard_set_translation(&vkbd->kbd, (vkbd->mode & KBD_MODE_KCC) != 0);
		/* ??? */
		vkbd_update_irq(vkbd);
		break;

	case KBD_CCMD_WRITE_OBUF:
		vkbd_queue(vkbd, data, 0);
		break;

	case KBD_CCMD_WRITE_AUX_OBUF:
		vkbd_queue(vkbd, data, 1);
		break;

	case KBD_CCMD_WRITE_OUTPORT:
		outport_write(vkbd, data);
		break;

	case KBD_CCMD_WRITE_MOUSE:
		ps2_write_mouse(&vkbd->mouse, data);
		break;

	default:
		break;
	}

	vkbd->write_cmd = 0;
}

static void
_vkbd_ioport_read(struct vm *vm, void *vkbd, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vkbd != NULL && data != NULL);
	KERN_ASSERT(port == KBSTATP || port == KBDATAP);

	if (port == KBSTATP)
		*(uint8_t *) data = vkbd_read_status(vkbd);
	else
		*(uint8_t *) data = vkbd_read_data(vkbd);

	/* vkbd_debug("Read port=%x, data=%x.\n", port, *(uint8_t *) data); */
}

static void
_vkbd_ioport_write(struct vm *vm, void *vkbd, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vkbd != NULL && data != NULL);
	KERN_ASSERT(port == KBCMDP || port == KBDATAP);

	/* vkbd_debug("Write port=%x, data=%x.\n", port, *(uint8_t *) data); */

	if (port == KBDATAP)
		vkbd_write_data(vkbd, *(uint8_t *) data);
	else
		vkbd_write_command(vkbd, *(uint8_t *) data);
}

static void
vkbd_reset(struct vkbd *vkbd)
{
	KERN_ASSERT(vkbd != NULL);

	vkbd->mode = KBD_MODE_KBD_INT | KBD_MODE_MOUSE_INT;
	vkbd->status = KBD_STAT_CMD | KBD_STAT_UNLOCKED;
	vkbd->outport = KBD_OUT_RESET | KBD_OUT_A20;
}

static void
vkbd_sync_kbd(struct vm *vm)
{
	/* synchronize the state of physical i8042 to virtualized i8042 */
	while (inb(KBSTATP) & KBD_STAT_OBF) {
		int c = inb(KBDATAP);
		vkbd_queue(&vm->vkbd, c, 0);
	}
}

void inject_vkbd_queue(struct vkbd *vkbd, int b, int aux)
{
  	vkbd_queue(vkbd, b, aux);
}

void
vkbd_init(struct vkbd *vkbd, struct vm *vm)
{
	KERN_ASSERT(vkbd != NULL && vm != NULL);

	memset(vkbd, 0x0, sizeof(struct vkbd));

	vkbd->status = KBD_STAT_CMD | KBD_STAT_UNLOCKED;
	vkbd->outport = KBD_OUT_RESET | KBD_OUT_A20;
	vkbd->vm = vm;

	ps2_kbd_init(&vkbd->kbd, vkbd_update_kbd_irq, vkbd);
	ps2_mouse_init(&vkbd->mouse, vkbd_update_aux_irq, vkbd);

	/* register virtualized device (handlers of I/O ports &  IRQs) */
	vmm_iodev_register_read(vm, vkbd, KBSTATP, SZ8, _vkbd_ioport_read);
	vmm_iodev_register_read(vm, vkbd, KBDATAP, SZ8, _vkbd_ioport_read);
	vmm_iodev_register_write(vm, vkbd, KBCMDP, SZ8, _vkbd_ioport_write);
	vmm_iodev_register_write(vm, vkbd, KBDATAP, SZ8, _vkbd_ioport_write);
	vmm_register_extintr(vm, vkbd, IRQ_KBD, vkbd_sync_kbd);
}
