
STORAGE_OVERLAY_SUFFIX := overlay

ifeq ($(ENABLE_OTG_USBDISK), true)
  STORAGE_OVERLAY_SUFFIX := otg_overlay
endif

# storage init files
ifeq ($(STORAGE_INTERNAL), emulated)
  ifeq ($(STORAGE_PRIMARY), internal)
    PRODUCT_COPY_FILES += \
	    $(PLATDIR)/init.storage1.rc:root/init.storage.rc
    PRODUCT_PACKAGE_OVERLAYS := $(PLATDIR)/storage1_$(STORAGE_OVERLAY_SUFFIX)
  endif
  ifeq ($(STORAGE_PRIMARY), external)
    PRODUCT_COPY_FILES += \
	    $(PLATDIR)/init.storage2.rc:root/init.storage.rc
    PRODUCT_PACKAGE_OVERLAYS := $(PLATDIR)/storage2_$(STORAGE_OVERLAY_SUFFIX)
  endif
endif

ifeq ($(STORAGE_INTERNAL), physical)
  ifeq ($(STORAGE_PRIMARY), internal)
    PRODUCT_COPY_FILES += \
	    $(PLATDIR)/init.storage3.rc:root/init.storage.rc
    PRODUCT_PACKAGE_OVERLAYS := $(PLATDIR)/storage3_$(STORAGE_OVERLAY_SUFFIX)
  endif
  ifeq ($(STORAGE_PRIMARY), external)
    PRODUCT_COPY_FILES += \
	    $(PLATDIR)/init.storage4.rc:root/init.storage.rc
    PRODUCT_PACKAGE_OVERLAYS := $(PLATDIR)/storage4_$(STORAGE_OVERLAY_SUFFIX)
  endif
endif

