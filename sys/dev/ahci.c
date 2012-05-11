/*
 * Copyright (c) 2006 Manuel Bouyer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Manuel Bouyer.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * This code is derived from NetBSD.
 * Adapted for CertiKOS by Haozhong Zhang at Yale University.
 */

#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/mem.h>
#include <sys/pcpu.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <dev/ahci.h>
#include <dev/pci.h>
#include <dev/sata_fis.h>
#include <dev/tsc.h>

#ifdef DEBUG_AHCI

#define AHCI_DEBUG				\
	KERN_DEBUG("AHCI: ");			\
	dprintf
#else

static gcc_inline void
AHCI_DEBUG(const char *fmt, ...)
{
}

#endif

static int achi_reset(struct ahci_controller *);

static void ahci_channel_start(struct ahci_controller *, int);
static void ahci_channel_stop(struct ahci_controller *, int);

static int ahci_init_port(struct ahci_controller *, int);
static int ahci_set_port_idle(struct ahci_controller *, int);
static int ahci_spinup_port(struct ahci_controller *, int);
static void ahci_setup_port(struct ahci_controller *, int);

static int ahci_cmd_start(struct ahci_controller *, int, uint64_t, size_t,
				  void *, bool);
static int ahci_cmd_complete(struct ahci_controller *, int, uint32_t);
static void ahci_intr_port(struct ahci_controller *, int);
static int ahci_setup_dma(struct ahci_channel *, int, void *, size_t);

#ifdef DEBUG_AHCI
static void ahci_test(void);
#endif

/* XXX: only use one AHCI controller */
static struct ahci_controller ahci_ctrl;
static volatile bool ahci_inited = FALSE;

static void
ahci_port_debug(struct ahci_controller *sc, int port)
{
	if (sc == NULL)
		return;
	if (port < 0 || port >= sc->nchannels)
		return;

	KERN_DEBUG("AHCI PORT: port %d\n", port);
	dprintf("\tCLB : %08x%08x\n",
		AHCI_READ(sc->hba, AHCI_P_CLBU(port), uint32_t),
		AHCI_READ(sc->hba, AHCI_P_CLB(port), uint32_t));
	dprintf("\tFB  : %08x%08x\n",
		AHCI_READ(sc->hba, AHCI_P_FBU(port), uint32_t),
		AHCI_READ(sc->hba, AHCI_P_FB(port), uint32_t));
	/* dprintf("\tIS  : %08x\n", */
	/* 	AHCI_READ(sc->hba, AHCI_P_IS(port), uint32_t)); */
	dprintf("\tIE  : %08x\n",
		AHCI_READ(sc->hba, AHCI_P_IE(port), uint32_t));
	dprintf("\tCMD : %08x\n",
		AHCI_READ(sc->hba, AHCI_P_CMD(port), uint32_t));
	dprintf("\tTFD : %08x\n",
		AHCI_READ(sc->hba, AHCI_P_TFD(port), uint32_t));
	dprintf("\tSIG : %08x\n",
		AHCI_READ(sc->hba, AHCI_P_SIG(port), uint32_t));
	dprintf("\tSSTS: %08x\n",
		AHCI_READ(sc->hba, AHCI_P_SSTS(port), uint32_t));
	dprintf("\tSCTL: %08x\n",
		AHCI_READ(sc->hba, AHCI_P_SCTL(port), uint32_t));
	dprintf("\tSERR: %08x\n",
		AHCI_READ(sc->hba, AHCI_P_SERR(port), uint32_t));
	dprintf("\tSACT: %08x\n",
		AHCI_READ(sc->hba, AHCI_P_SACT(port), uint32_t));
	dprintf("\tCI  : %08x\n",
		AHCI_READ(sc->hba, AHCI_P_CI(port), uint32_t));
}

/*
 * Reset AHCI controller.
 */
static int
ahci_reset(struct ahci_controller *sc)
{
	int i;

	/* reset controller */
	AHCI_WRITE(sc->hba, AHCI_GHC, AHCI_GHC_HR, uint32_t);
	/* wait up to 1s for reset to complete */
	for (i = 0; i < 1000; i++) {
		delay(1000);
		if ((AHCI_READ(sc->hba, AHCI_GHC, uint32_t) & AHCI_GHC_HR) == 0)
			break;
	}
	if ((AHCI_READ(sc->hba, AHCI_GHC, uint32_t) & AHCI_GHC_HR)) {
		KERN_INFO("AHCI: failed to rest AHCI controller.\n");
		return -1;
	}
	/* enable ahci mode */
	AHCI_WRITE(sc->hba, AHCI_GHC, AHCI_GHC_AE, uint32_t);
	return 0;
}

static int
ahci_set_port_idle(struct ahci_controller *sc, int port)
{
	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(0 <= port && port < sc->nchannels);

	uint32_t cmd = AHCI_READ(sc->hba, AHCI_P_CMD(port), uint32_t);
	if ((cmd & (AHCI_P_CMD_ST | AHCI_P_CMD_CR |
		    AHCI_P_CMD_FR | AHCI_P_CMD_FRE)) == 0)
		/* AHCI port is already in idle state */
		return 0;

	/* clear AHCI_P_CMD_ST to set the port to the idle state */
	AHCI_WRITE(sc->hba, AHCI_P_CMD(port), cmd & ~AHCI_P_CMD_ST, uint32_t);
	delay(500);
	cmd = AHCI_READ(sc->hba, AHCI_P_CMD(port), uint32_t);
	if ((cmd & AHCI_P_CMD_CR) != 0) {
		/* TODO: reset port and try again */
		AHCI_DEBUG("failed to clear CR on port %x.\n", port);
		return 1;
	}

	/* clear AHCI_P_CMD_FRE if it's not cleared */
	if ((cmd & AHCI_P_CMD_FRE) == 0)
		return 0;
	AHCI_WRITE(sc->hba, AHCI_P_CMD(port), cmd & ~AHCI_P_CMD_FRE, uint32_t);
	delay(500);
	cmd = AHCI_READ(sc->hba, AHCI_P_CMD(port), uint32_t);
	if ((cmd & AHCI_P_CMD_FR) != 0) {
		/* TODO: reset port and try again */
		AHCI_DEBUG("failed to clear FT on port %x.\n", port);
		return 1;
	}

	return 0;
}

static int
ahci_init_port(struct ahci_controller *sc, int port)
{
	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(0 <= port && port < sc->nchannels);

	if (ahci_set_port_idle(sc, port) != 0) {
		AHCI_DEBUG("failed to idle port %x.\n", port);
		return 1;
	}

	/* clear SERR */
	AHCI_WRITE(sc->hba, AHCI_P_SERR(port), AHCI_P_SERR_CLEAR, uint32_t);

	/* check SCTL.DET = 0 */
	uint32_t sctl = AHCI_READ(sc->hba, AHCI_P_SCTL(port), uint32_t);
	if ((sctl & AHCI_P_SCTL_DET_MASK) != 0) {
		AHCI_DEBUG("port %d, SCTL.DET (%x) != 0.\n", port, sctl);
		return 1;
	}

	/* check SSTS.IPM = 0 & SSTS.DET = 0 */
	uint32_t ssts;
	uint8_t ipm, det;
	ssts = AHCI_READ(sc->hba, AHCI_P_SSTS(port), uint32_t);
	ipm = (ssts & AHCI_P_SSTS_IPM_MASK) >> AHCI_P_SSTS_IPM_SHIFT;
	det = (ssts & AHCI_P_SSTS_DET_MASK) >> AHCI_P_SSTS_DET_SHIFT;
	if (ipm != AHCI_P_SSTS_IPM_ACTIVE) {
		AHCI_DEBUG("port %d, SSTS.IPM (%x) != 0.\n", port, ipm);
		return 1;
	}
	if (det != AHCI_P_SSTS_DET_PRESENT) {
		AHCI_DEBUG("port %d, SSTS.DET (%x) != 0.\n", port, det);
		return 1;
	}

	return 0;
}

static int
ahci_spinup_port(struct ahci_controller *sc, int port)
{
	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(0 <= port && port < sc->nchannels);

	int i;
	uint32_t cmd, sctl, ssts, det, tfd, sts;

	cmd = AHCI_READ(sc->hba, AHCI_P_CMD(port), uint32_t);
	if (cmd & (AHCI_P_CMD_ST | AHCI_P_CMD_CR |
		   AHCI_P_CMD_FRE | AHCI_P_CMD_FR)) {
		AHCI_DEBUG("failed to spin-up, port %d, CMD %x.\n", port, cmd);
		return 1;
	}
	sctl = AHCI_READ(sc->hba, AHCI_P_SCTL(port), uint32_t);
	if (sctl & AHCI_P_SCTL_DET_MASK) {
		AHCI_DEBUG("failed to spin-up, port %d, SCTL %x.\n", port, sctl);
		return 1;
	}

	AHCI_WRITE(sc->hba, AHCI_P_CMD(port),
		   AHCI_P_CMD_ICC_AC | AHCI_P_CMD_FRE |
		   AHCI_P_CMD_POD | AHCI_P_CMD_SUD, uint32_t);
	delay(1000);
	ssts = AHCI_READ(sc->hba, AHCI_P_SSTS(port), uint32_t);
	det = (ssts & AHCI_P_SSTS_DET_MASK) >> AHCI_P_SSTS_DET_SHIFT;
	if (det != AHCI_P_SSTS_DET_PRESENT) {
		AHCI_DEBUG("failed to spin-up, port %d, SSTS.DET %x.\n", port, det);
		return 1;
	}

	ahci_channel_start(sc, port);

	/* wait for 31s */
	for (i = 0; i < 31; i++) {
		delay(1000);
		tfd = AHCI_READ(sc->hba, AHCI_P_TFD(port), uint32_t);
		sts = (tfd & AHCI_P_TFD_ST) >> AHCI_P_TFD_ST_SHIFT;
		if (!(sts & (AHCI_P_TFD_ST_BSY |
			     AHCI_P_TFD_ST_DRQ | AHCI_P_TFD_ST_ERR)))
			return 0;
	}

	AHCI_DEBUG("failed to spin-up, port %d, TFD.STS %x.\n", port, sts);

	return 1;
}

static void
ahci_setup_port(struct ahci_controller *sc, int port)
{
	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(0 <= port && port < sc->nchannels);

	int is_64bit = (sizeof(uintptr_t) == 8) && (sc->cap1 & AHCI_CAP_64BIT);
	struct ahci_channel *channel = &sc->channels[port];
	uintptr_t cmdh = (uintptr_t) channel->cmd_header_list;
	uintptr_t rfis = (uintptr_t) channel->rfis;

	AHCI_WRITE(sc->hba, AHCI_P_CLB(port), cmdh & 0xffffffff, uint32_t);
	AHCI_WRITE(sc->hba, AHCI_P_FB(port), rfis & 0xffffffff, uint32_t);
	if (is_64bit) {
		AHCI_WRITE(sc->hba, AHCI_P_CLBU(port),
			   ((uint64_t) cmdh >> 32) & 0xffffffff, uint32_t);
		AHCI_WRITE(sc->hba, AHCI_P_FBU(port),
			   ((uint64_t) rfis >> 32) & 0xffffffff, uint32_t);
	} else {
		AHCI_WRITE(sc->hba, AHCI_P_CLBU(port), 0, uint32_t);
		AHCI_WRITE(sc->hba, AHCI_P_FBU(port), 0, uint32_t);
	}

	AHCI_DEBUG("port %d, command table header list @ %08x%08x, size %x.\n",
		   port,
		   AHCI_READ(sc->hba, AHCI_P_CLBU(port), uint32_t),
		   AHCI_READ(sc->hba, AHCI_P_CLB(port), uint32_t),
		   AHCI_CMDH_SIZE);
	AHCI_DEBUG("port %d, received FIS @ %08x%08x, size %x.\n",
		   port,
		   AHCI_READ(sc->hba, AHCI_P_FBU(port), uint32_t),
		   AHCI_READ(sc->hba, AHCI_P_FB(port), uint32_t),
		   sizeof(struct ahci_r_fis));
}

/*
 * Initialize AHCI controller.
 */
int
ahci_pci_attach(struct pci_func *f)
{
	/* XXX: only attach the first AHCI controller */
	if (pcpu_onboot() == FALSE || ahci_inited == TRUE)
		return 0;

	KERN_ASSERT(f != NULL);

	if (PCI_CLASS(f->dev_class) != PCI_CLASS_MASS_STORAGE ||
	    PCI_SUBCLASS(f->dev_class) != PCI_SUBCLASS_MASS_STORAGE_SATA) {
		KERN_WARN("PCI: %02x:%02x.%d: not SATA controller.\n",
			  f->bus->busno, f->dev, f->func);
		return 0;
	}

	int i, j;

	pci_func_enable(f);

	struct ahci_controller *sc = &ahci_ctrl;

	memset(sc, 0, sizeof(struct ahci_controller));
	sc->hba = f->reg_base[5];

	/* reset AHCI controller */
	if (ahci_reset(sc) != 0)
		return 0;

	sc->cap1 = AHCI_READ(sc->hba, AHCI_CAP, uint32_t);
	sc->nchannels = (sc->cap1 & AHCI_CAP_NPMASK) + 1;
	sc->ncmds = ((sc->cap1 & AHCI_CAP_NCS) >> 8) + 1;
	sc->cap2 = AHCI_READ(sc->hba, AHCI_CAP2, uint32_t);

	uint32_t rev = AHCI_READ(sc->hba, AHCI_VS, uint32_t);
	KERN_INFO("AHCI: revision ");
	switch (rev) {
	case AHCI_VS_10:
		KERN_INFO("1.0");
		break;
	case AHCI_VS_11:
		KERN_INFO("1.1");
		break;
	case AHCI_VS_12:
		KERN_INFO("1.2");
		break;
	default:
		KERN_INFO("0x%x", rev);
		break;
	}

	KERN_INFO(", %d ports, %d command slots, features 0x%x.\n",
		  sc->nchannels, sc->ncmds, sc->cap1, sc->cap1);

	/*
	 * initialize DMA relevant structures
	 */
	size_t dmasize;
	pageinfo_t *dma_pi;
	uintptr_t dma_cmdh, dma_rfis;
	dmasize = (AHCI_RFIS_SIZE + AHCI_CMDH_SIZE) * sc->nchannels;
	if ((dma_pi = mem_pages_alloc(dmasize)) == NULL) {
		AHCI_DEBUG("failed to allocate memory for DMA.\n");
		return 0;
	}
	dma_cmdh = mem_pi2phys(dma_pi);
	dma_rfis = dma_cmdh + AHCI_CMDH_SIZE * sc->nchannels;

	/*
	 * TODO: initialize AHCI interrupts
	 */

	/*
	 * Initialize AHCI ports
	 */
	uint32_t pi = AHCI_READ(sc->hba, AHCI_PI, uint32_t);
	for (i = 0; i < MIN(AHCI_MAX_PORTS, sc->nchannels); i++) {
		if ((pi & (1 << i)) == 0) /* port i is not present */
			continue;

		if (ahci_init_port(sc, i) != 0)
			continue;

		struct ahci_channel *channel = &sc->channels[i];
		struct ahci_cmd_header *cmdh;
		uintptr_t dma_tbl, tbl_addr;

		dmasize = AHCI_CMDTBL_SIZE * sc->ncmds;
		if ((dma_pi = mem_pages_alloc(dmasize)) == NULL) {
			AHCI_DEBUG("failed to allocate memory for DMA.\n");
			continue;
		}
		dma_tbl = mem_pi2phys(dma_pi);

		channel->cmd_header_list = (struct ahci_cmd_header *)
			(dma_cmdh + AHCI_CMDH_SIZE * i);
		channel->rfis = (struct ahci_r_fis *)
			(dma_rfis + AHCI_RFIS_SIZE * i);

		cmdh = channel->cmd_header_list;
		for (j = 0, tbl_addr = dma_tbl;
		     j < sc->ncmds;
		     j++, tbl_addr += AHCI_CMDTBL_SIZE) {
			cmdh[j].cmdh_cmdtba =
				(uint32_t) (tbl_addr & 0xffffffff);
			cmdh[j].cmdh_cmdtbau = (uint32_t)
				(((uint64_t) tbl_addr >> 32) & 0xffffffff);
			AHCI_DEBUG("port %d, command table %d @ %08x%08x, size %x.\n",
				   i, j,
				   cmdh[j].cmdh_cmdtbau, cmdh[j].cmdh_cmdtba,
				   AHCI_CMDTBL_SIZE);
		}

		ahci_setup_port(sc, i);

		if (sc->cap1 & AHCI_CAP_SSU) {
			if (ahci_spinup_port(sc, i) != 0) {
				AHCI_DEBUG("failed to spin up drive on port %d.\n", i);
				continue;
			}
		} else {
			ahci_channel_start(sc, i);
		}

		channel->present = TRUE;

		channel->sig =
			AHCI_READ(sc->hba, AHCI_P_SIG(i), uint32_t);

		KERN_INFO("AHCI: ");
		switch (channel->sig) {
		case AHCI_P_SIG_ATA:
			KERN_INFO("SATA");
			break;
		case AHCI_P_SIG_ATAPI:
			KERN_INFO("SATAPI");
			break;
		case AHCI_P_SIG_SEMB:
			KERN_INFO("SEMB");
			break;
		case AHCI_P_SIG_PM:
			KERN_INFO("PM");
			break;
		default:
			KERN_INFO("(sig=%08x)", channel->sig);
			break;
		}
		KERN_INFO(" drive found on port %d", i);

		KERN_INFO(", clb %llx, fb %llx.\n",
			  (uint64_t)(uintptr_t) channel->cmd_header_list,
			  (uint64_t)(uintptr_t) channel->rfis);
	}

#ifdef DEBUG_AHCI
	ahci_test();
#endif

	return 1;
}

static int
ahci_setup_dma(struct ahci_channel *channel, int slot, void *buf, size_t nsects)
{
	KERN_ASSERT(channel != NULL);
	KERN_ASSERT(channel->present == TRUE);
	KERN_ASSERT(slot >= 0);

	int i, nprds;
	uintptr_t prd_addr;
	struct ahci_cmd_header *cmdh = &channel->cmd_header_list[slot];
	struct ahci_cmd_tbl *tbl = (struct ahci_cmd_tbl *)(uintptr_t)
		(((uint64_t) cmdh->cmdh_cmdtbau << 32) |
		 ((uint64_t) cmdh->cmdh_cmdtba));

	if (buf == NULL) {
		cmdh->cmdh_prdtl = 0;
		goto end;
	}

	nprds = ROUNDUP(nsects * ATA_SECTOR_SIZE, AHCI_PRD_SIZE) / AHCI_PRD_SIZE;
	if (nprds > AHCI_NPRD-1) {
		AHCI_DEBUG("out of transfer size (%d sectors).\n",
			   nsects);
		return 1;
	}

	for (i = 0, prd_addr = (uintptr_t) buf;
	     i < nprds;
	     i++, prd_addr += AHCI_PRD_SIZE) {
		tbl->cmdt_prd[i].prd_dba = (uint32_t) prd_addr & 0xffffffff;
		tbl->cmdt_prd[i].prd_dbau =
			(uint32_t) ((uint64_t) prd_addr >> 32) & 0xffffffff;
		tbl->cmdt_prd[i].prd_dbc = AHCI_PRD_SIZE - 1;

		AHCI_DEBUG("PRD %d, addr %08x, dba %08x%08x, dbc %d.\n",
			   i,
			   &tbl->cmdt_prd[i],
			   tbl->cmdt_prd[i].prd_dbau, tbl->cmdt_prd[i].prd_dba,
			   tbl->cmdt_prd[i].prd_dbc);
	}
	tbl->cmdt_prd[i-1].prd_dbc |= AHCI_PRD_DBC_IPC;
	cmdh->cmdh_prdtl = nprds;
 end:
	return 0;
}

static void
ahci_channel_stop(struct ahci_controller *sc, int port)
{
	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(0 <= port && port < sc->nchannels);

	int i;
	/* stop channel */
	AHCI_WRITE(sc->hba, AHCI_P_CMD(port),
		   AHCI_READ(sc->hba, AHCI_P_CMD(port), uint32_t)
		   & ~AHCI_P_CMD_ST, uint32_t);
	/* wait 1s for channel to stop */
	for (i = 0; i <100; i++) {
		if (!(AHCI_READ(sc, AHCI_P_CMD(port), uint32_t) & AHCI_P_CMD_CR))
			break;
		delay(10);
	}
	if (AHCI_READ(sc, AHCI_P_CMD(port), uint32_t) & AHCI_P_CMD_CR) {
		AHCI_DEBUG("channel %x wouldn't stop\n", port);
		/* XXX controller reset ? */
		return;
	}
}

static void
ahci_channel_start(struct ahci_controller *sc, int port)
{
	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(0 <= port && port < sc->nchannels);

	/* clear error */
	AHCI_WRITE(sc->hba, AHCI_P_SERR(port), AHCI_P_SERR_CLEAR, uint32_t);

	/* and start controller */
	AHCI_WRITE(sc->hba, AHCI_P_CMD(port),
		   AHCI_P_CMD_ICC_AC | AHCI_P_CMD_POD | AHCI_P_CMD_SUD |
		   AHCI_P_CMD_FRE | AHCI_P_CMD_ST,
		   uint32_t);
}

static void
ahci_intr_port(struct ahci_controller *sc, int port)
{
	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(0 <= port && port < sc->nchannels);

	uint32_t is, tfd;
	int slot;
	struct ahci_channel *channel;

	channel = &sc->channels[port];

	is = AHCI_READ(sc->hba, AHCI_P_IS(port), uint32_t);
	AHCI_WRITE(sc->hba, AHCI_P_IS(port), is, uint32_t);
	AHCI_DEBUG("port %d, IS %x, CI %x.\n",
		   port, is, AHCI_READ(sc->hba, AHCI_P_CI(port), uint32_t));

	if (is & (AHCI_P_IX_TFES | AHCI_P_IX_HBFS | AHCI_P_IX_IFS |
		  AHCI_P_IX_OFS | AHCI_P_IX_UFS)) {
		slot = (AHCI_READ(sc, AHCI_P_CMD(port), uint32_t) &
			AHCI_P_CMD_CCS_MASK) >> AHCI_P_CMD_CCS_SHIFT;
		if ((channel->active_slots & (1<<slot)) == 0)
			return;
		/* stop channel */
		ahci_channel_stop(sc, port);

		if (slot != 0) {
			KERN_PANIC("AHCI: slot %d is used.\n", slot);
		}

		if (is & AHCI_P_IX_TFES) {
			tfd = AHCI_READ(sc->hba, AHCI_P_TFD(port), uint32_t);
			channel->error = (tfd & AHCI_P_TFD_ERR_MASK) >>
				AHCI_P_TFD_ERR_SHIFT;
			channel->status = (tfd & 0xff);
		} else {
			/* emulate a CRC error */
			channel->error = WDCE_CRC;
			channel->status = WDCS_ERR;
		}
		ahci_cmd_complete(sc, port, is);
		/* if channel has not been restarted, do it now */
		if (!(AHCI_READ(sc->hba, AHCI_P_CMD(port), uint32_t) &
		      AHCI_P_CMD_CR))
			ahci_channel_start(sc, port);
	} else {
		slot = 0; /* XXX */
		is = AHCI_READ(sc->hba, AHCI_P_IS(port), uint32_t);
		AHCI_DEBUG("port %d, IS %x, slot %x, CI %x.\n",
			   port, is, slot,
			   AHCI_READ(sc->hba, AHCI_P_CI(port), uint32_t));
		if ((channel->active_slots & (1 << slot)) == 0) {
			AHCI_DEBUG("slot %d is inactive.\n", slot);
			return;
		}
		if (!(AHCI_READ(sc->hba, AHCI_P_CI(port), uint32_t) &
		      (1 << slot)))
			ahci_cmd_complete(sc, port, is);
	}
}

static int
ahci_cmd_start(struct ahci_controller *sc, int port,
	       uint64_t lba, size_t nsects, void *buf, bool write)
{
	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(0 <= port && port < sc->nchannels);

	KERN_ASSERT(nsects > 0);
	KERN_ASSERT(buf != NULL);

	if (write == TRUE) {
		AHCI_DEBUG("writing %d sectors from mem %08x to LBA %llx on port %d.\n",
			   nsects, buf, lba, port);
	} else {
		AHCI_DEBUG("reading %d sectors from LBA %llx to mem %08x on port %d.\n",
			   nsects, lba, buf, port);
	}

	int ret;
	struct ahci_channel *channel = &sc->channels[port];
	/* XXX: only use the first command slot */
	int slot = 0;
	struct ahci_cmd_header *cmdh = &channel->cmd_header_list[slot];
	struct ahci_cmd_tbl *tbl = (struct ahci_cmd_tbl *)(uintptr_t)
		(((uint64_t) cmdh->cmdh_cmdtbau << 32) | (cmdh->cmdh_cmdtba));

	/* prepare FIS */
	struct sata_fis_reg *fis = (struct sata_fis_reg *) tbl->cmdt_cfis;
	fis->type = SATA_FIS_TYPE_REG_H2D;
	fis->flag = (1<<7);
	if (write == TRUE)
		fis->command = ATA_CMD_WRITE_DMA_EXT;
	else
		fis->command = ATA_CMD_READ_DMA_EXT;
	fis->lba0 = (uint8_t) lba & 0xff;
	fis->lba1 = (uint8_t) (lba >> 8) & 0xff;
	fis->lba2 = (uint8_t) (lba >> 16) & 0xff;
	fis->lba3 = (uint8_t) (lba >> 24) & 0xff;
	fis->lba4 = (uint8_t) (lba >> 32) & 0xff;
	fis->lba5 = (uint8_t) (lba >> 40) & 0xff;
	fis->countl = (uint8_t) nsects & 0xff;
	fis->counth = (uint8_t) (nsects >> 8) & 0xff;
	fis->dev = 0x40;     /* LBA */
	/* fis->control = 0x80; /\* LBA *\/ */

	sata_fis_reg_debug(fis);

	/* allocat DMA memory for PRDs */
	if (ahci_setup_dma(channel, 0, buf, nsects) != 0) {
		AHCI_DEBUG("failed to setup DMA on port %x.\n", port);
		return 1;
	}

	cmdh->cmdh_flags = ((write == TRUE) ? AHCI_CMDH_F_WR : 0) | (20 / 4);
	cmdh->cmdh_prdbc = 0;

	ahci_cmd_header_debug(cmdh);

	ret = 0;
	/* polled command */
	AHCI_WRITE(sc->hba, AHCI_GHC,
		   AHCI_READ(sc->hba, AHCI_GHC, uint32_t) & ~AHCI_GHC_IE,
		   uint32_t);
	AHCI_DEBUG("GHC %08x\n", AHCI_READ(sc->hba, AHCI_GHC, uint32_t));
	/* start command */
	ahci_port_debug(sc, port);
	AHCI_WRITE(sc->hba, AHCI_P_CI(port), 1<<slot, uint32_t);
	channel->active_slots |= (1<<slot);

	/* polled command */
	/* TODO: check timeout instead of infinite loop */
	while (1) {
		uint32_t ci, is;

		ahci_intr_port(sc, port);

		ci = AHCI_READ(sc->hba, AHCI_P_CI(port), uint32_t);
		KERN_DEBUG("CI %x.\n", ci);
		if (!(ci & (1<<slot)))
			break;

		is = AHCI_READ(sc->hba, AHCI_P_IS(port), uint32_t);
		KERN_DEBUG("IS %x.\n", is);
		if (is & AHCI_P_IX_TFES) {
			AHCI_DEBUG("task file error on port %d.\n", port);
			ret = 1;
			goto end;
		}

		/* delay(10); */
	}

	uint32_t is = AHCI_READ(sc->hba, AHCI_P_IS(port), uint32_t);
	KERN_DEBUG("IS %x.\n", is);
	if (is & AHCI_P_IX_TFES) {
		AHCI_DEBUG("task file error on port %d.\n", port);
		sata_fis_reg_debug((struct sata_fis_reg *)
				   channel->rfis->rfis_rfis);
		ret = 1;
		goto end;
	}

 end:
	AHCI_WRITE(sc->hba, AHCI_GHC,
		   AHCI_READ(sc->hba, AHCI_GHC, uint32_t) | AHCI_GHC_IE,
		   uint32_t);

	return ret;
}

static int
ahci_cmd_complete(struct ahci_controller *sc, int port, uint32_t is)
{
	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(0 <= port && port < sc->nchannels);

	struct ahci_channel *channel = &sc->channels[port];
	channel->active_slots &= ~(1<<0);

	return 0;
}

#ifdef DEBUG_AHCI

static void
ahci_test(void)
{
	uint8_t buf[PAGE_SIZE];
	int i;
	uint64_t lba = 1;

	AHCI_DEBUG("ahci_test(): read 1 sector from LBA 0x1, port 0 to mem %x.\n",
		   buf);
	if (ahci_cmd_start(&ahci_ctrl, 0, lba, 1, buf, FALSE)) {
		AHCI_DEBUG("AHCI READ ERROR.\n");
		return;
	}
	for (i = 0; i < 16*10; i += 2) {
		if (i % 16 == 0)
			dprintf("\n%08x:", (uint32_t) lba+i);
		dprintf(" %02x%02x", buf[i], buf[i+1]);
	}
}

#endif
