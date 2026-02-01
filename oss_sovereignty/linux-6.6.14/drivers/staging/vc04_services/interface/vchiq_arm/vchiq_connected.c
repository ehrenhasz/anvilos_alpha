
 

#include "vchiq_connected.h"
#include "vchiq_core.h"
#include <linux/module.h>
#include <linux/mutex.h>

#define  MAX_CALLBACKS  10

static   int                        g_connected;
static   int                        g_num_deferred_callbacks;
static   void (*g_deferred_callback[MAX_CALLBACKS])(void);
static   int                        g_once_init;
static   DEFINE_MUTEX(g_connected_mutex);

 
static void connected_init(void)
{
	if (!g_once_init)
		g_once_init = 1;
}

 
void vchiq_add_connected_callback(void (*callback)(void))
{
	connected_init();

	if (mutex_lock_killable(&g_connected_mutex))
		return;

	if (g_connected) {
		 
		callback();
	} else {
		if (g_num_deferred_callbacks >= MAX_CALLBACKS) {
			vchiq_log_error(vchiq_core_log_level,
					"There already %d callback registered - please increase MAX_CALLBACKS",
					g_num_deferred_callbacks);
		} else {
			g_deferred_callback[g_num_deferred_callbacks] =
				callback;
			g_num_deferred_callbacks++;
		}
	}
	mutex_unlock(&g_connected_mutex);
}
EXPORT_SYMBOL(vchiq_add_connected_callback);

 
void vchiq_call_connected_callbacks(void)
{
	int i;

	connected_init();

	if (mutex_lock_killable(&g_connected_mutex))
		return;

	for (i = 0; i <  g_num_deferred_callbacks; i++)
		g_deferred_callback[i]();

	g_num_deferred_callbacks = 0;
	g_connected = 1;
	mutex_unlock(&g_connected_mutex);
}
