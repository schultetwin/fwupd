/*
 * Copyright (C) 2023 Synaptics Incorporated <simon.ho@synaptics.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <string.h>

#include "fu-synaptics-cape-sngl-firmware.h"
#include "fu-synaptics-cape-struct.h"

struct _FuSynapticsCapeSnglFirmware {
	FuSynapticsCapeFirmware parent_instance;
};

G_DEFINE_TYPE(FuSynapticsCapeSnglFirmware,
	      fu_synaptics_cape_sngl_firmware,
	      FU_TYPE_SYNAPTICS_CAPE_FIRMWARE)

static gboolean
fu_synaptics_cape_sngl_firmware_parse_stream(FuFirmware *firmware,
					     GInputStream *stream,
					     gsize offset,
					     FwupdInstallFlags flags,
					     GError **error)
{
	FuSynapticsCapeSnglFirmware *self = FU_SYNAPTICS_CAPE_SNGL_FIRMWARE(firmware);
	gsize streamsz = 0;
	guint16 num_fw_file;
	g_autoptr(GByteArray) st = NULL;
	g_autofree gchar *version_str = NULL;

	/* sanity check */
	if (!fu_input_stream_size(stream, &streamsz, error))
		return FALSE;
	if ((guint32)streamsz % 4 != 0) {
		g_set_error_literal(error,
				    FWUPD_ERROR,
				    FWUPD_ERROR_INVALID_FILE,
				    "data not aligned to 32 bits");
		return FALSE;
	}

	/* unpack */
	st = fu_struct_synaptics_cape_sngl_hdr_parse_stream(stream, offset, error);
	if (st == NULL)
		return FALSE;
	if (fu_struct_synaptics_cape_sngl_hdr_get_file_size(st) != streamsz) {
		g_set_error_literal(error,
				    FWUPD_ERROR,
				    FWUPD_ERROR_INVALID_FILE,
				    "file size is incorrect");
		return FALSE;
	}

	/* check CRC */
	if ((flags & FWUPD_INSTALL_FLAG_IGNORE_CHECKSUM) == 0) {
		guint32 crc_calc;
		const guint8 *buf;
		g_autoptr(GBytes) fw = NULL;

		fw = fu_bytes_get_contents_stream_full(stream, 0x0, G_MAXUINT32, error);
		if (fw == NULL)
			return FALSE;
		buf = g_bytes_get_data(fw, &streamsz);
		crc_calc = fu_crc32(buf + 8, streamsz - 8);
		if (crc_calc != fu_struct_synaptics_cape_sngl_hdr_get_file_crc(st)) {
			g_set_error(error,
				    G_IO_ERROR,
				    G_IO_ERROR_INVALID_DATA,
				    "CRC did not match, got 0x%x, expected 0x%x",
				    fu_struct_synaptics_cape_sngl_hdr_get_file_crc(st),
				    crc_calc);
			return FALSE;
		}
	}

	fu_synaptics_cape_firmware_set_vid(FU_SYNAPTICS_CAPE_FIRMWARE(self),
					   fu_struct_synaptics_cape_sngl_hdr_get_vid(st));
	fu_synaptics_cape_firmware_set_pid(FU_SYNAPTICS_CAPE_FIRMWARE(self),
					   fu_struct_synaptics_cape_sngl_hdr_get_pid(st));
	version_str = fu_version_from_uint32(fu_struct_synaptics_cape_sngl_hdr_get_fw_version(st),
					     FWUPD_VERSION_FORMAT_QUAD);
	fu_firmware_set_version(FU_FIRMWARE(self), version_str);

	/* add each file */
	num_fw_file = fu_struct_synaptics_cape_sngl_hdr_get_fw_file_num(st);
	if (num_fw_file == 0) {
		g_set_error_literal(error,
				    G_IO_ERROR,
				    G_IO_ERROR_INVALID_DATA,
				    "no image files found");
		return FALSE;
	}

	/* success */
	return TRUE;
}

static GByteArray *
fu_synaptics_cape_sngl_firmware_write(FuFirmware *firmware, GError **error)
{
	FuSynapticsCapeSnglFirmware *self = FU_SYNAPTICS_CAPE_SNGL_FIRMWARE(firmware);
	g_autoptr(GByteArray) buf = fu_struct_synaptics_cape_sngl_hdr_new();

	/* pack */
	fu_struct_synaptics_cape_sngl_hdr_set_vid(
	    buf,
	    fu_synaptics_cape_firmware_get_vid(FU_SYNAPTICS_CAPE_FIRMWARE(self)));
	fu_struct_synaptics_cape_sngl_hdr_set_pid(
	    buf,
	    fu_synaptics_cape_firmware_get_pid(FU_SYNAPTICS_CAPE_FIRMWARE(self)));

	/* success */
	return g_steal_pointer(&buf);
}

static void
fu_synaptics_cape_sngl_firmware_init(FuSynapticsCapeSnglFirmware *self)
{
	fu_firmware_add_flag(FU_FIRMWARE(self), FU_FIRMWARE_FLAG_HAS_CHECKSUM);
}

static void
fu_synaptics_cape_sngl_firmware_class_init(FuSynapticsCapeSnglFirmwareClass *klass)
{
	FuFirmwareClass *klass_firmware = FU_FIRMWARE_CLASS(klass);
	klass_firmware->parse_stream = fu_synaptics_cape_sngl_firmware_parse_stream;
	klass_firmware->write = fu_synaptics_cape_sngl_firmware_write;
}
