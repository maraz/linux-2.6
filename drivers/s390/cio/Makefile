#
# Makefile for the S/390 common i/o drivers
#

obj-y += airq.o blacklist.o chsc.o cio.o css.o chp.o idset.o isc.o scsw.o \
	fcx.o itcw.o
ccw_device-objs += device.o device_fsm.o device_ops.o
ccw_device-objs += device_id.o device_pgid.o device_status.o
obj-y += ccw_device.o cmf.o
obj-$(CONFIG_CHSC_SCH) += chsc_sch.o
obj-$(CONFIG_CCWGROUP) += ccwgroup.o
obj-$(CONFIG_QDIO) += qdio.o
