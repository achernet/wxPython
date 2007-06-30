## This file reverse renames symbols in the wx package to give
## them their wx prefix again, for backwards compatibility.
##
## Generated by BuildRenamers in config.py

# This silly stuff here is so the wxPython.wx module doesn't conflict
# with the wx package.  We need to import modules from the wx package
# here, then we'll put the wxPython.wx entry back in sys.modules.
import sys
_wx = None
if sys.modules.has_key('wxPython.wx'):
    _wx = sys.modules['wxPython.wx']
    del sys.modules['wxPython.wx']

import wx.webkit

sys.modules['wxPython.wx'] = _wx
del sys, _wx


# Now assign all the reverse-renamed names:
wxWebKitNameStr = wx.webkit.WebKitNameStr
wxWebKitCtrl = wx.webkit.WebKitCtrl
wxWebKitCtrlPtr = wx.webkit.WebKitCtrlPtr
wxPreWebKitCtrl = wx.webkit.PreWebKitCtrl
wxWEBKIT_STATE_START = wx.webkit.WEBKIT_STATE_START
wxWEBKIT_STATE_NEGOTIATING = wx.webkit.WEBKIT_STATE_NEGOTIATING
wxWEBKIT_STATE_REDIRECTING = wx.webkit.WEBKIT_STATE_REDIRECTING
wxWEBKIT_STATE_TRANSFERRING = wx.webkit.WEBKIT_STATE_TRANSFERRING
wxWEBKIT_STATE_STOP = wx.webkit.WEBKIT_STATE_STOP
wxWEBKIT_STATE_FAILED = wx.webkit.WEBKIT_STATE_FAILED
wxEVT_WEBKIT_STATE_CHANGED = wx.webkit.wxEVT_WEBKIT_STATE_CHANGED
wxWebKitStateChangedEvent = wx.webkit.WebKitStateChangedEvent
wxWebKitStateChangedEventPtr = wx.webkit.WebKitStateChangedEventPtr


d = globals()
for k, v in wx.webkit.__dict__.iteritems():
    if k.startswith('EVT'):
        d[k] = v
del d, k, v



