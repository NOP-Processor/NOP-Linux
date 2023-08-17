#
# All Loongson based systems
#

obj-y += prom.o setup.o env.o reset.o irq.o mem.o serial.o uart_base.o

obj-$(CONFIG_SMP)	+= smp.o

obj-$(CONFIG_PCI_MSI)	+= msi.o

obj-$(CONFIG_EARLY_PRINTK)	+= early_printk.o
obj-$(CONFIG_DMA_NONCOHERENT)   += dma-noncoherent.o
