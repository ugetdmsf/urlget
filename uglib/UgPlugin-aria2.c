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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_PLUGIN_ARIA2

#include <UgPlugin-aria2.h>

#ifdef HAVE_LIBPWMD
#include <stdlib.h>
#include <libpwmd.h>
#endif  // HAVE_LIBPWMD

#define ARIA2_XMLRPC_URI		"http://localhost:6800/rpc"

// functions for UgPluginClass
static gboolean	ug_plugin_aria2_global_init		(void);
static void		ug_plugin_aria2_global_finalize	(void);

static gboolean	ug_plugin_aria2_init		(UgPluginAria2* plugin, UgDataset* dataset);
static void		ug_plugin_aria2_finalize	(UgPluginAria2* plugin);

static UgResult	ug_plugin_aria2_set_state	(UgPluginAria2* plugin, UgState  state);
static UgResult	ug_plugin_aria2_get_state	(UgPluginAria2* plugin, UgState* state);
static UgResult	ug_plugin_aria2_set			(UgPluginAria2* plugin, guint parameter, gpointer data);
static UgResult	ug_plugin_aria2_get			(UgPluginAria2* plugin, guint parameter, gpointer data);

// thread function
static gpointer	ug_plugin_aria2_thread		(UgPluginAria2* plugin);

// aria2 methods
static gboolean	ug_plugin_aria2_response	(UgPluginAria2* plugin, const gchar* method);
static gboolean	ug_plugin_aria2_add_uri		(UgPluginAria2* plugin);
static gboolean	ug_plugin_aria2_remove		(UgPluginAria2* plugin);
static gboolean	ug_plugin_aria2_get_servers	(UgPluginAria2* plugin);
static gboolean	ug_plugin_aria2_tell_status	(UgPluginAria2* plugin);

// setup functions
static void		ug_plugin_aria2_set_scheme	(UgPluginAria2* plugin, UgXmlrpcValue* options);
static void		ug_plugin_aria2_set_http	(UgPluginAria2* plugin, UgXmlrpcValue* options);
static gboolean	ug_plugin_aria2_set_proxy	(UgPluginAria2* plugin, UgXmlrpcValue* options);
#ifdef HAVE_LIBPWMD
static gboolean	ug_plugin_aria2_set_proxy_pwmd (UgPluginAria2 *plugin, UgXmlrpcValue* options);
#endif

// utility
static gint64	ug_xmlrpc_value_get_int64	(UgXmlrpcValue* value);
static int		ug_xmlrpc_value_get_int		(UgXmlrpcValue* value);


enum Aria2Status
{
	ARIA2_WAITING,
	ARIA2_PAUSED,
	ARIA2_ACTIVE,
	ARIA2_ERROR,
	ARIA2_COMPLETE,
	ARIA2_REMOVED,
};

enum Aria2ErrorCode
{
	ARIA2_ERROR_NONE,
	ARIA2_ERROR_UNKNOW,
	ARIA2_ERROR_TIME_OUT,
	ARIA2_ERROR_RESOURCE_NOT_FOUND,
	ARIA2_ERROR_TOO_MANY_RESOURCE_NOT_FOUND,
	ARIA2_ERROR_SPEED_TOO_SLOW,
	ARIA2_ERROR_NETWORK,
};

// static data for UgPluginClass
static const char*	supported_schemes[]   = {"http", "https", "ftp", "ftps", NULL};
static const char*	supported_filetypes[] = {"torrent", "metalink", NULL};

static const	UgPluginClass	plugin_class_aria2 =
{
	"aria2",													// name
	NULL,														// reserve
	sizeof (UgPluginAria2),										// instance_size
	supported_schemes,											// schemes
	supported_filetypes,										// file_types

	(UgGlobalInitFunc)		ug_plugin_aria2_global_init,		// global_init
	(UgGlobalFinalizeFunc)	ug_plugin_aria2_global_finalize,	// global_finalize

	(UgPluginInitFunc)		ug_plugin_aria2_init,				// init
	(UgFinalizeFunc)		ug_plugin_aria2_finalize,			// finalize

	(UgSetStateFunc)		ug_plugin_aria2_set_state,			// set_state
	(UgGetStateFunc)		ug_plugin_aria2_get_state,			// get_state
	(UgSetFunc)				ug_plugin_aria2_set,				// set
	(UgGetFunc)				ug_plugin_aria2_get,				// get
};

// extern
const UgPluginClass*	UgPluginAria2Class = &plugin_class_aria2;
// ----------------------------------------------------------------------------
// functions for UgPluginClass
//
static gboolean	ug_plugin_aria2_global_init (void)
{
	return TRUE;
}

static void	ug_plugin_aria2_global_finalize (void)
{
}

static gboolean	ug_plugin_aria2_init (UgPluginAria2* plugin, UgDataset* dataset)
{
	UgDataCommon*	common;
	UgDataHttp*		http;

	// get data
	common = ug_dataset_get (dataset, UgDataCommonClass, 0);
	http   = ug_dataset_get (dataset, UgDataHttpClass, 0);
	// check data
	if (common == NULL  ||  common->url == NULL)
		return FALSE;
	// reset data
	if (common)
		common->retry_count = 0;
	if (http)
		http->redirection_count = 0;
	// copy supported data
	plugin->common = ug_data_list_copy (ug_dataset_get (dataset, UgDataCommonClass, 0));
	plugin->proxy  = ug_data_list_copy (ug_dataset_get (dataset, UgDataProxyClass, 0));
	plugin->http   = ug_data_list_copy (ug_dataset_get (dataset, UgDataHttpClass, 0));
	plugin->ftp    = ug_data_list_copy (ug_dataset_get (dataset, UgDataFtpClass, 0));
	// xmlrpc
	ug_xmlrpc_init (&plugin->xmlrpc);
	ug_xmlrpc_use_client (&plugin->xmlrpc, ARIA2_XMLRPC_URI, NULL);
	// others
	plugin->string = g_string_sized_new (128);
	plugin->chunk  = g_string_chunk_new (512);

	return TRUE;
}

static void	ug_plugin_aria2_finalize (UgPluginAria2* plugin)
{
	// free data
	ug_data_list_free (plugin->common);
	ug_data_list_free (plugin->proxy);
	ug_data_list_free (plugin->http);
	ug_data_list_free (plugin->ftp);
	// xmlrpc
	ug_xmlrpc_finalize (&plugin->xmlrpc);
	// others
	g_string_free (plugin->string, TRUE);
	g_string_chunk_free (plugin->chunk);
}

static UgResult	ug_plugin_aria2_set_state (UgPluginAria2* plugin, UgState  state)
{
	UgState		old_state;

	old_state		= plugin->state;
	plugin->state	= state;
	// change plug-in status
	if (state != old_state) {
		if (state == UG_STATE_ACTIVE && old_state < UG_STATE_ACTIVE) {
			// call ug_plugin_unref () by ug_plugin_aria2_thread ()
			ug_plugin_ref ((UgPlugin*) plugin);
			g_thread_create ((GThreadFunc) ug_plugin_aria2_thread, plugin, FALSE, NULL);
		}

		ug_plugin_post ((UgPlugin*) plugin, ug_message_new_state (state));
	}

	return UG_RESULT_OK;
}

static UgResult	ug_plugin_aria2_get_state (UgPluginAria2* plugin, UgState* state)
{
	if (state) {
		*state = plugin->state;
		return UG_RESULT_OK;
	}

	return UG_RESULT_ERROR;
}

UgResult	ug_plugin_aria2_set (UgPluginAria2* plugin, guint parameter, gpointer data)
{
	return UG_RESULT_UNSUPPORT;
}

static UgResult	ug_plugin_aria2_get (UgPluginAria2* plugin, guint parameter, gpointer data)
{
	UgProgress*	progress;

	if (parameter != UG_DATA_TYPE_INSTANCE)
		return UG_RESULT_UNSUPPORT;
	if (data == NULL || ((UgData*)data)->data_class != UgProgressClass)
		return UG_RESULT_UNSUPPORT;

	progress = data;
	progress->download_speed = (gdouble) plugin->downloadSpeed;
	progress->complete = plugin->completedLength;
	progress->total = plugin->totalLength;
	progress->consume_time = plugin->consumeTime;
	// If total size is unknown, don't calculate percent.
	if (progress->total)
		progress->percent = (gdouble) (progress->complete * 100 / progress->total);
	else
		progress->percent = 0;
	// If total size and average speed is unknown, don't calculate remain time.
	if (progress->download_speed > 0 && progress->total > 0)
		progress->remain_time = (progress->total - progress->complete) / progress->download_speed;

	return UG_RESULT_OK;
}

// ----------------------------------------------------------------------------
// thread function
//
static gpointer	ug_plugin_aria2_thread (UgPluginAria2* plugin)
{
	UgMessage*	message;
	gchar*		string;
	time_t		startingTime;
	gboolean	redirection;

	g_string_chunk_clear (plugin->chunk);
	startingTime = time (NULL);
	redirection  = TRUE;

	if (ug_plugin_aria2_add_uri (plugin) == FALSE)
		goto exit;

	do {
		plugin->consumeTime = (gdouble) (time(NULL) - startingTime);

		ug_plugin_delay ((UgPlugin*) plugin, 500);
		ug_plugin_aria2_tell_status (plugin);

		if (plugin->completedLength > 0)
			ug_plugin_post ((UgPlugin*)plugin, ug_message_new_progress ());
		if (redirection) {
			if (plugin->completedLength)
				redirection = FALSE;
			ug_plugin_aria2_get_servers (plugin);
		}

		switch (plugin->aria2Status) {
		case ARIA2_COMPLETE:
			goto exit;

		case ARIA2_ERROR:
			string = g_strdup_printf ("aria2 error code: %u", plugin->errorCode);
			message = ug_message_new_error (UG_MESSAGE_ERROR_CUSTOM, string);
			g_free (string);
			ug_plugin_post ((UgPlugin*) plugin, message);
			goto exit;
		}
	} while (plugin->state == UG_STATE_ACTIVE);

exit:
	ug_plugin_aria2_remove (plugin);

	if (plugin->state == UG_STATE_ACTIVE)
		ug_plugin_aria2_set_state (plugin, UG_STATE_READY);
	// call ug_plugin_ref () by ug_plugin_aria2_set_state ()
	ug_plugin_unref ((UgPlugin*) plugin);
	return NULL;
}


// ----------------------------------------------------------------------------
// aria2 methods
//
static gboolean	ug_plugin_aria2_response (UgPluginAria2* plugin, const gchar* method)
{
	UgXmlrpcResponse	response;
	UgMessage*			message;
	gchar*				temp;

	response = ug_xmlrpc_response (&plugin->xmlrpc);
	switch (response) {
	case UG_XMLRPC_ERROR:
		temp = g_strconcat (method, " response error", NULL);
		message = ug_message_new_error (UG_MESSAGE_ERROR_CUSTOM, temp);
		g_free (temp);
		break;

	case UG_XMLRPC_FAULT:
		temp = g_strconcat (method, " response fault", NULL);
		message = ug_message_new_error (UG_MESSAGE_ERROR_CUSTOM, temp);
		g_free (temp);
		break;

	case UG_XMLRPC_OK:
		message = NULL;
		break;
	}

	if (message) {
		ug_plugin_post ((UgPlugin*)plugin, message);
		return FALSE;
	}
	return TRUE;
}

static gboolean	ug_plugin_aria2_add_uri	(UgPluginAria2* plugin)
{
	UgXmlrpcValue*		uris;		// UG_XMLRPC_ARRAY
	UgXmlrpcValue*		options;	// UG_XMLRPC_STRUCT
	UgXmlrpcValue*		value;
	GString*			string;
	UgDataCommon*		common;
	gboolean			result;

	string = plugin->string;
	common = plugin->common;

	// URIs array
	uris = ug_xmlrpc_value_new_array (1);
	value = ug_xmlrpc_value_alloc (uris);
	value->type = UG_XMLRPC_STRING;
	value->c.string = common->url;
	// options struct
	options = ug_xmlrpc_value_new_struct (16);
	if (common->folder) {
		value = ug_xmlrpc_value_alloc (options);
		value->name = "dir";
		value->type = UG_XMLRPC_STRING;
		value->c.string = common->folder;
	}
	// max-tries
	value = ug_xmlrpc_value_alloc (options);
	value->name = "max-tries";
	value->type = UG_XMLRPC_STRING;
	g_string_printf (string, "%u", common->retry_limit);
	value->c.string = g_string_chunk_insert (plugin->chunk, string->str);
	// lowest-speed-limit
	value = ug_xmlrpc_value_alloc (options);
	value->name = "lowest-speed-limit";
	value->type = UG_XMLRPC_STRING;
	g_string_printf (string, "%u", 512);
	value->c.string = g_string_chunk_insert (plugin->chunk, string->str);
	// others
	ug_plugin_aria2_set_scheme (plugin, options);
	ug_plugin_aria2_set_proxy (plugin, options);
	ug_plugin_aria2_set_http (plugin, options);

	// aria2.addUri
	result = ug_xmlrpc_call (&plugin->xmlrpc,
			"aria2.addUri",
			UG_XMLRPC_ARRAY,  uris,
			UG_XMLRPC_STRUCT, options,
			UG_XMLRPC_NONE);
	// clear uris, options, and string chunk
	ug_xmlrpc_value_free (uris);
	ug_xmlrpc_value_free (options);
	g_string_chunk_clear (plugin->chunk);
	// message
	if (result == FALSE) {
		ug_plugin_post ((UgPlugin*)plugin,
				ug_message_new_error (UG_MESSAGE_ERROR_CUSTOM,
					"An error occurred during aria2.addUri"));
		return FALSE;
	}
	if (ug_plugin_aria2_response (plugin, "aria2.addUri") == FALSE)
		return FALSE;

	// get gid
	value = ug_xmlrpc_value_new ();
	ug_xmlrpc_get_value (&plugin->xmlrpc, value);
	plugin->gid = g_string_chunk_insert (plugin->chunk, value->c.string);
	ug_xmlrpc_value_free (value);

	return TRUE;
}

static gboolean	ug_plugin_aria2_remove	(UgPluginAria2* plugin)
{
	gboolean			result;

	result = ug_xmlrpc_call (&plugin->xmlrpc, "aria2.remove",
			UG_XMLRPC_STRING, plugin->gid,
			UG_XMLRPC_NONE);
	// message
	if (result == FALSE) {
		ug_plugin_post ((UgPlugin*)plugin,
				ug_message_new_error (UG_MESSAGE_ERROR_CUSTOM,
					"An error occurred during aria2.remove"));
		return FALSE;
	}
	if (ug_plugin_aria2_response (plugin, "aria2.remove") == FALSE)
		return FALSE;
	return TRUE;
}

static gboolean	ug_plugin_aria2_get_servers (UgPluginAria2* plugin)
{
	UgXmlrpcValue*		array;		// UG_XMLRPC_ARRAY
	UgXmlrpcValue*		servers;	// UG_XMLRPC_STRUCT
	UgXmlrpcValue*		member;
	gboolean			result;

	result = ug_xmlrpc_call (&plugin->xmlrpc, "aria2.getServers",
			UG_XMLRPC_STRING, plugin->gid,
			UG_XMLRPC_NONE);
	// message
	if (result == FALSE) {
		ug_plugin_post ((UgPlugin*)plugin,
				ug_message_new_error (UG_MESSAGE_ERROR_CUSTOM,
					"An error occurred during aria2.getServers"));
		return FALSE;
	}
	if (ug_plugin_aria2_response (plugin, "aria2.getServers") == FALSE)
		return FALSE;

	// get servers
	array = ug_xmlrpc_value_new ();
	ug_xmlrpc_get_value (&plugin->xmlrpc, array);
	if (array->type == UG_XMLRPC_ARRAY && array->len) {
		servers = ug_xmlrpc_value_at (array, 0);
		member = ug_xmlrpc_value_find (servers, "currentUri");
		if (member && strcmp (plugin->common->url, member->c.string) != 0) {
			g_free (plugin->common->url);
			plugin->common->url = g_strdup (member->c.string);
			ug_plugin_post ((UgPlugin*) plugin,
					ug_message_new_data (UG_MESSAGE_DATA_URL_CHANGED,
						member->c.string));

			g_print ("--- currentUri : %s\n", member->c.string);
		}
	}
	// clear
	ug_xmlrpc_value_free (array);

	return TRUE;
}

static gboolean	ug_plugin_aria2_tell_status	(UgPluginAria2* plugin)
{
	UgXmlrpcValue*		keys;		// UG_XMLRPC_ARRAY
	UgXmlrpcValue*		progress;	// UG_XMLRPC_STRUCT
	UgXmlrpcValue*		value;
	gboolean			result;

	// set keys array
	keys = ug_xmlrpc_value_new_array (5);
	value = ug_xmlrpc_value_alloc (keys);
	value->type = UG_XMLRPC_STRING;
	value->c.string = "status";
	value = ug_xmlrpc_value_alloc (keys);
	value->type = UG_XMLRPC_STRING;
	value->c.string = "totalLength";
	value = ug_xmlrpc_value_alloc (keys);
	value->type = UG_XMLRPC_STRING;
	value->c.string = "completedLength";
	value = ug_xmlrpc_value_alloc (keys);
	value->type = UG_XMLRPC_STRING;
	value->c.string = "downloadSpeed";
	value = ug_xmlrpc_value_alloc (keys);
	value->type = UG_XMLRPC_STRING;
	value->c.string = "errorCode";

	result = ug_xmlrpc_call (&plugin->xmlrpc,
			"aria2.tellStatus",
			UG_XMLRPC_STRING, plugin->gid,
			UG_XMLRPC_ARRAY,  keys,
			UG_XMLRPC_NONE);
	// clear keys array
	ug_xmlrpc_value_free (keys);
	// message
	if (result == FALSE) {
		ug_plugin_post ((UgPlugin*)plugin,
				ug_message_new_error (UG_MESSAGE_ERROR_CUSTOM,
					"An error occurred during aria2.tellStatus"));
		return FALSE;
	}
	if (ug_plugin_aria2_response (plugin, "aria2.tellStatus") == FALSE)
		return FALSE;

	// get progress struct
	progress = ug_xmlrpc_value_new_struct (5);
	ug_xmlrpc_get_value (&plugin->xmlrpc, progress);
	if (progress->type != UG_XMLRPC_STRUCT) {
		ug_xmlrpc_value_free (progress);
		return FALSE;
	}
	// status
	value = ug_xmlrpc_value_find (progress, "status");
	if (value && value->c.string) {
		switch (value->c.string[0]) {
		case 'a':	// active
			plugin->aria2Status = ARIA2_ACTIVE;
			break;

		case 'p':	// paused
			plugin->aria2Status = ARIA2_PAUSED;
			break;

		default:
		case 'w':	// waiting
			plugin->aria2Status = ARIA2_WAITING;
			break;

		case 'e':	// error
			plugin->aria2Status = ARIA2_ERROR;
			break;

		case 'c':	// complete
			plugin->aria2Status = ARIA2_COMPLETE;
			break;

		case 'r':	// removed
			plugin->aria2Status = ARIA2_REMOVED;
			break;
		}
	}
	// errorCode
	value = ug_xmlrpc_value_find (progress, "errorCode");
	plugin->errorCode = ug_xmlrpc_value_get_int (value);
	// totalLength
	value = ug_xmlrpc_value_find (progress, "totalLength");
	plugin->totalLength = ug_xmlrpc_value_get_int64 (value);
	// completedLength
	value = ug_xmlrpc_value_find (progress, "completedLength");
	plugin->completedLength = ug_xmlrpc_value_get_int64 (value);
	// downloadSpeed
	value = ug_xmlrpc_value_find (progress, "downloadSpeed");
	plugin->downloadSpeed = ug_xmlrpc_value_get_int (value);

	// clear progress struct
	ug_xmlrpc_value_free (progress);

	return TRUE;
}


// ----------------------------------------------------------------------------
// aria2 setup functions
//
static void	ug_plugin_aria2_set_scheme (UgPluginAria2* plugin, UgXmlrpcValue* options)
{
	UgXmlrpcValue*	value;
	UgDataCommon*	common;
	UgDataHttp*		http;
	UgDataFtp*		ftp;
	gchar*			user;
	gchar*			password;

	common = plugin->common;
	http   = plugin->http;
	ftp    = plugin->ftp;
	user     = NULL;
	password = NULL;

	if (http && (http->user || http->password) ) {
		user     = http->user     ? http->user     : "";
		password = http->password ? http->password : "";
		// http-user
		value = ug_xmlrpc_value_alloc (options);
		value->name = "http-user";
		value->type = UG_XMLRPC_STRING;
		value->c.string = user;
		// http-password
		value = ug_xmlrpc_value_alloc (options);
		value->name = "http-password";
		value->type = UG_XMLRPC_STRING;
		value->c.string = password;
	}

	if (ftp && (ftp->user || ftp->password)) {
		user     = ftp->user     ? ftp->user     : "";
		password = ftp->password ? ftp->password : "";
		// ftp-user
		value = ug_xmlrpc_value_alloc (options);
		value->name = "ftp-user";
		value->type = UG_XMLRPC_STRING;
		value->c.string = user;
		// ftp-password
		value = ug_xmlrpc_value_alloc (options);
		value->name = "ftp-password";
		value->type = UG_XMLRPC_STRING;
		value->c.string = password;
	}

	if (user == NULL) {
		if (common->user || common->password) {
			user     = common->user     ? common->user     : "";
			password = common->password ? common->password : "";
			if (g_ascii_strncasecmp (common->url, "http", 4) == 0) {
				// http-user
				value = ug_xmlrpc_value_alloc (options);
				value->name = "http-user";
				value->type = UG_XMLRPC_STRING;
				value->c.string = user;
				// http-password
				value = ug_xmlrpc_value_alloc (options);
				value->name = "http-password";
				value->type = UG_XMLRPC_STRING;
				value->c.string = password;
			}
			else if (g_ascii_strncasecmp (common->url, "ftp", 3) == 0) {
				// ftp-user
				value = ug_xmlrpc_value_alloc (options);
				value->name = "ftp-user";
				value->type = UG_XMLRPC_STRING;
				value->c.string = user;
				// ftp-password
				value = ug_xmlrpc_value_alloc (options);
				value->name = "ftp-password";
				value->type = UG_XMLRPC_STRING;
				value->c.string = password;
			}
		}
	}
}

static void	ug_plugin_aria2_set_http (UgPluginAria2* plugin, UgXmlrpcValue* options)
{
	UgXmlrpcValue*	value;
	UgDataHttp*		http;

	http = plugin->http;

	if (http) {
		if (http->referrer) {
			value = ug_xmlrpc_value_alloc (options);
			value->name = "referer";
			value->type = UG_XMLRPC_STRING;
			value->c.string = http->referrer;
		}
		if (http->cookie_file) {
			value = ug_xmlrpc_value_alloc (options);
			value->name = "load-cookies";
			value->type = UG_XMLRPC_STRING;
			value->c.string = http->cookie_file;
		}
		if (http->user_agent) {
			value = ug_xmlrpc_value_alloc (options);
			value->name = "user-agent";
			value->type = UG_XMLRPC_STRING;
			value->c.string = http->user_agent;
		}
	}
}

static gboolean	ug_plugin_aria2_set_proxy (UgPluginAria2* plugin, UgXmlrpcValue* options)
{
	UgXmlrpcValue*	value;
	UgDataProxy*	proxy;

	proxy = plugin->proxy;
	if (proxy == NULL)
		return TRUE;

#ifdef HAVE_LIBPWMD
	if (proxy->type == UG_DATA_PROXY_PWMD) {
		if (ug_plugin_aria2_set_proxy_pwmd (plugin, options) == FALSE)
			return FALSE;
	}
#endif

	// host
	if (proxy->host) {
		value = ug_xmlrpc_value_alloc (options);
		value->name = "all-proxy";
		value->type = UG_XMLRPC_STRING;
		if (proxy->port)
			value->c.string = proxy->host;
		else {
			g_string_printf (plugin->string, "%s:%u", proxy->host, proxy->port);
			value->c.string = g_string_chunk_insert (plugin->chunk, plugin->string->str);
		}

		// proxy user and password
		if (proxy->user || proxy->password) {
			// user
			value = ug_xmlrpc_value_alloc (options);
			value->name = "all-proxy-user";
			value->type = UG_XMLRPC_STRING;
			value->c.string = proxy->user ? proxy->user : "";
			// password
			value = ug_xmlrpc_value_alloc (options);
			value->name = "all-proxy-password";
			value->type = UG_XMLRPC_STRING;
			value->c.string = proxy->password ? proxy->password : "";
		}
	}

	return TRUE;
}


// ----------------------------------------------------------------------------
// PWMD
//
#ifdef HAVE_LIBPWMD
static gboolean	ug_plugin_aria2_set_proxy_pwmd (UgPluginAria2 *plugin, UgXmlrpcValue* options)
{
    gpg_error_t rc;
    gchar *result, *hostname = NULL, *username = NULL, *password = NULL,
	  *type = NULL;
    gint port = 80;
    gchar *path = NULL;
    gint i;
    UgMessage *message;

    if (plugin->proxy->pwmd.element) {
	path = g_strdup_printf("%s\t", plugin->proxy->pwmd.element);

	for (i = 0; i < strlen(path); i++) {
	    if (path[i] == '^')
		path[i] = '\t';
	}
    }

    pwmd_init();
    pwm_t *pwm = pwmd_new("uget");
    rc = pwmd_connect(pwm, plugin->proxy->pwmd.socket);

    if (rc)
	goto fail;

    rc = pwmd_open(pwm, plugin->proxy->pwmd.file);

    if (rc)
	goto fail;

    rc = pwmd_command(pwm, &result, "GET %stype", path ? path : "");

    if (rc)
	goto fail;

    type = result;
    rc = pwmd_command(pwm, &result, "GET %shostname", path ? path : "");

    if (rc)
	goto fail;

    hostname = result;
    rc = pwmd_command(pwm, &result, "GET %sport", path ? path : "");

    if (rc && rc != GPG_ERR_ELEMENT_NOT_FOUND)
	goto fail;

    port = atoi(result);
    pwmd_free(result);
    rc = pwmd_command(pwm, &result, "GET %susername", path ? path : "");

    if (rc && rc != GPG_ERR_ELEMENT_NOT_FOUND)
	goto fail;

    if (!rc)
	username = result;

    rc = pwmd_command(pwm, &result, "GET %spassword", path ? path : "");

    if (rc && rc != GPG_ERR_ELEMENT_NOT_FOUND)
	goto fail;

    if (!rc)
	password = result;

    // proxy host and port
	// host
	value = ug_xmlrpc_value_alloc (options);
	value->name = "all-proxy";
	value->type = UG_XMLRPC_STRING;
	if (port == 0)
		value->c.string = g_string_chunk_insert (plugin->chunk, hostname);
	else {
		g_string_printf (plugin->string, "%s:%u", hostname, port);
		value->c.string = g_string_chunk_insert (plugin->chunk, plugin->string->str);
	}

	// proxy user and password
	if (username || password) {
		// user
		value = ug_xmlrpc_value_alloc (options);
		value->name = "all-proxy-user";
		value->type = UG_XMLRPC_STRING;
		value->c.string = g_string_chunk_insert (plugin->chunk, username ? username : "");
		// password
		value = ug_xmlrpc_value_alloc (options);
		value->name = "all-proxy-password";
		value->type = UG_XMLRPC_STRING;
		value->c.string = g_string_chunk_insert (plugin->chunk, password ? password : "");
	}

    pwmd_free(type);
    pwmd_free(hostname);

    if (username)
	pwmd_free(username);

    if (password)
	pwmd_free(password);

    if (path)
	g_free(path);

    pwmd_close(pwm);
    return TRUE;

fail:
    if (type)
	pwmd_free(type);

    if (hostname)
	pwmd_free(hostname);

    if (username)
	pwmd_free(username);

    if (password)
	pwmd_free(password);

    if (path)
	g_free(path);

    if (pwm)
	pwmd_close(pwm);

    gchar *e = g_strdup_printf("Pwmd ERR %i: %s", rc, pwmd_strerror(rc));
    message = ug_message_new_error (UG_MESSAGE_ERROR_CUSTOM, e);
    ug_plugin_post ((UgPlugin*) plugin, message);
    fprintf(stderr, "%s\n", e);
    g_free(e);
    return FALSE;
}

#endif	// HAVE_LIBPWMD


// ----------------------------------------------------------------------------
// utility
//
static gint64	ug_xmlrpc_value_get_int64 (UgXmlrpcValue* value)
{
	if (value) {
		switch (value->type) {
		case UG_XMLRPC_STRING:
#ifdef _WIN32
			return _atoi64 (value->c.string);
#else
			return atoll (value->c.string);
#endif
		case UG_XMLRPC_INT64:
			return value->c.int64;

		case UG_XMLRPC_DOUBLE:
			return (gint64) value->c.double_;

		default:
			break;
		}
	}
	return 0;
}

static int	ug_xmlrpc_value_get_int (UgXmlrpcValue* value)
{
	if (value) {
		switch (value->type) {
		case UG_XMLRPC_STRING:
			return atoi (value->c.string);

		case UG_XMLRPC_INT:
			return value->c.int_;

		case UG_XMLRPC_DOUBLE:
			return (int) value->c.double_;

		default:
			break;
		}
	}
	return 0;
}

#endif	// HAVE_PLUGIN_ARIA2

