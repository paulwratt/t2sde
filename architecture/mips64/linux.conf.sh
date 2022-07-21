# --- T2-COPYRIGHT-NOTE-BEGIN ---
# T2 SDE: architecture/mips64/linux.conf.sh
# Copyright (C) 2013 - 2022 The T2 SDE Project
# 
# This Copyright note is generated by scripts/Create-CopyPatch,
# more information can be found in the files COPYING and README.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2.
# --- T2-COPYRIGHT-NOTE-END ---

{
	[ "$SDECFG_MIPS64_ENDIANESS" = "EL" ] &&
		echo "CONFIG_CPU_LITTLE_ENDIAN=y" ||
		echo "CONFIG_CPU_BIG_ENDIAN=y" ||

	echo
	cat <<- 'EOT'
 		include(`linux.conf.m4')
		include(`linux.conf.ip30')
	EOT
} | m4 -I $base/architecture/$arch -I $base/architecture/mips -I $base/architecture/share
