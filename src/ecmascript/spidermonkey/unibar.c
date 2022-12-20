/* The SpiderMonkey location and history objects implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey/unibar.h"
#include "ecmascript/spidermonkey/window.h"
#include "intl/gettext/libintl.h"
#include "main/select.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

static JSBool unibar_get_property_visible(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSMutableHandleValue hvp);
static JSBool unibar_set_property_visible(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSBool strict, JSMutableHandleValue hvp);

/* Each @menubar_class object must have a @window_class parent.  */
JSClass menubar_class = {
	"menubar",
	JSCLASS_HAS_PRIVATE,	/* const char * "t" */
	JS_PropertyStub, JS_PropertyStub,
	JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL
};
/* Each @statusbar_class object must have a @window_class parent.  */
JSClass statusbar_class = {
	"statusbar",
	JSCLASS_HAS_PRIVATE,	/* const char * "s" */
	JS_PropertyStub, JS_PropertyStub,
	JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL
};

/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (even though this object has no array elements).
 * ECMAScript code should not use these directly as in menubar[-1];
 * future versions of ELinks may change the numbers.  */
enum unibar_prop {
	JSP_UNIBAR_VISIBLE = -1,
};
JSPropertySpec unibar_props[] = {
	{ "visible",	0,	JSPROP_ENUMERATE|JSPROP_SHARED, JSOP_WRAPPER(unibar_get_property_visible), JSOP_WRAPPER(unibar_set_property_visible) },
	{ NULL }
};



static JSBool
unibar_get_property_visible(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct session_status *status;
	unsigned char *bar;

	/* This can be called if @obj if not itself an instance of either
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, &menubar_class, NULL)
	 && !JS_InstanceOf(ctx, obj, &statusbar_class, NULL))
		return JS_FALSE;
	parent_win = JS_GetParent(obj);
	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	doc_view = vs->doc_view;
	status = &doc_view->session->status;
	bar = JS_GetPrivate(obj); /* from @menubar_class or @statusbar_class */

#define unibar_fetch(bar) \
	boolean_to_jsval(ctx, vp, status->force_show_##bar##_bar >= 0 \
	          ? status->force_show_##bar##_bar \
	          : status->show_##bar##_bar)
	switch (*bar) {
	case 's':
		unibar_fetch(status);
		break;
	case 't':
		unibar_fetch(title);
		break;
	default:
		boolean_to_jsval(ctx, vp, 0);
		break;
	}
#undef unibar_fetch

	return JS_TRUE;
}

static JSBool
unibar_set_property_visible(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSBool strict, JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct session_status *status;
	unsigned char *bar;

	/* This can be called if @obj if not itself an instance of either
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, &menubar_class, NULL)
	 && !JS_InstanceOf(ctx, obj, &statusbar_class, NULL))
		return JS_FALSE;
	parent_win = JS_GetParent(obj);
	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	doc_view = vs->doc_view;
	status = &doc_view->session->status;
	bar = JS_GetPrivate(obj); /* from @menubar_class or @statusbar_class */

	switch (*bar) {
	case 's':
		status->force_show_status_bar = jsval_to_boolean(ctx, vp);
		break;
	case 't':
		status->force_show_title_bar = jsval_to_boolean(ctx, vp);
		break;
	default:
		break;
	}
	register_bottom_half(update_status, NULL);

	return JS_TRUE;
}
