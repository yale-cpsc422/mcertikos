#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/mem.h>
#include <sys/pcpu.h>
#include <sys/spinlock.h>
#include <sys/stdarg.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <dev/ahci.h>
#include <dev/pci.h>
#include <dev/sata_fis.h>
#include <dev/tsc.h>

#ifdef DEBUG_AHCI

#define AHCI_DEBUG(fmt...)			\
	{					\
		KERN_DEBUG("AHCI: ");		\
		dprintf(fmt);			\
	}

#else

#define AHCI_DEBUG(fmt...)			\
	{					\
	}

#endif

/* support only one AHCI controller */
static struct ahci_controller ahci_ctrl;
static bool ahci_inited = FALSE;
static spinlock_t ahci_lk;

static int ahci_reset(struct ahci_controller *);
static int ahci_init(struct ahci_controller *);
static void ahci_print_info(struct ahci_controller *);

static int ahci_init_port(struct ahci_controller *, int port);
static int ahci_alloc_port(struct ahci_controller *, int port);
static int ahci_reset_port(struct ahci_controller *, int port);
static int ahci_spinup_port(struct ahci_controller *, int port);

static void ahci_prepare_sata_command(struct ahci_controller *,
				      int port,
				      int command);
static int ahci_command(struct ahci_controller *,
			int port,
			int write,
			int atapi,
			void *buffer,
			size_t bsize);

static gcc_inline uint8_t
ahci_readb(uintptr_t dev, uintptr_t offset)
{
	volatile uint8_t val = *(volatile uint8_t *) (dev + offset);
	return (uint8_t) val;
}

static gcc_inline uint16_t
ahci_readw(uintptr_t dev, uintptr_t offset)
{
	volatile uint16_t val = *(volatile uint16_t *) (dev + offset);
	return (uint16_t) val;
}

static gcc_inline uint32_t
ahci_readl(uintptr_t dev, uintptr_t offset)
{
	volatile uint32_t val = *(volatile uint32_t *) (dev + offset);
	return (uint32_t) val;
}

static gcc_inline void
ahci_writeb(uintptr_t dev, uintptr_t offset, uint8_t val)
{
	uintptr_t addr = dev + offset;
	*(volatile uint8_t *) addr = val;
}

static gcc_inline void
ahci_writew(uintptr_t dev, uintptr_t offset, uint16_t val)
{
	uintptr_t addr = dev + offset;
	*(volatile uint16_t *) addr = val;
}

static gcc_inline void
ahci_writel(uintptr_t dev, uintptr_t offset, uint32_t val)
{
	uintptr_t addr = dev + offset;
	*(volatile uint32_t *) addr = val;
}

static void
ahci_print_info(struct ahci_controller *sc)
{
	KERN_ASSERT(sc != NULL);

	KERN_INFO("AHCI: ");

	KERN_INFO("revision ");
	switch (sc->revision) {
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
		KERN_INFO("0x%x", sc->revision);
		break;
	}

	KERN_INFO(", %d ports, %d command slots, max speed ",
		  sc->nchannels, sc->ncmds);
	switch (sc->cap1 & AHCI_CAP_IS) {
	case AHCI_CAP_IS_GEN1:
		KERN_INFO("1.5 Gbps");
		break;
	case AHCI_CAP_IS_GEN2:
		KERN_INFO("3 Gbps");
		break;
	case AHCI_CAP_IS_GEN3:
		KERN_INFO("6 Gbps");
		break;
	default:
		KERN_INFO("unknown");
		break;
	}
	KERN_INFO("\n");

	KERN_INFO("AHCI: features");
	if (sc->cap1 & AHCI_CAP_64BIT)
		KERN_INFO(" S64A");
	if (sc->cap1 & AHCI_CAP_NCQ)
		KERN_INFO(" SNCQ");
	if (sc->cap1 & AHCI_CAP_NTF)
		KERN_INFO(" SSNTF");
	if (sc->cap1 & AHCI_CAP_MPS)
		KERN_INFO(" SMPS");
	if (sc->cap1 & AHCI_CAP_SSU)
		KERN_INFO(" SSS");
	if (sc->cap1 & AHCI_CAP_ALP)
		KERN_INFO(" SALP");
	if (sc->cap1 & AHCI_CAP_AL)
		KERN_INFO(" SAL");
	if (sc->cap1 & AHCI_CAP_CLO)
		KERN_INFO(" SCLO");
	if (sc->cap1 & AHCI_CAP_SAM)
		KERN_INFO(" SAM");
	if (sc->cap1 & AHCI_CAP_SPM)
		KERN_INFO(" SPM");
	if (sc->cap1 & AHCI_CAP_FBS)
		KERN_INFO(" FBSS");
	if (sc->cap1 & AHCI_CAP_PMD)
		KERN_INFO(" PMD");
	if (sc->cap1 & AHCI_CAP_SS)
		KERN_INFO(" SSC");
	if (sc->cap1 & AHCI_CAP_PS)
		KERN_INFO(" PSC");
	if (sc->cap1 & AHCI_CAP_CCC)
		KERN_INFO(" CCCS");
	if (sc->cap1 & AHCI_CAP_EM)
		KERN_INFO(" EMS");
	if (sc->cap1 & AHCI_CAP_XS)
		KERN_INFO(" SXS");
	if (sc->cap2 & AHCI_CAP2_APST)
		KERN_INFO(" APST");
	if (sc->cap2 & AHCI_CAP2_NVMP)
		KERN_INFO(" NVMP");
	if (sc->cap2 & AHCI_CAP2_BOH)
		KERN_INFO(" BOH");
	KERN_INFO("\n");
}

/*
 * Reset AHCI controller.
 */
static int
ahci_reset(struct ahci_controller *sc)
{
	int i;

	/* enable AHCI mode */
	ahci_writel(sc->hba, AHCI_GHC, AHCI_GHC_AE);

	/* reset controller */
	ahci_writel(sc->hba, AHCI_GHC, AHCI_GHC_AE | AHCI_GHC_HR);
	/* wait up to 1s for reset to complete */
	for (i = 0; i < 1000; i++) {
		delay(1000);
		if ((ahci_readl(sc->hba, AHCI_GHC) & AHCI_GHC_HR) == 0)
			break;
	}
	if ((ahci_readl(sc->hba, AHCI_GHC) & AHCI_GHC_HR))
		return 1;
	/* enable ahci mode */
	ahci_writel(sc->hba, AHCI_GHC, AHCI_GHC_AE);
	return 0;
}

/*
 * Initialize AHCI controller.
 */
static int
ahci_init(struct ahci_controller *sc)
{
	KERN_ASSERT(sc != NULL);

	/* reset AHCI controller */
	if (ahci_reset(sc)) {
		AHCI_DEBUG("failed to reset AHCI controller.\n");
		return 1;
	}

	/* setup sc */
	sc->cap1 = ahci_readl(sc->hba, AHCI_CAP);
	sc->nchannels = (sc->cap1 & AHCI_CAP_NPMASK) + 1;
	sc->ncmds = ((sc->cap1 & AHCI_CAP_NCS) >> 8) + 1;
	sc->cap2 = ahci_readl(sc->hba, AHCI_CAP2);
	sc->revision = ahci_readl(sc->hba, AHCI_VS);
	ahci_print_info(sc);

	return 0;
}

/*
 * Reset AHCI port.
 */
static int
ahci_reset_port(struct ahci_controller *sc, int port)
{
	uint32_t cmd, is;

	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(0 <= port && port < sc->nchannels);

	cmd = ahci_readl(sc->hba, AHCI_P_CMD(port));
	if (!(cmd & (AHCI_P_CMD_ST | AHCI_P_CMD_CR |
		     AHCI_P_CMD_FR | AHCI_P_CMD_FRE)))
		/* AHCI port is already ready */
		return 0;

	/* reset AHCI port */
	cmd &= ~(AHCI_P_CMD_ST | AHCI_P_CMD_FRE);
	ahci_writel(sc->hba, AHCI_P_CMD(port), cmd);

	/* wait for AHCI port ready */
	delay(500);
	cmd = ahci_readl(sc->hba, AHCI_P_CMD(port));
	if ((cmd & (AHCI_P_CMD_ST | AHCI_P_CMD_CR |
		    AHCI_P_CMD_FR | AHCI_P_CMD_FRE))) {
		AHCI_DEBUG("failed to rest port %d (status=%x).\n", port, cmd);
		return 1;
	}

	/* disable AHCI port interrupt */
	ahci_writel(sc->hba, AHCI_P_IE(port), 0);
	is = ahci_readl(sc->hba, AHCI_P_IS(port));
	if (is)
		ahci_writel(sc->hba, AHCI_P_IS(port), is);

	return 0;
}

/*
 * Allocate memory for the command header list and the receive FIS area.
 */
static int
ahci_alloc_port(struct ahci_controller *sc, int port)
{
	pageinfo_t *pi;
	uintptr_t clb, fb, tb, addr;
	int is_64bit, i;

	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(0 <= port && port < sc->nchannels);

	if ((pi = mem_pages_alloc(AHCI_CMDH_SIZE + AHCI_RFIS_SIZE)) == NULL) {
		AHCI_DEBUG("failed to allocate memory for command list and RFIS on port %d.\n",
			   port);
		return 1;
	}

	clb = mem_pi2phys(pi);
	fb = clb + AHCI_CMDH_SIZE;

	sc->channels[port].cmd_header_list = (struct ahci_cmd_header *) clb;
	sc->channels[port].rfis = (struct ahci_r_fis *) fb;

	is_64bit = (sizeof(uintptr_t) == 8) && (sc->cap1 & AHCI_CAP_64BIT);

	ahci_writel(sc->hba, AHCI_P_CLB(port), clb & 0xffffffff);
	ahci_writel(sc->hba, AHCI_P_FB(port), fb & 0xffffffff);

	if (is_64bit) {
		ahci_writel(sc->hba, AHCI_P_CLBU(port),
			    ((uint64_t) clb >> 32) & 0xffffffff);
		ahci_writel(sc->hba, AHCI_P_FBU(port),
			    ((uint64_t) fb >> 32) & 0xffffffff);
	} else {
		ahci_writel(sc->hba, AHCI_P_CLBU(port), 0);
		ahci_writel(sc->hba, AHCI_P_FBU(port), 0);
	}

	AHCI_DEBUG("port %d, clb %08x%08x, fb %08x%08x.\n",
		   port,
		   ahci_readl(sc->hba, AHCI_P_CLBU(port)),
		   ahci_readl(sc->hba, AHCI_P_CLB(port)),
		   ahci_readl(sc->hba, AHCI_P_FBU(port)),
		   ahci_readl(sc->hba, AHCI_P_FB(port)));

	/* allocate memory for command slots */
	if ((pi = mem_pages_alloc(AHCI_CMDTBL_SIZE * sc->ncmds)) == NULL) {
		AHCI_DEBUG("failed to allocate memory for command slots on port %d.\n",
			   port);
		return 1;
	}
	tb = mem_pi2phys(pi);
	for (i = 0, addr = tb; i < sc->ncmds; i++, addr += AHCI_CMDTBL_SIZE) {
		sc->channels[port].cmd_header_list[i].cmdh_cmdtba =
			(uint32_t) (addr & 0xffffffff);
		if (is_64bit)
			sc->channels[port].cmd_header_list[i].cmdh_cmdtbau =
				(uint32_t)
				(((uint64_t) addr >> 32) & 0xffffffff);
		else
			sc->channels[port].cmd_header_list[i].cmdh_cmdtbau = 0;

		AHCI_DEBUG("port %d, command slot %d, addr %08x%08x.\n",
			   port, i,
			   sc->channels[port].cmd_header_list[i].cmdh_cmdtbau,
			   sc->channels[port].cmd_header_list[i].cmdh_cmdtba);
	}

	return 0;
}

/*
 * Perform the staggered spin-up process.
 */
static int
ahci_spinup_port(struct ahci_controller *sc, int port)
{
	int i;
	uint32_t cmd, ssts, det, tfd, sts, serr;

	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(0 <= port && port < sc->nchannels);

	/* spin up */
	cmd = ahci_readl(sc->hba, AHCI_P_CMD(port));
	cmd |= AHCI_P_CMD_ICC_AC | AHCI_P_CMD_FRE |
		AHCI_P_CMD_POD | AHCI_P_CMD_SUD;
	ahci_writel(sc->hba, AHCI_P_CMD(port), cmd);

	/* wait up to 1 sec */
	delay(1000);
	ssts = ahci_readl(sc->hba, AHCI_P_SSTS(port));
	det = (ssts & AHCI_P_SSTS_DET_MASK) >> AHCI_P_SSTS_DET_SHIFT;
	if (det != AHCI_P_SSTS_DET_PRESENT) {
		AHCI_DEBUG("failed to spin-up, port %d, SSTS.DET %x.\n", port, det);
		return 1;
	}

	/* clear errors */
	serr = ahci_readl(sc->hba, AHCI_P_SERR(port));
	ahci_writel(sc->hba, AHCI_P_SERR(port), serr);

	/* wait for 31s */
	for (i = 0; i < 31; i++) {
		delay(1000);
		tfd = ahci_readl(sc->hba, AHCI_P_TFD(port));
		sts = (tfd & AHCI_P_TFD_ST) >> AHCI_P_TFD_ST_SHIFT;
		if (!(sts & (AHCI_P_TFD_ST_BSY |
			     AHCI_P_TFD_ST_DRQ | AHCI_P_TFD_ST_ERR)))
			return 0;
	}

	AHCI_DEBUG("failed to spin-up, port %d, TFD.STS %x.\n", port, sts);

	return 1;
}

static void
ahci_detect_drive(struct ahci_controller *sc, int port)
{
	int ret;
	uint32_t cmd;
	uint16_t buf[256];
	struct ahci_channel *channel;

	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(0 <= port && port < sc->nchannels);

	channel = &sc->channels[port];

	/* start the port */
	cmd = ahci_readl(sc->hba, AHCI_P_CMD(port));
	cmd |= AHCI_P_CMD_ST;
	ahci_writel(sc->hba, AHCI_P_CMD(port), cmd);

	channel->atapi = 0;
	switch (channel->sig) {
	case AHCI_P_SIG_ATA:
		ahci_prepare_sata_command(sc, port, ATA_ATA_IDENTIFY);
		ret = ahci_command(sc, port, 0, 0, buf, sizeof(buf));

		if (ret)
			return;

		if (buf[83] & (1 << 10)) {
			channel->lba48 = TRUE;
			channel->nsectors = *(uint64_t *) &buf[100];
		} else {
			channel->lba48 = FALSE;
			channel->nsectors = *(uint32_t *) &buf[60];
		}

		KERN_INFO("AHCI: ATA drive detected on port %d, size %d MB.\n",
			  port, channel->nsectors * ATA_SECTOR_SIZE >> 20);

		break;

	case AHCI_P_SIG_ATAPI:
		channel->atapi = 1;
		KERN_INFO("AHCI: ATAPI drive detected on port %d.\n", port);
		break;

	case AHCI_P_SIG_SEMB:
		KERN_INFO("AHCI: SEMB drive detected on port %d.\n", port);
		break;

	case AHCI_P_SIG_PM:
		KERN_INFO("AHCI: SEMB drive detected on port %d.\n", port);
		break;

	default:
		KERN_INFO("AHCI: Unknown drive (sig %x) detect on port %d.\n",
			  channel->sig, port);
		break;
	}
}

static void
ahci_prepare_sata_command(struct ahci_controller *sc, int port, int ata_cmd)
{
	struct ahci_channel *channel;
	struct ahci_cmd_header *cmdh;
	struct ahci_cmd_tbl *tbl;
	struct sata_fis_reg *fis;

	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(0 <= port && port < sc->nchannels);

	channel = &sc->channels[port];
	cmdh = &channel->cmd_header_list[0];
	tbl = (struct ahci_cmd_tbl *)(uintptr_t)
		(((uint64_t) cmdh->cmdh_cmdtbau << 32) | (cmdh->cmdh_cmdtba));
	fis = (struct sata_fis_reg *) tbl->cmdt_cfis;

	memset(fis, 0, sizeof(struct sata_fis_reg));

	fis->command = ata_cmd;
}

static int
ahci_command(struct ahci_controller *sc, int port, int write, int atapi,
	     void *buffer, size_t bsize)
{
	int i;
	uint32_t ci, is, status, cmd, serr, tfd, sctl;
#ifdef DEBUG_AHCI
	uint32_t error = 0;
#endif
	struct ahci_channel *channel;
	struct ahci_cmd_header *cmdh;
	struct ahci_cmd_tbl *tbl;
	struct sata_fis_reg *fis;
	struct ahci_r_fis *rfis;

	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(0 <= port && port < sc->nchannels);

	channel = &sc->channels[port];
	cmdh = &channel->cmd_header_list[0];
	tbl = (struct ahci_cmd_tbl *)(uintptr_t)
		(((uint64_t) cmdh->cmdh_cmdtbau << 32) | (cmdh->cmdh_cmdtba));
	fis = (struct sata_fis_reg *) tbl->cmdt_cfis;
	rfis = channel->rfis;

	fis->type = SATA_FIS_TYPE_REG_H2D;
	fis->flag = (1 << 7);

	tbl->cmdt_prd[0].prd_dba = (uint32_t) buffer & 0xffffffff;
	tbl->cmdt_prd[0].prd_dbau = 0;
	tbl->cmdt_prd[0].prd_dbc = bsize - 1;

	cmdh->cmdh_flags = (write ? AHCI_CMDH_F_WR : 0) |
		(atapi ? AHCI_CMDH_F_A : 0) | 5;
	cmdh->cmdh_prdtl = 1;
	cmdh->cmdh_prdbc = 0;

	/* clear interrupts */
	is = ahci_readl(sc->hba, AHCI_P_IS(port));
	if (is)
		ahci_writel(sc->hba, AHCI_P_IS(port), is);

	/* issue the command */
	ahci_writel(sc->hba, AHCI_P_SACT(port), 1);
	ahci_writel(sc->hba, AHCI_P_CI(port), 1);
	AHCI_DEBUG("port %d, issue command %x.\n", port, fis->command);

	/* wait up to 31 sec */
	for (i = 0; i < 3100; i++) {
		is = ahci_readl(sc->hba, AHCI_P_IS(port));

		if (is) {
			ahci_writel(sc->hba, AHCI_P_IS(port), is);

			if (is & AHCI_P_IX_PSS) {
				status = rfis->rfis_psfis[2];
#ifdef DEBUG_AHCI
				error = rfis->rfis_psfis[3];
#endif
				if (!(status & ATA_S_BUSY))
					break;
			}

			if (is & AHCI_P_IX_DHRS) {
				status = rfis->rfis_rfis[2];
#ifdef DEBUG_AHCI
				error = rfis->rfis_rfis[3];
#endif
				if (!(status & ATA_S_BUSY))
					break;
			}

			if (is & AHCI_P_IX_SDBS) {
				status = rfis->rfis_sdbfis[2];
#ifdef DEBUG_AHCI
				error = rfis->rfis_sdbfis[3];
#endif
				if (!(status & ATA_S_BUSY))
					break;
			}
		}

		delay(10);
	}

	if (i == 3100) {
		AHCI_DEBUG("port %d, timeout.\n", port);
		return 1;
	}

	/* check status */
	if ((status & ATA_S_READY) &&
	    !(status & (ATA_S_ERROR | ATA_S_DWF | ATA_S_BUSY))) {
		AHCI_DEBUG("port %d, status %x, OK.\n", port, status);

		do {
			ci = ahci_readl(sc->hba, AHCI_P_CI(port));
			smp_wmb();
		} while (ci & 0x1);

		return 0;
	} else {
		AHCI_DEBUG("port %d, status %x, error %x.\n",
			   port, status, error);

		/* error recovery (sec 6.2.2.1, AHCI v1.3) */

		/* clear PxCMD.ST to reset PxCI */
		cmd = ahci_readl(sc->hba, AHCI_P_CMD(port));
		cmd &= ~AHCI_P_CMD_ST;
		ahci_writel(sc->hba, AHCI_P_CMD(port), cmd);

		/* wait for PxCMD.CR to clear to 0 */
		while (1) {
			cmd = ahci_readl(sc->hba, AHCI_P_CMD(port));
			if (!(cmd & AHCI_P_CMD_CR))
				break;
		}

		/* clear PxSERR */
		serr = ahci_readl(sc->hba, AHCI_P_SERR(port));
		if (serr)
			ahci_writel(sc->hba, AHCI_P_SERR(port), serr);

		/* clear PxIS */
		is = ahci_readl(sc->hba, AHCI_P_IS(port));
		if (is)
			ahci_writel(sc->hba, AHCI_P_IS(port), is);

		/* issue COMRESET if necessary */
		tfd = ahci_readl(sc->hba, AHCI_P_TFD(port));
		if (tfd & (AHCI_P_TFD_ST_BSY | AHCI_P_TFD_ST_DRQ)) {
			sctl = ahci_readl(sc->hba, AHCI_P_SCTL(port));
			ahci_writel(sc->hba, AHCI_P_SCTL(port),
				    sctl | AHCI_P_SCTL_COMRESET);
			delay(1);
			ahci_writel(sc->hba, AHCI_P_SCTL(port), sctl);
		}

		/* set PxCMD.ST=1 */
		cmd = ahci_readl(sc->hba, AHCI_P_CMD(port));
		cmd |= AHCI_P_CMD_ST;
		ahci_writel(sc->hba, AHCI_P_CMD(port), cmd);

		return 1;
	}
}

static int
ahci_init_port(struct ahci_controller *sc, int port)
{
	uint32_t cmd, serr, sctl, ssts, sig;
	uint8_t ipm, det;

	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(0 <= port && port < sc->nchannels);

	memset(&sc->channels[port], 0, sizeof(struct ahci_channel));

	/* reset AHCI port */
	if (ahci_reset_port(sc, port)) {
		AHCI_DEBUG("failed to reset AHCI port %d.\n", port);
		return 1;
	}

	/* detect drive */
	serr = ahci_readl(sc->hba, AHCI_P_SERR(port));
	if (serr)
		ahci_writel(sc->hba, AHCI_P_SERR(port), serr);
	sctl = ahci_readl(sc->hba, AHCI_P_SCTL(port));
	ssts = ahci_readl(sc->hba, AHCI_P_SSTS(port));
	ipm = (ssts & AHCI_P_SSTS_IPM_MASK) >> AHCI_P_SSTS_IPM_SHIFT;
	det = (ssts & AHCI_P_SSTS_DET_MASK) >> AHCI_P_SSTS_DET_SHIFT;
	if ((sctl & AHCI_P_SCTL_DET_MASK) ||
	    ipm != AHCI_P_SSTS_IPM_ACTIVE || det != AHCI_P_SSTS_DET_PRESENT) {
		AHCI_DEBUG("no drive on port %d.\n", port);
		sc->channels[port].present = FALSE;
		return 0;
	}

	/* allocate memory for command list and FIS receive area */
	if (ahci_alloc_port(sc, port)) {
		AHCI_DEBUG("failed to allocate memory for command list and rFIS on port %d.\n",
			   port);
		return 1;
	}

	/* FIS receive enable */
	cmd = ahci_readl(sc->hba, AHCI_P_CMD(port));
	cmd |= AHCI_P_CMD_FRE;
	ahci_writel(sc->hba, AHCI_P_CMD(port), cmd);

	serr = ahci_readl(sc->hba, AHCI_P_SERR(port));
	if (serr)
		ahci_writel(sc->hba, AHCI_P_SERR(port), serr);

	/* spin-up port */
	if (ahci_spinup_port(sc, port)) {
		KERN_INFO("AHCI: port %d link down.\n", port);
		return 1;
	} else
		KERN_INFO("AHCI: port %d link up.\n", port);

	/* detect drive */
	sig = ahci_readl(sc->hba, AHCI_P_SIG(port));
	sc->channels[port].sig = sig;
	ahci_detect_drive(sc, port);

	sc->channels[port].present = TRUE;

	return 0;
}

/*
 * Initialize AHCI controller and drives attached to it.
 *
 * @return 1 when sucessful; otherwise, 0.
 */
int
ahci_pci_attach(struct pci_func *f)
{
	struct ahci_controller *sc;
	int port;
	uint32_t pi;

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

	/* enable PCI device */
	pci_func_enable(f);

	/* Initialize AHCI controller */
	sc = &ahci_ctrl;
	memset(sc, 0, sizeof(struct ahci_controller));
	sc->hba = f->reg_base[5];
	if (ahci_init(sc)) {
		AHCI_DEBUG("failed to initialize AHCI controller.\n");
		return 0;
	}

	/* Initialize AHCI ports */
	pi = ahci_readl(sc->hba, AHCI_PI);
	for (port = 0; port < MIN(AHCI_MAX_PORTS, sc->nchannels); port++) {
		if ((pi & (1 << port)) == 0) /* port is not present */
			continue;

		if (ahci_init_port(sc, port))
			AHCI_DEBUG("failed to initialize port %d.\n", port);
	}

	ahci_inited = TRUE;
	spinlock_init(&ahci_lk);

#ifdef DEBUG_AHCI
	uint8_t buf[ATA_SECTOR_SIZE];

	if (ahci_disk_read(0, 0, 1, buf)) {
		AHCI_DEBUG("read test failed.\n");
		goto test_end;
	}

#if 0
	memset(buf, 0, ATA_SECTOR_SIZE);
	if (ahci_disk_write(0, 0, 1, buf)) {
		AHCI_DEBUG("write test failed.\n");
		goto test_end;
	}

	if (ahci_disk_read(0, 0, 1, buf)) {
		AHCI_DEBUG("read test failed.\n");
		goto test_end;
	}
#endif

 test_end:
#endif

	return 1;
}

static int
ahci_disk_rw(struct ahci_controller *sc, int port, int write,
	     uint64_t lba, uint16_t nsects, void *buf)
{
	int ret;
	struct ahci_channel *channel;
	struct ahci_cmd_header *cmdh;
	struct ahci_cmd_tbl *tbl;
	struct sata_fis_reg *fis;

	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(0 <= port && port < sc->nchannels);

	channel = &sc->channels[port];

	if (channel->present == FALSE) {
		AHCI_DEBUG("port %d not available.\n", port);
		return 1;
	}

	if (lba + nsects > channel->nsectors) {
		AHCI_DEBUG("port %d, out of range.\n", port);
		return 1;
	}

	cmdh = &channel->cmd_header_list[0];
	tbl = (struct ahci_cmd_tbl *)(uintptr_t)
		(((uint64_t) cmdh->cmdh_cmdtbau << 32) | (cmdh->cmdh_cmdtba));
	fis = (struct sata_fis_reg *) tbl->cmdt_cfis;

	/* prepare command FIS */
	memset(fis, 0, sizeof(struct sata_fis_reg));
	if (nsects >= (1 << 8) || lba + nsects >= (1 << 28)) {
		fis->command = write ? ATA_WRITE_DMA48 : ATA_READ_DMA48;
		fis->lba3 = (uint8_t) (lba >> 24) & 0xff;
		fis->lba4 = (uint8_t) (lba >> 32) & 0xff;
		fis->lba5 = (uint8_t) (lba >> 40) & 0xff;
		fis->counth = (uint8_t) (nsects >> 8) & 0xff;
	} else {
		fis->command = write ? ATA_WRITE_DMA : ATA_READ_DMA;
	}
	fis->featurel = 1; /* DMA */
	fis->lba0 = (uint8_t) lba & 0xff;
	fis->lba1 = (uint8_t) (lba >> 8) & 0xff;
	fis->lba2 = (uint8_t) (lba >> 16) & 0xff;
	fis->dev = (uint8_t) ((lba >> 24) & 0xf) | 0x40;
	fis->countl = (uint8_t) nsects & 0xff;

	ret = ahci_command(sc, port, write, 0, buf, nsects * ATA_SECTOR_SIZE);
	AHCI_DEBUG("port %d, %s %d sectors %s LBA %llx %s %x, ",
		   port,
		   write ? "write" : "read",
		   (uint32_t) nsects,
		   write ? "to" : "from",
		   lba,
		   write ? "from" : "to",
		   buf);
	if (ret) {
#ifdef DEBUG_AHCI
		dprintf("failed.\n");
#endif
		return 1;
	} else {
#ifdef DEBUG_AHCI
		dprintf("OK.\n");
#endif
		return 0;
	}
}

/*
 * Read nsects sectors from LBA lba to addr buf.
 *
 * XXX: always use the first AHCI controller and drives attached to it
 *
 * @param port   the AHCI port where the hard disk is attached
 * @param lba    the LBA48 address of the sector where we start to read
 * @param nsects the number of sectors that we want to read
 * @param buf    the target memory buffer
 * @return 0 if succssful; otherwise, non-zero
 */
int
ahci_disk_read(int port,
	       uint64_t lba, uint16_t nsects, void *buf)
{
	int ret;

	spinlock_acquire(&ahci_lk);
	ret = ahci_disk_rw(&ahci_ctrl, port, 0, lba, nsects, buf);
	spinlock_release(&ahci_lk);

#ifdef DEBUG_AHCI
 /* 	if (ret) */
 /* 		goto err; */

 /* 	int i; */
 /* 	uint16_t sector; */
 /* 	uint8_t *p = buf; */
 /* 	for (sector = 0; sector < nsects; sector++) { */
 /* 		dprintf("sector 0x%llx:\n", lba+sector); */
 /* 		for (i = 0; i < ATA_SECTOR_SIZE; i++, p++) { */
 /* 			if (i % 16 == 0) */
 /* 				dprintf("\n%08x:", i); */
 /* 			dprintf(" %02x", *p); */
 /* 		} */
 /* 		dprintf("\n\n"); */
 /* 	} */
 /* err: */
#endif

	return ret;
}

/*
 * Write nsects sectors to LBA lba from addr buf.
 *
 * XXX: always use the first AHCI controller and drives attached to it
 *
 * @param port   the AHCI port where the hard disk is attached
 * @param lba    the LBA48 address of the sector where we start to write
 * @param nsects the number of sectors that we want to write
 * @param buf    the source memory buffer
 * @return 0 if succssful; otherwise, non-zero
 */
int
ahci_disk_write(int port,
		uint64_t lba, uint16_t nsects, void *buf)
{
	int ret;

	spinlock_acquire(&ahci_lk);
	ret = ahci_disk_rw(&ahci_ctrl, port, 1, lba, nsects, buf);
	spinlock_release(&ahci_lk);

	return ret;
}

uint64_t
ahci_disk_capacity(int port)
{
	struct ahci_channel *channel;

	if (port >= ahci_ctrl.nchannels)
		return 0;

	channel = &ahci_ctrl.channels[port];

	if (channel->present == TRUE)
		return channel->nsectors;
	else
		return 0;
}
