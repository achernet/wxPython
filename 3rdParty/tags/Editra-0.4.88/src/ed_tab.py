###############################################################################
# Name: ed_tab.py                                                             #
# Purpose: Notebook tab inteface class                                        #
# Author: Cody Precord <cprecord@editra.org>                                  #
# Copyright: (c) 2008 Cody Precord <staff@editra.org>                         #
# License: wxWindows License                                                  #
###############################################################################

"""
Base class for all views that want to be able to be viewable in the main
notebook. 

@summary: Main notebook tab base class

"""

__author__ = "Cody Precord <cprecord@editra.org>"
__svnid__ = "$Id$"
__revision__ = "$Revision$"

#--------------------------------------------------------------------------#
# Imports


#--------------------------------------------------------------------------#

class EdTabBase(object):
    """Base class for all tab views to derive from, this class just defines
    the abstract interface and some common basic methods. Initialize this
    base class after initializing the wx control instance of the subclass.

    """
    def __init__(self):
        """Initialize the tab base class"""
        object.__init__(self)

        # Attributes
        self._lbl = u''
        self._idx = -1

    @property
    def _nb(self):
        """Get the notebook that owns this tab"""
        return self.GetParent()

    #---- Methods to override in subclasses ----#

    def DoDeactivateTab(self):
        """Called when the tab is moved from the foreground to background"""
        pass

    def DoOnIdle(self):
        """Called when the notebook is idle and this instance is the active
        tab.

        """
        pass

    def DoTabClosing(self):
        """Called when the tab has been selected to be closed in the notebook"""
        pass

    def DoTabOpen(self, ):
        """Called to open a new tab"""
        pass

    def DoTabSelected(self):
        """Called when the page is selected in the notebook"""
        pass

    def GetName(self):
        """Get the unique name for this tab control.
        @return: (unicode) string

        """
        raise NotImplementedError, "GetName Must be implemented!!"

    def GetTabMenu(self):
        """Get the context menu to show on the tab
        @return: wx.Menu or None

        """
        return None

    def GetTitleString(self):
        """Get the title string to display in the MainWindows title bar
        @return: (unicode) string

        """
        return u''

    def CanCloseTab(self):
        """Called when checking if tab can be closed or not
        @return: bool

        """
        return True

    def OnTabMenu(self, evt):
        """Handle events from this tabs menu
        @param evt: menu event

        """
        evt.Skip()

    #---- Common Base Methods ----#

    def GetTabIndex(self):
        """Return the index of the tab in the notebook
        @return: int

        """
        return self._idx

    def GetTabLabel(self):
        """Get the tabs label
        @return: string

        """
        return self._lbl

    def SetTabIndex(self, idx):
        """Set the tab index
        @param idx: int

        """
        self._idx = idx

    def SetTabLabel(self, lbl):
        """Set the tabs label
        @param lbl: string

        """
        self._lbl = lbl

    def SetTabTitle(self, title):
        """Set the notebooks title text for this tab"""
        obj_id = self.GetId()

        # Find which page we are and update the text
        for page in range(self._nb.GetPageCount()):
            ctrl = self._nb.GetPage(page)
            if ctrl.GetId() == obj_id:
                self._nb.SetPageText(title)
                break
        else:
            # TODO: notify of error?
            pass

#--------------------------------------------------------------------------#
