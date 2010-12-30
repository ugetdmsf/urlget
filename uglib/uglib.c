/*
 *
 *   Copyright (C) 2005-2011 by Raymond Huang
 *   plushuang at users.sourceforge.net
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  In addition, as a special exception, the copyright holders give
 *  permission to link the code of portions of this program with the
 *  OpenSSL library under certain conditions as described in each
 *  individual source file, and distribute linked combinations
 *  including the two.
 *  You must obey the GNU Lesser General Public License in all respects
 *  for all of the code used other than OpenSSL.  If you modify
 *  file(s) with this exception, you may extend this exception to your
 *  version of the file(s), but you are not obligated to do so.  If you
 *  do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source
 *  files in the program, then also delete it here.
 *
 */

#include <string.h>

#include <uglib.h>
#include <UgPlugin-curl.h>


// ------------------------------------------------------------------
gboolean ug_class_init (void)
{
	// data
	ug_data_class_register (UgDataCommonClass);
	ug_data_class_register (UgDataProxyClass);
	ug_data_class_register (UgProgressClass);
	ug_data_class_register (UgDataHttpClass);
	ug_data_class_register (UgDataFtpClass);
	ug_data_class_register (UgDataLogClass);
	// category
	ug_data_class_register (UgCategoryClass);
	ug_data_class_register (UgRelationClass);
	// message
	ug_data_class_register (UgMessageClass);
	// plug-ins
	ug_plugin_class_register (UgPluginCurlClass);

	return TRUE;
}

void ug_class_finalize (void)
{
	// data
	ug_data_class_unregister (UgDataCommonClass);
	ug_data_class_unregister (UgDataProxyClass);
	ug_data_class_unregister (UgProgressClass);
	ug_data_class_unregister (UgDataHttpClass);
	ug_data_class_unregister (UgDataFtpClass);
	ug_data_class_unregister (UgDataLogClass);
	// category
	ug_data_class_unregister (UgCategoryClass);
	ug_data_class_unregister (UgRelationClass);
	// message
	ug_data_class_unregister (UgMessageClass);
	// plug-ins
	ug_plugin_class_unregister (UgPluginCurlClass);
}

