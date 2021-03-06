###############################################################################
# Name: wxcompat.py                                                           #
# Purpose: Help with compatibility between wx versions.                       #
# Author: Cody Precord <cprecord@editra.org>                                  #
# Copyright: (c) 2008 Cody Precord <staff@editra.org>                         #
# License: wxWindows License                                                  #
###############################################################################

"""
@summary: wx Compatibility helper module

"""

__author__ = "Cody Precord <cprecord@editra.org>"
__svnid__ = "$Id$"
__revision__ = "$Revision$"

#-----------------------------------------------------------------------------#
# Imports
import wx

#-----------------------------------------------------------------------------#

if wx.Platform == '__WXMAC__' and not hasattr(wx, 'MacThemeColour'):
    def MacThemeColour(theme_id):
        """Get a specified Mac theme colour
        @param theme_id: Carbon theme id
        @return: wx.Colour

        """
        brush = wx.Brush(wx.Colour(0, 0, 0))
        brush.MacSetTheme(theme_id)
        return brush.GetColour()

    wx.MacThemeColour = MacThemeColour

# GetText is not available in 2.9 but GetItemLabel is not available pre 2.8.6
if wx.VERSION < (2, 8, 6, 0, ''):
    wx.MenuItem.GetItemLabel = wx.MenuItem.GetText
    wx.MenuItem.GetItemLabelText = wx.MenuItem.GetLabel

# HACK temporary bandaid to allow the the program to run until a replacement
#      for this method can be found.
if wx.VERSION >= (2, 9, 0, 0, ''):
    wx.Brush.MacSetTheme = lambda x, y: x

