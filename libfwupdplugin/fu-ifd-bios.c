/*
 * Copyright (C) 2021 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN "FuIfdBios"

#include "config.h"

#include "fu-bytes.h"
#include "fu-common.h"
#include "fu-efi-firmware-volume.h"
#include "fu-ifd-bios.h"
#include "fu-mem.h"

/**
 * FuIfdBios:
 *
 * An Intel BIOS section.
 *
 * See also: [class@FuFirmware]
 */

G_DEFINE_TYPE(FuIfdBios, fu_ifd_bios, FU_TYPE_IFD_IMAGE)

#define FU_IFD_BIOS_FIT_SIGNATURE 0x5449465F
#define FU_IFD_BIOS_FIT_SIZE	  0x150000

static gboolean
fu_ifd_bios_parse(FuFirmware *firmware,
		  GBytes *fw,
		  gsize offset,
		  FwupdInstallFlags flags,
		  GError **error)
{
	gsize bufsz = 0;
	guint32 sig;
	guint img_cnt = 0;
	const guint8 *buf = g_bytes_get_data(fw, &bufsz);

	/* jump 16MiB as required */
	if (bufsz > 0x100000)
		offset += 0x100000;

	/* read each volume in order */
	while (offset < bufsz) {
		g_autoptr(FuFirmware) firmware_tmp = NULL;

		/* ignore _FIT_ as EOF */
		if (!fu_memread_uint32_safe(buf, bufsz, offset, &sig, G_LITTLE_ENDIAN, error)) {
			g_prefix_error(error, "failed to read start signature: ");
			return FALSE;
		}
		if (sig == FU_IFD_BIOS_FIT_SIGNATURE)
			break;
		if (sig == 0xffffffff)
			break;

		/* FV */
		firmware_tmp = fu_firmware_new_from_gtypes(fw,
							   offset,
							   flags,
							   error,
							   FU_TYPE_EFI_FIRMWARE_VOLUME,
							   G_TYPE_INVALID);
		if (firmware_tmp == NULL) {
			g_prefix_error(error,
				       "failed to read @0x%x of 0x%x: ",
				       (guint)offset,
				       (guint)bufsz);
			return FALSE;
		}
		fu_firmware_set_offset(firmware_tmp, offset);
		fu_firmware_add_image(firmware, firmware_tmp);

		/* next! */
		offset += fu_firmware_get_size(firmware_tmp);
		img_cnt++;
	}

	/* found nothing */
	if (img_cnt == 0) {
		g_set_error_literal(error,
				    FWUPD_ERROR,
				    FWUPD_ERROR_INVALID_FILE,
				    "no EFI firmware volumes");
		return FALSE;
	}

	/* success */
	return TRUE;
}

static void
fu_ifd_bios_init(FuIfdBios *self)
{
	fu_firmware_set_alignment(FU_FIRMWARE(self), FU_FIRMWARE_ALIGNMENT_4K);
	fu_firmware_set_images_max(FU_FIRMWARE(self), 1024);
}

static void
fu_ifd_bios_class_init(FuIfdBiosClass *klass)
{
	FuFirmwareClass *klass_firmware = FU_FIRMWARE_CLASS(klass);
	klass_firmware->parse = fu_ifd_bios_parse;
}

/**
 * fu_ifd_bios_new:
 *
 * Creates a new #FuFirmware
 *
 * Since: 1.6.2
 **/
FuFirmware *
fu_ifd_bios_new(void)
{
	return FU_FIRMWARE(g_object_new(FU_TYPE_IFD_BIOS, NULL));
}
