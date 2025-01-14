#ifndef ASMARM_PCI_H
#define ASMARM_PCI_H

#ifdef __KERNEL__

#include <asm/arch/hardware.h>

static inline void pcibios_set_master(struct pci_dev *dev)
{
	/* No special bus mastering setup handling */
}

static inline void pcibios_penalize_isa_irq(int irq)
{
	/* We don't do dynamic PCI IRQ allocation */
}

#include <asm/scatterlist.h>
#include <asm/io.h>

struct pci_dev;

/* Allocate and map kernel buffer using consistent mode DMA for a device.
 * hwdev should be valid struct pci_dev pointer for PCI devices,
 * NULL for PCI-like buses (ISA, EISA).
 * Returns non-NULL cpu-view pointer to the buffer if successful and
 * sets *dma_addrp to the pci side dma address as well, else *dma_addrp
 * is undefined.
 */
extern void *pci_alloc_consistent(struct pci_dev *hwdev, size_t size, dma_addr_t *handle);

/* Free and unmap a consistent DMA buffer.
 * cpu_addr is what was returned from pci_alloc_consistent,
 * size must be the same as what as passed into pci_alloc_consistent,
 * and likewise dma_addr must be the same as what *dma_addrp was set to.
 *
 * References to the memory and mappings associated with cpu_addr/dma_addr
 * past this call are illegal.
 */
static inline void
pci_free_consistent(struct pci_dev *hwdev, size_t size, void *vaddr,
		    dma_addr_t dma_handle)
{
	consistent_free(vaddr, size, dma_handle);
}

/* Map a single buffer of the indicated size for DMA in streaming mode.
 * The 32-bit bus address to use is returned.
 *
 * Once the device is given the dma address, the device owns this memory
 * until either pci_unmap_single or pci_dma_sync_single is performed.
 */
static inline dma_addr_t
pci_map_single(struct pci_dev *hwdev, void *ptr, size_t size, int direction)
{
#if defined(CONFIG_SA1111)
	extern dma_addr_t sa1111_map_single(struct pci_dev *, void *, size_t, int);

	/*
	 * for SA1111 these functions are "magic" and relocate buffers.  We
	 * only need to do these if hwdev is non-null; otherwise we expect
	 * the buffer to already be suitable for DMA.
	 */
	if (hwdev != NULL)
		return sa1111_map_single(hwdev, ptr, size, direction);
#elif defined(CONFIG_ARCH_S3C2410)
	extern dma_addr_t s3c2410_map_single(struct pci_dev *, void *, size_t, int);
	if (hwdev != NULL)
		        return s3c2410_map_single(hwdev, ptr, size, direction);
#endif
	consistent_sync(ptr, size, direction);
	return virt_to_bus(ptr);
}

/* Unmap a single streaming mode DMA translation.  The dma_addr and size
 * must match what was provided for in a previous pci_map_single call.  All
 * other usages are undefined.
 *
 * After this call, reads by the cpu to the buffer are guarenteed to see
 * whatever the device wrote there.
 */
static inline void
pci_unmap_single(struct pci_dev *hwdev, dma_addr_t dma_addr, size_t size, int direction)
{
#if defined(CONFIG_SA1111)
	extern void sa1111_unmap_single(struct pci_dev *, dma_addr_t, size_t, int);

	if (hwdev != NULL)
		sa1111_unmap_single(hwdev, dma_addr, size, direction);
#elif defined(CONFIG_ARCH_S3C2410)
	extern void s3c2410_unmap_single(struct pci_dev *, dma_addr_t, size_t, int);

	if (hwdev != NULL)
		s3c2410_unmap_single(hwdev, dma_addr, size, direction);
#endif
	/* nothing to do */
}

/* Whether pci_unmap_{single,page} is a nop depends upon the
 * configuration.
 */
#if defined(CONFIG_SA1111) || defined(CONFIG_ARCH_S3C2410)
#define DECLARE_PCI_UNMAP_ADDR(ADDR_NAME)	\
	dma_addr_t ADDR_NAME;
#define DECLARE_PCI_UNMAP_LEN(LEN_NAME)		\
	__u32 LEN_NAME;
#define pci_unmap_addr(PTR, ADDR_NAME)			\
	((PTR)->ADDR_NAME)
#define pci_unmap_addr_set(PTR, ADDR_NAME, VAL)		\
	(((PTR)->ADDR_NAME) = (VAL))
#define pci_unmap_len(PTR, LEN_NAME)			\
	((PTR)->LEN_NAME)
#define pci_unmap_len_set(PTR, LEN_NAME, VAL)		\
	(((PTR)->LEN_NAME) = (VAL))
#else /* !(CONFIG_SA1111 || CONFIG_ARCH_S3C2410) */
#define DECLARE_PCI_UNMAP_ADDR(ADDR_NAME)
#define DECLARE_PCI_UNMAP_LEN(LEN_NAME)
#define pci_unmap_addr(PTR, ADDR_NAME)		(0)
#define pci_unmap_addr_set(PTR, ADDR_NAME, VAL)	do { } while (0)
#define pci_unmap_len(PTR, LEN_NAME)		(0)
#define pci_unmap_len_set(PTR, LEN_NAME, VAL)	do { } while (0)
#endif /* CONFIG_SA1111 */

/* Map a set of buffers described by scatterlist in streaming
 * mode for DMA.  This is the scather-gather version of the
 * above pci_map_single interface.  Here the scatter gather list
 * elements are each tagged with the appropriate dma address
 * and length.  They are obtained via sg_dma_{address,length}(SG).
 *
 * NOTE: An implementation may be able to use a smaller number of
 *       DMA address/length pairs than there are SG table elements.
 *       (for example via virtual mapping capabilities)
 *       The routine returns the number of addr/length pairs actually
 *       used, at most nents.
 *
 * Device ownership issues as mentioned above for pci_map_single are
 * the same here.
 */
static inline int
pci_map_sg(struct pci_dev *hwdev, struct scatterlist *sg, int nents, int direction)
{
	int i;

	for (i = 0; i < nents; i++, sg++) {
		consistent_sync(sg->address, sg->length, direction);
		sg->dma_address = virt_to_bus(sg->address);
	}

	return nents;
}

/* Unmap a set of streaming mode DMA translations.
 * Again, cpu read rules concerning calls here are the same as for
 * pci_unmap_single() above.
 */
static inline void
pci_unmap_sg(struct pci_dev *hwdev, struct scatterlist *sg, int nents, int direction)
{
	/* nothing to do */
}

/* Make physical memory consistent for a single
 * streaming mode DMA translation after a transfer.
 *
 * If you perform a pci_map_single() but wish to interrogate the
 * buffer using the cpu, yet do not wish to teardown the PCI dma
 * mapping, you must call this function before doing so.  At the
 * next point you give the PCI dma address back to the card, the
 * device again owns the buffer.
 */
static inline void
pci_dma_sync_single(struct pci_dev *hwdev, dma_addr_t dma_handle, size_t size, int direction)
{
	consistent_sync(bus_to_virt(dma_handle), size, direction);
}

/* Make physical memory consistent for a set of streaming
 * mode DMA translations after a transfer.
 *
 * The same as pci_dma_sync_single but for a scatter-gather list,
 * same rules and usage.
 */
static inline void
pci_dma_sync_sg(struct pci_dev *hwdev, struct scatterlist *sg, int nelems, int direction)
{
	int i;

	for (i = 0; i < nelems; i++, sg++)
		consistent_sync(sg->address, sg->length, direction);
}

/* Return whether the given PCI device DMA address mask can
 * be supported properly.  For example, if your device can
 * only drive the low 24-bits during PCI bus mastering, then
 * you would pass 0x00ffffff as the mask to this function.
 */
static inline int pci_dma_supported(struct pci_dev *hwdev, u64 mask)
{
	return 1;
}

/* This isn't fine. */
#define pci_dac_dma_supported(pci_dev, mask)	(0)

/* Return the index of the PCI controller for device PDEV. */
#define pci_controller_num(PDEV)	(0)

#endif /* __KERNEL__ */
 
#endif
