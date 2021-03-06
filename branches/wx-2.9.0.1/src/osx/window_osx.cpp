/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/window.cpp
// Purpose:     wxWindowMac
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: window.cpp 54981 2008-08-05 17:52:02Z SC $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/window.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/panel.h"
    #include "wx/frame.h"
    #include "wx/dc.h"
    #include "wx/dcclient.h"
    #include "wx/button.h"
    #include "wx/menu.h"
    #include "wx/dialog.h"
    #include "wx/settings.h"
    #include "wx/msgdlg.h"
    #include "wx/scrolbar.h"
    #include "wx/statbox.h"
    #include "wx/textctrl.h"
    #include "wx/toolbar.h"
    #include "wx/layout.h"
    #include "wx/statusbr.h"
    #include "wx/menuitem.h"
    #include "wx/treectrl.h"
    #include "wx/listctrl.h"
#endif

#include "wx/tooltip.h"
#include "wx/spinctrl.h"
#include "wx/geometry.h"

#if wxUSE_LISTCTRL
    #include "wx/listctrl.h"
#endif

#if wxUSE_TREECTRL
    #include "wx/treectrl.h"
#endif

#if wxUSE_CARET
    #include "wx/caret.h"
#endif

#if wxUSE_POPUPWIN
    #include "wx/popupwin.h"
#endif

#if wxUSE_DRAG_AND_DROP
#include "wx/dnd.h"
#endif

#include "wx/graphics.h"

#if wxOSX_USE_CARBON
#include "wx/osx/uma.h"
#else
#include "wx/osx/private.h"
// bring in themeing
#include <Carbon/Carbon.h>
#endif

#define MAC_SCROLLBAR_SIZE 15
#define MAC_SMALL_SCROLLBAR_SIZE 11

#include <string.h>

#ifdef __WXUNIVERSAL__
    IMPLEMENT_ABSTRACT_CLASS(wxWindowMac, wxWindowBase)
#else
    IMPLEMENT_DYNAMIC_CLASS(wxWindow, wxWindowBase)
#endif

BEGIN_EVENT_TABLE(wxWindowMac, wxWindowBase)
    EVT_NC_PAINT(wxWindowMac::OnNcPaint)
    EVT_ERASE_BACKGROUND(wxWindowMac::OnEraseBackground)
    EVT_MOUSE_EVENTS(wxWindowMac::OnMouseEvent)
END_EVENT_TABLE()

#define wxMAC_DEBUG_REDRAW 0
#ifndef wxMAC_DEBUG_REDRAW
#define wxMAC_DEBUG_REDRAW 0
#endif

// ===========================================================================
// implementation
// ===========================================================================

// ----------------------------------------------------------------------------
 // constructors and such
// ----------------------------------------------------------------------------

wxWindowMac::wxWindowMac()
{
    Init();
}

wxWindowMac::wxWindowMac(wxWindowMac *parent,
            wxWindowID id,
            const wxPoint& pos ,
            const wxSize& size ,
            long style ,
            const wxString& name )
{
    Init();
    Create(parent, id, pos, size, style, name);
}

void wxWindowMac::Init()
{
    m_peer = NULL ;
    m_macAlpha = 255 ;
    m_cgContextRef = NULL ;

    // as all windows are created with WS_VISIBLE style...
    m_isShown = true;

    m_hScrollBar = NULL ;
    m_vScrollBar = NULL ;
    m_hScrollBarAlwaysShown = false;
    m_vScrollBarAlwaysShown = false;

    m_macIsUserPane = true;
    m_clipChildren = false ;
    m_cachedClippedRectValid = false ;
}

wxWindowMac::~wxWindowMac()
{
    SendDestroyEvent();

    MacInvalidateBorders() ;

#ifndef __WXUNIVERSAL__
    // VS: make sure there's no wxFrame with last focus set to us:
    for ( wxWindow *win = GetParent(); win; win = win->GetParent() )
    {
        wxFrame *frame = wxDynamicCast(win, wxFrame);
        if ( frame )
        {
            if ( frame->GetLastFocus() == this )
                frame->SetLastFocus(NULL);
            break;
        }
    }
#endif

    // destroy children before destroying this window itself
    DestroyChildren();

    // wxRemoveMacControlAssociation( this ) ;
    // If we delete an item, we should initialize the parent panel,
    // because it could now be invalid.
    wxTopLevelWindow *tlw = wxDynamicCast(wxGetTopLevelParent((wxWindow*)this), wxTopLevelWindow);
    if ( tlw )
    {
        if ( tlw->GetDefaultItem() == (wxButton*) this)
            tlw->SetDefaultItem(NULL);
    }

    if ( g_MacLastWindow == this )
        g_MacLastWindow = NULL ;

#ifndef __WXUNIVERSAL__
    wxFrame* frame = wxDynamicCast( wxGetTopLevelParent( (wxWindow*)this ) , wxFrame ) ;
    if ( frame )
    {
        if ( frame->GetLastFocus() == this )
            frame->SetLastFocus( NULL ) ;
    }
#endif

    // delete our drop target if we've got one
#if wxUSE_DRAG_AND_DROP
    if ( m_dropTarget != NULL )
    {
        delete m_dropTarget;
        m_dropTarget = NULL;
    }
#endif

    delete m_peer ;
}

WXWidget wxWindowMac::GetHandle() const
{
    if ( m_peer )
        return (WXWidget) m_peer->GetWXWidget() ;
    return NULL;
}

// ---------------------------------------------------------------------------
// Utility Routines to move between different coordinate systems
// ---------------------------------------------------------------------------

/*
 * Right now we have the following setup :
 * a border that is not part of the native control is always outside the
 * control's border (otherwise we loose all native intelligence, future ways
 * may be to have a second embedding control responsible for drawing borders
 * and backgrounds eventually)
 * so all this border calculations have to be taken into account when calling
 * native methods or getting native oriented data
 * so we have three coordinate systems here
 * wx client coordinates
 * wx window coordinates (including window frames)
 * native coordinates
 */

//
//

// Constructor
bool wxWindowMac::Create(wxWindowMac *parent,
    wxWindowID id,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name)
{
    wxCHECK_MSG( parent, false, wxT("can't create wxWindowMac without parent") );

    if ( !CreateBase(parent, id, pos, size, style, wxDefaultValidator, name) )
        return false;

    m_windowVariant = parent->GetWindowVariant() ;

    if ( m_macIsUserPane )
    {
        m_peer = wxWidgetImpl::CreateUserPane( this, parent, id, pos, size , style, GetExtraStyle() );
        MacPostControlCreate(pos, size) ;
    }

#ifndef __WXUNIVERSAL__
    // Don't give scrollbars to wxControls unless they ask for them
    if ( (! IsKindOf(CLASSINFO(wxControl)) && ! IsKindOf(CLASSINFO(wxStatusBar)))
         || (IsKindOf(CLASSINFO(wxControl)) && ((style & wxHSCROLL) || (style & wxVSCROLL))))
    {
        MacCreateScrollBars( style ) ;
    }
#endif

    wxWindowCreateEvent event((wxWindow*)this);
    GetEventHandler()->AddPendingEvent(event);

    return true;
}

void wxWindowMac::MacChildAdded()
{
    if ( m_vScrollBar )
        m_vScrollBar->Raise() ;
    if ( m_hScrollBar )
        m_hScrollBar->Raise() ;
}

void wxWindowMac::MacPostControlCreate(const wxPoint& WXUNUSED(pos), const wxSize& size)
{
    wxASSERT_MSG( m_peer != NULL && m_peer->IsOk() , wxT("No valid mac control") ) ;

    GetParent()->AddChild( this );

    m_peer->InstallEventHandler();
    m_peer->Embed(GetParent()->GetPeer());

    GetParent()->MacChildAdded() ;

    // adjust font, controlsize etc
    DoSetWindowVariant( m_windowVariant ) ;

    m_peer->SetLabel( wxStripMenuCodes(m_label, wxStrip_Mnemonics), GetFont().GetEncoding() ) ;

    // for controls we want to use best size for wxDefaultSize params )
    if ( !m_macIsUserPane )
        SetInitialSize(size);

    SetCursor( *wxSTANDARD_CURSOR ) ;
}

void wxWindowMac::DoSetWindowVariant( wxWindowVariant variant )
{
    // Don't assert, in case we set the window variant before
    // the window is created
    // wxASSERT( m_peer->Ok() ) ;

    m_windowVariant = variant ;

    if (m_peer == NULL || !m_peer->IsOk())
        return;

    m_peer->SetControlSize( variant );
#if wxOSX_USE_CARBON
    ControlSize size ;

    // we will get that from the settings later
    // and make this NORMAL later, but first
    // we have a few calculations that we must fix

    switch ( variant )
    {
        case wxWINDOW_VARIANT_NORMAL :
            size = kControlSizeNormal;
            break ;

        case wxWINDOW_VARIANT_SMALL :
            size = kControlSizeSmall;
            break ;

        case wxWINDOW_VARIANT_MINI :
            // not always defined in the headers
            size = 3 ;
            break ;

        case wxWINDOW_VARIANT_LARGE :
            size = kControlSizeLarge;
            break ;

        default:
            wxFAIL_MSG(_T("unexpected window variant"));
            break ;
    }
    m_peer->SetData<ControlSize>(kControlEntireControl, kControlSizeTag, &size ) ;
#endif

    wxFont font ;

    wxOSXSystemFont systemFont = wxOSX_SYSTEM_FONT_NORMAL ;

    switch ( variant )
    {
        case wxWINDOW_VARIANT_NORMAL :
            systemFont = wxOSX_SYSTEM_FONT_NORMAL ;
            break ;

        case wxWINDOW_VARIANT_SMALL :
            systemFont = wxOSX_SYSTEM_FONT_SMALL ;
            break ;

        case wxWINDOW_VARIANT_MINI :
            systemFont = wxOSX_SYSTEM_FONT_MINI ;
            break ;

        case wxWINDOW_VARIANT_LARGE :
            systemFont = wxOSX_SYSTEM_FONT_NORMAL ;
            break ;

        default:
            wxFAIL_MSG(_T("unexpected window variant"));
            break ;
    }

    font.CreateSystemFont( systemFont ) ;

    SetFont( font ) ;
}

void wxWindowMac::MacUpdateControlFont()
{
    if ( m_peer )
        m_peer->SetFont( GetFont() , GetForegroundColour() , GetWindowStyle() ) ;

    // do not trigger refreshes upon invisible and possible partly created objects
    if ( IsShownOnScreen() )
        Refresh() ;
}

bool wxWindowMac::SetFont(const wxFont& font)
{
    bool retval = wxWindowBase::SetFont( font );

    MacUpdateControlFont() ;

    return retval;
}

bool wxWindowMac::SetForegroundColour(const wxColour& col )
{
    bool retval = wxWindowBase::SetForegroundColour( col );

    if (retval)
        MacUpdateControlFont();

    return retval;
}

bool wxWindowMac::SetBackgroundColour(const wxColour& col )
{
    if ( !wxWindowBase::SetBackgroundColour(col) && m_hasBgCol )
        return false ;

    if ( m_peer )
        m_peer->SetBackgroundColour( col ) ;

    return true ;
}

static bool wxIsWindowOrParentDisabled(wxWindow* w)
{
    while (w && !w->IsTopLevel())
    {
        if (!w->IsEnabled())
            return true;
        w = w->GetParent();
    }
    return false;
}

void wxWindowMac::SetFocus()
{
    if ( !AcceptsFocus() )
            return ;

    if (wxIsWindowOrParentDisabled((wxWindow*) this))
        return;

    wxWindow* former = FindFocus() ;
    if ( former == this )
        return ;

    m_peer->SetFocus() ;
}

void wxWindowMac::DoCaptureMouse()
{
    wxApp::s_captureWindow = (wxWindow*) this ;
    m_peer->CaptureMouse() ;
}

wxWindow * wxWindowBase::GetCapture()
{
    return wxApp::s_captureWindow ;
}

void wxWindowMac::DoReleaseMouse()
{
    wxApp::s_captureWindow = NULL ;

    m_peer->ReleaseMouse() ;
}

#if wxUSE_DRAG_AND_DROP

void wxWindowMac::SetDropTarget(wxDropTarget *pDropTarget)
{
    delete m_dropTarget;

    m_dropTarget = pDropTarget;
    if ( m_dropTarget != NULL )
    {
        // TODO:
    }
}

#endif

// Old-style File Manager Drag & Drop
void wxWindowMac::DragAcceptFiles(bool WXUNUSED(accept))
{
    // TODO:
}

// From a wx position / size calculate the appropriate size of the native control

bool wxWindowMac::MacGetBoundsForControl(
    const wxPoint& pos,
    const wxSize& size,
    int& x, int& y,
    int& w, int& h , bool adjustOrigin ) const
{
    // the desired size, minus the border pixels gives the correct size of the control
    x = (int)pos.x;
    y = (int)pos.y;

    w = WidthDefault( size.x ); 
    h = HeightDefault( size.y ); 

    x += MacGetLeftBorderSize() ;
    y += MacGetTopBorderSize() ;
    w -= MacGetLeftBorderSize() + MacGetRightBorderSize() ;
    h -= MacGetTopBorderSize() + MacGetBottomBorderSize() ;

    if ( adjustOrigin )
        AdjustForParentClientOrigin( x , y ) ;

    // this is in window relative coordinate, as this parent may have a border, its physical position is offset by this border
    if ( GetParent() && !GetParent()->IsTopLevel() )
    {
        x -= GetParent()->MacGetLeftBorderSize() ;
        y -= GetParent()->MacGetTopBorderSize() ;
    }

    return true ;
}

// Get window size (not client size)
void wxWindowMac::DoGetSize(int *x, int *y) const
{
    int width, height;
    m_peer->GetSize( width, height );

    if (x)
       *x = width + MacGetLeftBorderSize() + MacGetRightBorderSize() ;
    if (y)
       *y = height + MacGetTopBorderSize() + MacGetBottomBorderSize() ;
}

// get the position of the bounds of this window in client coordinates of its parent
void wxWindowMac::DoGetPosition(int *x, int *y) const
{
    int x1, y1;
    
    m_peer->GetPosition( x1, y1 ) ;

    // get the wx window position from the native one
    x1 -= MacGetLeftBorderSize() ;
    y1 -= MacGetTopBorderSize() ;

    if ( !IsTopLevel() )
    {
        wxWindow *parent = GetParent();
        if ( parent )
        {
            // we must first adjust it to be in window coordinates of the parent,
            // as otherwise it gets lost by the ClientAreaOrigin fix
            x1 += parent->MacGetLeftBorderSize() ;
            y1 += parent->MacGetTopBorderSize() ;

            // and now to client coordinates
            wxPoint pt(parent->GetClientAreaOrigin());
            x1 -= pt.x ;
            y1 -= pt.y ;
        }
    }

    if (x)
       *x = x1 ;
    if (y)
       *y = y1 ;
}

void wxWindowMac::DoScreenToClient(int *x, int *y) const
{
    wxNonOwnedWindow* tlw = MacGetTopLevelWindow() ;
    wxCHECK_RET( tlw , wxT("TopLevel Window missing") ) ;
    tlw->GetNonOwnedPeer()->ScreenToWindow( x, y);
    MacRootWindowToWindow( x , y ) ;

    wxPoint origin = GetClientAreaOrigin() ;
    if (x)
       *x -= origin.x ;
    if (y)
       *y -= origin.y ;
}

void wxWindowMac::DoClientToScreen(int *x, int *y) const
{
    wxNonOwnedWindow* tlw = MacGetTopLevelWindow() ;
    wxCHECK_RET( tlw , wxT("TopLevel window missing") ) ;

    wxPoint origin = GetClientAreaOrigin() ;
    if (x)
       *x += origin.x ;
    if (y)
       *y += origin.y ;

    MacWindowToRootWindow( x , y ) ;
    tlw->GetNonOwnedPeer()->WindowToScreen( x , y );
}

void wxWindowMac::MacClientToRootWindow( int *x , int *y ) const
{
    wxPoint origin = GetClientAreaOrigin() ;
    if (x)
       *x += origin.x ;
    if (y)
       *y += origin.y ;

    MacWindowToRootWindow( x , y ) ;
}

void wxWindowMac::MacWindowToRootWindow( int *x , int *y ) const
{
    wxPoint pt ;

    if (x)
        pt.x = *x ;
    if (y)
        pt.y = *y ;

    if ( !IsTopLevel() )
    {
        wxNonOwnedWindow* top = MacGetTopLevelWindow();
        if (top)
        {
            pt.x -= MacGetLeftBorderSize() ;
            pt.y -= MacGetTopBorderSize() ;
            wxWidgetImpl::Convert( &pt , m_peer , top->m_peer ) ;
        }
    }

    if (x)
        *x = (int) pt.x ;
    if (y)
        *y = (int) pt.y ;
}

void wxWindowMac::MacRootWindowToWindow( int *x , int *y ) const
{
    wxPoint pt ;

    if (x)
        pt.x = *x ;
    if (y)
        pt.y = *y ;

    if ( !IsTopLevel() )
    {
        wxNonOwnedWindow* top = MacGetTopLevelWindow();
        if (top)
        {
            wxWidgetImpl::Convert( &pt , top->m_peer , m_peer ) ;
            pt.x += MacGetLeftBorderSize() ;
            pt.y += MacGetTopBorderSize() ;
        }
    }

    if (x)
        *x = (int) pt.x ;
    if (y)
        *y = (int) pt.y ;
}

wxSize wxWindowMac::DoGetSizeFromClientSize( const wxSize & size )  const
{
    wxSize sizeTotal = size;

    int innerwidth, innerheight;
    int left, top;
    int outerwidth, outerheight;
    
    m_peer->GetContentArea( left, top, innerwidth, innerheight );
    m_peer->GetSize( outerwidth, outerheight );
    
    sizeTotal.x += outerwidth-innerwidth;
    sizeTotal.y += outerheight-innerheight;
    
    sizeTotal.x += MacGetLeftBorderSize() + MacGetRightBorderSize() ;
    sizeTotal.y += MacGetTopBorderSize() + MacGetBottomBorderSize() ;

    return sizeTotal;
}

// Get size *available for subwindows* i.e. excluding menu bar etc.
void wxWindowMac::DoGetClientSize( int *x, int *y ) const
{
    int ww, hh;

    int left, top;
    
    m_peer->GetContentArea( left, top, ww, hh );

    if (m_hScrollBar  && m_hScrollBar->IsShown() )
        hh -= m_hScrollBar->GetSize().y ;

    if (m_vScrollBar  && m_vScrollBar->IsShown() )
        ww -= m_vScrollBar->GetSize().x ;

    if (x)
       *x = ww;
    if (y)
       *y = hh;
}

bool wxWindowMac::SetCursor(const wxCursor& cursor)
{
    if (m_cursor.IsSameAs(cursor))
        return false;

    if (!cursor.IsOk())
    {
        if ( ! wxWindowBase::SetCursor( *wxSTANDARD_CURSOR ) )
            return false ;
    }
    else
    {
        if ( ! wxWindowBase::SetCursor( cursor ) )
            return false ;
    }

    wxASSERT_MSG( m_cursor.Ok(),
        wxT("cursor must be valid after call to the base version"));

    if ( GetPeer() != NULL )
        GetPeer()->SetCursor( m_cursor );

    return true ;
}

#if wxUSE_MENUS
bool wxWindowMac::DoPopupMenu(wxMenu *menu, int x, int y)
{
#ifndef __WXUNIVERSAL__
    menu->SetInvokingWindow((wxWindow*)this);
    menu->UpdateUI();

    if ( x == wxDefaultCoord && y == wxDefaultCoord )
    {
        wxPoint mouse = wxGetMousePosition();
        x = mouse.x;
        y = mouse.y;
    }
    else
    {
        ClientToScreen( &x , &y ) ;
    }
    menu->GetPeer()->PopUp(this, x, y);
    menu->SetInvokingWindow( NULL );
    return true;
#else
    // actually this shouldn't be called, because universal is having its own implementation
    return false;
#endif
}
#endif

// ----------------------------------------------------------------------------
// tooltips
// ----------------------------------------------------------------------------

#if wxUSE_TOOLTIPS

void wxWindowMac::DoSetToolTip(wxToolTip *tooltip)
{
    wxWindowBase::DoSetToolTip(tooltip);

    if ( m_tooltip )
        m_tooltip->SetWindow(this);
}

#endif

void wxWindowMac::MacInvalidateBorders()
{
    if ( m_peer == NULL )
        return ;

    bool vis = IsShownOnScreen() ;
    if ( !vis )
        return ;

    int outerBorder = MacGetLeftBorderSize() ;

    if ( m_peer->NeedsFocusRect() )
        outerBorder += 4 ;

    if ( outerBorder == 0 )
        return ;

    // now we know that we have something to do at all

    int tx,ty,tw,th;
    
    m_peer->GetSize( tw, th );
    m_peer->GetPosition( tx, ty );

    wxRect leftupdate( tx-outerBorder,ty,outerBorder,th );
    wxRect rightupdate( tx+tw, ty, outerBorder, th );
    wxRect topupdate( tx-outerBorder, ty-outerBorder, tw + 2 * outerBorder, outerBorder );
    wxRect bottomupdate( tx-outerBorder, ty + th, tw + 2 * outerBorder, outerBorder );
    
    if (GetParent()) {
        GetParent()->m_peer->SetNeedsDisplay(&leftupdate);
        GetParent()->m_peer->SetNeedsDisplay(&rightupdate);
        GetParent()->m_peer->SetNeedsDisplay(&topupdate);
        GetParent()->m_peer->SetNeedsDisplay(&bottomupdate);
    }
}

void wxWindowMac::DoMoveWindow(int x, int y, int width, int height)
{
    // this is never called for a toplevel window, so we know we have a parent
    int former_x , former_y , former_w, former_h ;

    // Get true coordinates of former position
    DoGetPosition( &former_x , &former_y ) ;
    DoGetSize( &former_w , &former_h ) ;

    wxWindow *parent = GetParent();
    if ( parent )
    {
        wxPoint pt(parent->GetClientAreaOrigin());
        former_x += pt.x ;
        former_y += pt.y ;
    }

    int actualWidth = width ;
    int actualHeight = height ;
    int actualX = x;
    int actualY = y;

#if 0
    // min and max sizes are only for sizers, not for explicit size setting
    if ((m_minWidth != -1) && (actualWidth < m_minWidth))
        actualWidth = m_minWidth;
    if ((m_minHeight != -1) && (actualHeight < m_minHeight))
        actualHeight = m_minHeight;
    if ((m_maxWidth != -1) && (actualWidth > m_maxWidth))
        actualWidth = m_maxWidth;
    if ((m_maxHeight != -1) && (actualHeight > m_maxHeight))
        actualHeight = m_maxHeight;
#endif

    bool doMove = false, doResize = false ;

    if ( actualX != former_x || actualY != former_y )
        doMove = true ;

    if ( actualWidth != former_w || actualHeight != former_h )
        doResize = true ;

    if ( doMove || doResize )
    {
        // as the borders are drawn outside the native control, we adjust now

        wxRect bounds( wxPoint( actualX + MacGetLeftBorderSize() ,actualY + MacGetTopBorderSize() ),
            wxSize( actualWidth - (MacGetLeftBorderSize() + MacGetRightBorderSize()) ,
                actualHeight - (MacGetTopBorderSize() + MacGetBottomBorderSize()) ) ) ;

        if ( parent && !parent->IsTopLevel() )
        {
            bounds.Offset( -parent->MacGetLeftBorderSize(), -parent->MacGetTopBorderSize() );
        }

        MacInvalidateBorders() ;

        m_cachedClippedRectValid = false ;
        
        m_peer->Move( bounds.x, bounds.y, bounds.width, bounds.height);

        wxWindowMac::MacSuperChangedPosition() ; // like this only children will be notified

        MacInvalidateBorders() ;

        MacRepositionScrollBars() ;
        if ( doMove )
        {
            wxPoint point(actualX, actualY);
            wxMoveEvent event(point, m_windowId);
            event.SetEventObject(this);
            HandleWindowEvent(event) ;
        }

        if ( doResize )
        {
            MacRepositionScrollBars() ;
            wxSize size(actualWidth, actualHeight);
            wxSizeEvent event(size, m_windowId);
            event.SetEventObject(this);
            HandleWindowEvent(event);
        }
    }
}

wxSize wxWindowMac::DoGetBestSize() const
{
    if ( m_macIsUserPane || IsTopLevel() )
    {
        return wxWindowBase::DoGetBestSize() ;
    }
    else
    {
        wxRect r ;
        
        m_peer->GetBestRect(&r);

        if ( r.GetWidth() == 0 && r.GetHeight() == 0 )
        {
            r.x =
            r.y = 0 ;
            r.width =
            r.height = 16 ;

            if ( IsKindOf( CLASSINFO( wxScrollBar ) ) )
            {
                r.height = 16 ;
            }
    #if wxUSE_SPINBTN
            else if ( IsKindOf( CLASSINFO( wxSpinButton ) ) )
            {
                r.height = 24 ;
            }
    #endif
            else
            {
                // return wxWindowBase::DoGetBestSize() ;
            }
        }

        int bestWidth = r.width + MacGetLeftBorderSize() + 
                    MacGetRightBorderSize();
        int bestHeight = r.height + MacGetTopBorderSize() + 
                     MacGetBottomBorderSize();
        if ( bestHeight < 10 )
            bestHeight = 13 ;

        return wxSize(bestWidth, bestHeight);
    }
}

// set the size of the window: if the dimensions are positive, just use them,
// but if any of them is equal to -1, it means that we must find the value for
// it ourselves (unless sizeFlags contains wxSIZE_ALLOW_MINUS_ONE flag, in
// which case -1 is a valid value for x and y)
//
// If sizeFlags contains wxSIZE_AUTO_WIDTH/HEIGHT flags (default), we calculate
// the width/height to best suit our contents, otherwise we reuse the current
// width/height
void wxWindowMac::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
    // get the current size and position...
    int currentX, currentY;
    int currentW, currentH;

    GetPosition(&currentX, &currentY);
    GetSize(&currentW, &currentH);

    // ... and don't do anything (avoiding flicker) if it's already ok
    if ( x == currentX && y == currentY &&
        width == currentW && height == currentH && ( height != -1 && width != -1 ) )
    {
        // TODO: REMOVE
        MacRepositionScrollBars() ; // we might have a real position shift

        if (sizeFlags & wxSIZE_FORCE_EVENT)
        {
            wxSizeEvent event( wxSize(width,height), GetId() );
            event.SetEventObject( this );
            HandleWindowEvent( event );
        }

        return;
    }

    if ( !(sizeFlags & wxSIZE_ALLOW_MINUS_ONE) )
    {
        if ( x == wxDefaultCoord )
            x = currentX;
        if ( y == wxDefaultCoord )
            y = currentY;
    }

    AdjustForParentClientOrigin( x, y, sizeFlags );

    wxSize size = wxDefaultSize;
    if ( width == wxDefaultCoord )
    {
        if ( sizeFlags & wxSIZE_AUTO_WIDTH )
        {
            size = DoGetBestSize();
            width = size.x;
        }
        else
        {
            // just take the current one
            width = currentW;
        }
    }

    if ( height == wxDefaultCoord )
    {
        if ( sizeFlags & wxSIZE_AUTO_HEIGHT )
        {
            if ( size.x == wxDefaultCoord )
                size = DoGetBestSize();
            // else: already called DoGetBestSize() above

            height = size.y;
        }
        else
        {
            // just take the current one
            height = currentH;
        }
    }

    DoMoveWindow( x, y, width, height );
}

wxPoint wxWindowMac::GetClientAreaOrigin() const
{
    int left,top,width,height;
    m_peer->GetContentArea( left , top , width , height);
    return wxPoint( left + MacGetLeftBorderSize() , top + MacGetTopBorderSize() );
}

void wxWindowMac::DoSetClientSize(int clientwidth, int clientheight)
{
    if ( clientwidth != wxDefaultCoord || clientheight != wxDefaultCoord )
    {
        int currentclientwidth , currentclientheight ;
        int currentwidth , currentheight ;

        GetClientSize( &currentclientwidth , &currentclientheight ) ;
        GetSize( &currentwidth , &currentheight ) ;

        DoSetSize( wxDefaultCoord , wxDefaultCoord , currentwidth + clientwidth - currentclientwidth ,
            currentheight + clientheight - currentclientheight , wxSIZE_USE_EXISTING ) ;
    }
}

void wxWindowMac::SetLabel(const wxString& title)
{
    m_label = title ;

    if ( m_peer && m_peer->IsOk() && !(IsKindOf( CLASSINFO(wxButton) ) && GetId() == wxID_HELP) )
        m_peer->SetLabel( wxStripMenuCodes(m_label, wxStrip_Mnemonics), GetFont().GetEncoding() ) ;

    // do not trigger refreshes upon invisible and possible partly created objects
    if ( IsShownOnScreen() )
        Refresh() ;
}

wxString wxWindowMac::GetLabel() const
{
    return m_label ;
}

bool wxWindowMac::Show(bool show)
{
    if ( !wxWindowBase::Show(show) )
        return false;

    if ( m_peer )
        m_peer->SetVisibility( show ) ;

    return true;
}

void wxWindowMac::DoEnable(bool enable)
{
    m_peer->Enable( enable ) ;
}

//
// status change notifications
//

void wxWindowMac::MacVisibilityChanged()
{
}

void wxWindowMac::MacHiliteChanged()
{
}

void wxWindowMac::MacEnabledStateChanged()
{
    OnEnabled( m_peer->IsEnabled() );
}

//
// status queries on the inherited window's state
//

bool wxWindowMac::MacIsReallyEnabled()
{
    return m_peer->IsEnabled() ;
}

bool wxWindowMac::MacIsReallyHilited()
{
#if wxOSX_USE_CARBON
    return m_peer->IsActive();
#else
    return true; // TODO
#endif
}

int wxWindowMac::GetCharHeight() const
{
    wxCoord height;
    GetTextExtent( wxT("g") , NULL , &height , NULL , NULL , NULL );

    return height;
}

int wxWindowMac::GetCharWidth() const
{
    wxCoord width;
    GetTextExtent( wxT("g") , &width , NULL , NULL , NULL , NULL );

    return width;
}

void wxWindowMac::GetTextExtent(const wxString& str, int *x, int *y,
                           int *descent, int *externalLeading, const wxFont *theFont ) const
{
    const wxFont *fontToUse = theFont;
    wxFont tempFont;
    if ( !fontToUse )
    {
        tempFont = GetFont();
        fontToUse = &tempFont;
    }

    wxGraphicsContext* ctx = wxGraphicsContext::Create();
    ctx->SetFont( *fontToUse, *wxBLACK );

    wxDouble h , d , e , w;
    ctx->GetTextExtent( str, &w, &h, &d, &e );
    
    delete ctx;
    
    if ( externalLeading )
        *externalLeading = (wxCoord)(e+0.5);
    if ( descent )
        *descent = (wxCoord)(d+0.5);
    if ( x )
        *x = (wxCoord)(w+0.5);
    if ( y )
        *y = (wxCoord)(h+0.5);
}

/*
 * Rect is given in client coordinates, for further reading, read wxTopLevelWindowMac::InvalidateRect
 * we always intersect with the entire window, not only with the client area
 */

void wxWindowMac::Refresh(bool WXUNUSED(eraseBack), const wxRect *rect)
{
    if ( m_peer == NULL )
        return ;

    if ( !IsShownOnScreen() )
        return ;
        
    m_peer->SetNeedsDisplay( rect ) ;
}

void wxWindowMac::DoFreeze()
{
#if wxOSX_USE_CARBON
    if ( m_peer && m_peer->IsOk() )
        m_peer->SetDrawingEnabled( false ) ;
#endif
}

void wxWindowMac::DoThaw()
{
#if wxOSX_USE_CARBON
    if ( m_peer && m_peer->IsOk() )
    {
        m_peer->SetDrawingEnabled( true ) ;
        m_peer->InvalidateWithChildren() ;
    }
#endif
}

wxWindow *wxGetActiveWindow()
{
    // actually this is a windows-only concept
    return NULL;
}

// Coordinates relative to the window
void wxWindowMac::WarpPointer(int WXUNUSED(x_pos), int WXUNUSED(y_pos))
{
    // We really don't move the mouse programmatically under Mac.
}

void wxWindowMac::OnEraseBackground(wxEraseEvent& event)
{
    if ( MacGetTopLevelWindow() == NULL )
        return ;
/*
#if TARGET_API_MAC_OSX
    if ( !m_backgroundColour.Ok() || GetBackgroundStyle() == wxBG_STYLE_TRANSPARENT )
    {
    }
    else
#endif
*/
    if ( GetBackgroundStyle() == wxBG_STYLE_COLOUR )
    {
        event.GetDC()->Clear() ;
    }
    else if ( GetBackgroundStyle() == wxBG_STYLE_CUSTOM )
    {
        // don't skip the event here, custom background means that the app
        // is drawing it itself in its OnPaint(), so don't draw it at all
        // now to avoid flicker
    }
    else
    {
        event.Skip() ;
    }
}

void wxWindowMac::OnNcPaint( wxNcPaintEvent& event )
{
    event.Skip() ;
}

int wxWindowMac::GetScrollPos(int orient) const
{
    if ( orient == wxHORIZONTAL )
    {
       if ( m_hScrollBar )
           return m_hScrollBar->GetThumbPosition() ;
    }
    else
    {
       if ( m_vScrollBar )
           return m_vScrollBar->GetThumbPosition() ;
    }

    return 0;
}

// This now returns the whole range, not just the number
// of positions that we can scroll.
int wxWindowMac::GetScrollRange(int orient) const
{
    if ( orient == wxHORIZONTAL )
    {
       if ( m_hScrollBar )
           return m_hScrollBar->GetRange() ;
    }
    else
    {
       if ( m_vScrollBar )
           return m_vScrollBar->GetRange() ;
    }

    return 0;
}

int wxWindowMac::GetScrollThumb(int orient) const
{
    if ( orient == wxHORIZONTAL )
    {
       if ( m_hScrollBar )
           return m_hScrollBar->GetThumbSize() ;
    }
    else
    {
       if ( m_vScrollBar )
           return m_vScrollBar->GetThumbSize() ;
    }

    return 0;
}

void wxWindowMac::SetScrollPos(int orient, int pos, bool WXUNUSED(refresh))
{
    if ( orient == wxHORIZONTAL )
    {
       if ( m_hScrollBar )
           m_hScrollBar->SetThumbPosition( pos ) ;
    }
    else
    {
       if ( m_vScrollBar )
           m_vScrollBar->SetThumbPosition( pos ) ;
    }
}

void
wxWindowMac::AlwaysShowScrollbars(bool hflag, bool vflag)
{
    bool needVisibilityUpdate = false;

    if ( m_hScrollBarAlwaysShown != hflag )
    {
        m_hScrollBarAlwaysShown = hflag;
        needVisibilityUpdate = true;
    }

    if ( m_vScrollBarAlwaysShown != vflag )
    {
        m_vScrollBarAlwaysShown = vflag;
        needVisibilityUpdate = true;
    }

    if ( needVisibilityUpdate )
        DoUpdateScrollbarVisibility();
}

//
// we draw borders and grow boxes, are already set up and clipped in the current port / cgContextRef
// our own window origin is at leftOrigin/rightOrigin
//

void  wxWindowMac::MacPaintGrowBox()
{
    if ( IsTopLevel() )
        return ;

    if ( MacHasScrollBarCorner() )
    {
        CGContextRef cgContext = (CGContextRef) MacGetCGContextRef() ;
        wxASSERT( cgContext ) ;

        int tx,ty,tw,th;
    
        m_peer->GetSize( tw, th );
        m_peer->GetPosition( tx, ty );

        Rect rect  = { ty,tx, ty+th, tx+tw };


        int size = m_hScrollBar ? m_hScrollBar->GetSize().y : ( m_vScrollBar ? m_vScrollBar->GetSize().x : MAC_SCROLLBAR_SIZE ) ;
        CGRect cgrect = CGRectMake( rect.right - size , rect.bottom - size , size , size ) ;
        CGContextSaveGState( cgContext );

        if ( m_backgroundColour.Ok() )
        {
            CGContextSetFillColorWithColor( cgContext, m_backgroundColour.GetCGColor() );
        }
        else
        {
            CGContextSetRGBFillColor( cgContext, (CGFloat) 1.0, (CGFloat)1.0 ,(CGFloat) 1.0 , (CGFloat)1.0 );
        }
        CGContextFillRect( cgContext, cgrect );
        CGContextRestoreGState( cgContext );
    }
}

void wxWindowMac::MacPaintBorders( int WXUNUSED(leftOrigin) , int WXUNUSED(rightOrigin) )
{
    if ( IsTopLevel() )
        return ;

    bool hasFocus = m_peer->NeedsFocusRect() && m_peer->HasFocus() ;

    // back to the surrounding frame rectangle
    int tx,ty,tw,th;
    
    m_peer->GetSize( tw, th );
    m_peer->GetPosition( tx, ty );

    Rect rect  = { ty,tx, ty+th, tx+tw };

#if wxOSX_USE_COCOA_OR_CARBON

    InsetRect( &rect, -1 , -1 ) ;

    {
        CGRect cgrect = CGRectMake( rect.left , rect.top , rect.right - rect.left ,
            rect.bottom - rect.top ) ;

        CGContextRef cgContext = (CGContextRef) GetParent()->MacGetCGContextRef() ;
        wxASSERT( cgContext ) ;

        if ( m_peer->NeedsFrame() )
        {
            HIThemeFrameDrawInfo info ;
            memset( &info, 0 , sizeof(info) ) ;

            info.version = 0 ;
            info.kind = 0 ;
            info.state = IsEnabled() ? kThemeStateActive : kThemeStateInactive ;
            info.isFocused = hasFocus ;

            if ( HasFlag(wxRAISED_BORDER) || HasFlag(wxSUNKEN_BORDER) || HasFlag(wxDOUBLE_BORDER) )
            {
                info.kind = kHIThemeFrameTextFieldSquare ;
                HIThemeDrawFrame( &cgrect , &info , cgContext , kHIThemeOrientationNormal ) ;
            }
            else if ( HasFlag(wxSIMPLE_BORDER) )
            {
                info.kind = kHIThemeFrameListBox ;
                HIThemeDrawFrame( &cgrect , &info , cgContext , kHIThemeOrientationNormal ) ;
            }
        }
        
        if ( hasFocus )
        {
            HIThemeDrawFocusRect( &cgrect , true , cgContext , kHIThemeOrientationNormal ) ;
        }
    }
#endif // wxOSX_USE_COCOA_OR_CARBON
}

void wxWindowMac::RemoveChild( wxWindowBase *child )
{
    if ( child == m_hScrollBar )
        m_hScrollBar = NULL ;
    if ( child == m_vScrollBar )
        m_vScrollBar = NULL ;

    wxWindowBase::RemoveChild( child ) ;
}

void wxWindowMac::DoUpdateScrollbarVisibility()
{
    bool triggerSizeEvent = false;

    if ( m_hScrollBar )
    {
        bool showHScrollBar = m_hScrollBarAlwaysShown || m_hScrollBar->IsNeeded();

        if ( m_hScrollBar->IsShown() != showHScrollBar )
        {
            m_hScrollBar->Show( showHScrollBar );
            triggerSizeEvent = true;
        }
    }

    if ( m_vScrollBar)
    {
        bool showVScrollBar = m_vScrollBarAlwaysShown || m_vScrollBar->IsNeeded();

        if ( m_vScrollBar->IsShown() != showVScrollBar )
        {
            m_vScrollBar->Show( showVScrollBar ) ;
            triggerSizeEvent = true;
        }
    }

    MacRepositionScrollBars() ;
    if ( triggerSizeEvent )
    {
        wxSizeEvent event(GetSize(), m_windowId);
        event.SetEventObject(this);
        HandleWindowEvent(event);
    }
}

// New function that will replace some of the above.
void wxWindowMac::SetScrollbar(int orient, int pos, int thumb,
                               int range, bool refresh)
{
    if ( orient == wxHORIZONTAL && m_hScrollBar )
        m_hScrollBar->SetScrollbar(pos, thumb, range, thumb, refresh);
    else if ( orient == wxVERTICAL && m_vScrollBar )
        m_vScrollBar->SetScrollbar(pos, thumb, range, thumb, refresh);

    DoUpdateScrollbarVisibility();
}

// Does a physical scroll
void wxWindowMac::ScrollWindow(int dx, int dy, const wxRect *rect)
{
    if ( dx == 0 && dy == 0 )
        return ;

    int width , height ;
    GetClientSize( &width , &height ) ;

    {
        wxRect scrollrect( MacGetLeftBorderSize() , MacGetTopBorderSize() , width , height ) ;
        if ( rect )
            scrollrect.Intersect( *rect ) ;
        // as the native control might be not a 0/0 wx window coordinates, we have to offset
        scrollrect.Offset( -MacGetLeftBorderSize() , -MacGetTopBorderSize() ) ;

        m_peer->ScrollRect( &scrollrect, dx, dy );
    }

    wxWindowMac *child;
    int x, y, w, h;
    for (wxWindowList::compatibility_iterator node = GetChildren().GetFirst(); node; node = node->GetNext())
    {
        child = node->GetData();
        if (child == NULL)
            continue;
        if (child == m_vScrollBar)
            continue;
        if (child == m_hScrollBar)
            continue;
        if (child->IsTopLevel())
            continue;

        child->GetPosition( &x, &y );
        child->GetSize( &w, &h );
        if (rect)
        {
            wxRect rc( x, y, w, h );
            if (rect->Intersects( rc ))
                child->SetSize( x + dx, y + dy, w, h, wxSIZE_AUTO|wxSIZE_ALLOW_MINUS_ONE );
        }
        else
        {
            child->SetSize( x + dx, y + dy, w, h, wxSIZE_AUTO|wxSIZE_ALLOW_MINUS_ONE );
        }
    }
}

void wxWindowMac::MacOnScroll( wxScrollEvent &event )
{
    if ( event.GetEventObject() == m_vScrollBar || event.GetEventObject() == m_hScrollBar )
    {
        wxScrollWinEvent wevent;
        wevent.SetPosition(event.GetPosition());
        wevent.SetOrientation(event.GetOrientation());
        wevent.SetEventObject(this);

        if (event.GetEventType() == wxEVT_SCROLL_TOP)
            wevent.SetEventType( wxEVT_SCROLLWIN_TOP );
        else if (event.GetEventType() == wxEVT_SCROLL_BOTTOM)
            wevent.SetEventType( wxEVT_SCROLLWIN_BOTTOM );
        else if (event.GetEventType() == wxEVT_SCROLL_LINEUP)
            wevent.SetEventType( wxEVT_SCROLLWIN_LINEUP );
        else if (event.GetEventType() == wxEVT_SCROLL_LINEDOWN)
            wevent.SetEventType( wxEVT_SCROLLWIN_LINEDOWN );
        else if (event.GetEventType() == wxEVT_SCROLL_PAGEUP)
            wevent.SetEventType( wxEVT_SCROLLWIN_PAGEUP );
        else if (event.GetEventType() == wxEVT_SCROLL_PAGEDOWN)
            wevent.SetEventType( wxEVT_SCROLLWIN_PAGEDOWN );
        else if (event.GetEventType() == wxEVT_SCROLL_THUMBTRACK)
            wevent.SetEventType( wxEVT_SCROLLWIN_THUMBTRACK );
        else if (event.GetEventType() == wxEVT_SCROLL_THUMBRELEASE)
            wevent.SetEventType( wxEVT_SCROLLWIN_THUMBRELEASE );

        HandleWindowEvent(wevent);
    }
}

wxWindow *wxWindowBase::DoFindFocus()
{
    return wxFindWindowFromWXWidget(wxWidgetImpl::FindFocus());
}

void wxWindowMac::OnInternalIdle()
{
    // This calls the UI-update mechanism (querying windows for
    // menu/toolbar/control state information)
    if (wxUpdateUIEvent::CanUpdate(this) && IsShownOnScreen())
        UpdateWindowUI(wxUPDATE_UI_FROMIDLE);
}

// Raise the window to the top of the Z order
void wxWindowMac::Raise()
{
    m_peer->Raise();
}

// Lower the window to the bottom of the Z order
void wxWindowMac::Lower()
{
    m_peer->Lower();
}

// static wxWindow *gs_lastWhich = NULL;

bool wxWindowMac::MacSetupCursor( const wxPoint& pt )
{
    // first trigger a set cursor event

    wxPoint clientorigin = GetClientAreaOrigin() ;
    wxSize clientsize = GetClientSize() ;
    wxCursor cursor ;
    if ( wxRect2DInt( clientorigin.x , clientorigin.y , clientsize.x , clientsize.y ).Contains( wxPoint2DInt( pt ) ) )
    {
        wxSetCursorEvent event( pt.x , pt.y );

        bool processedEvtSetCursor = HandleWindowEvent(event);
        if ( processedEvtSetCursor && event.HasCursor() )
        {
            cursor = event.GetCursor() ;
        }
        else
        {
            // the test for processedEvtSetCursor is here to prevent using m_cursor
            // if the user code caught EVT_SET_CURSOR() and returned nothing from
            // it - this is a way to say that our cursor shouldn't be used for this
            // point
            if ( !processedEvtSetCursor && m_cursor.Ok() )
                cursor = m_cursor ;

            if ( !wxIsBusy() && !GetParent() )
                cursor = *wxSTANDARD_CURSOR ;
        }

        if ( cursor.Ok() )
            cursor.MacInstall() ;
    }

    return cursor.Ok() ;
}

wxString wxWindowMac::MacGetToolTipString( wxPoint &WXUNUSED(pt) )
{
#if wxUSE_TOOLTIPS
    if ( m_tooltip )
        return m_tooltip->GetTip() ;
#endif

    return wxEmptyString ;
}

void wxWindowMac::ClearBackground()
{
    Refresh() ;
    Update() ;
}

void wxWindowMac::Update()
{
    wxNonOwnedWindow* top = MacGetTopLevelWindow();
    if (top)
        top->Update() ;
}

wxNonOwnedWindow* wxWindowMac::MacGetTopLevelWindow() const
{
    wxWindowMac *iter = (wxWindowMac*)this ;

    while ( iter )
    {
        if ( iter->IsTopLevel() )
        {
            wxTopLevelWindow* toplevel = wxDynamicCast(iter,wxTopLevelWindow);
            if ( toplevel )
                return toplevel;
#if wxUSE_POPUPWIN
            wxPopupWindow* popupwin = wxDynamicCast(iter,wxPopupWindow);
            if ( popupwin )
                return popupwin;
#endif
        }
        iter = iter->GetParent() ;
    }

    return NULL ;
}

const wxRect& wxWindowMac::MacGetClippedClientRect() const
{
    MacUpdateClippedRects() ;

    return m_cachedClippedClientRect ;
}

const wxRect& wxWindowMac::MacGetClippedRect() const
{
    MacUpdateClippedRects() ;

    return m_cachedClippedRect ;
}

const wxRect&wxWindowMac:: MacGetClippedRectWithOuterStructure() const
{
    MacUpdateClippedRects() ;

    return m_cachedClippedRectWithOuterStructure ;
}

const wxRegion& wxWindowMac::MacGetVisibleRegion( bool includeOuterStructures )
{
    static wxRegion emptyrgn ;

    if ( !m_isBeingDeleted && IsShownOnScreen() )
    {
        MacUpdateClippedRects() ;
        if ( includeOuterStructures )
            return m_cachedClippedRegionWithOuterStructure ;
        else
            return m_cachedClippedRegion ;
    }
    else
    {
        return emptyrgn ;
    }
}

void wxWindowMac::MacUpdateClippedRects() const
{
#if wxOSX_USE_CARBON
    if ( m_cachedClippedRectValid )
        return ;

    // includeOuterStructures is true if we try to draw somthing like a focus ring etc.
    // also a window dc uses this, in this case we only clip in the hierarchy for hard
    // borders like a scrollwindow, splitter etc otherwise we end up in a paranoia having
    // to add focus borders everywhere

    Rect rIncludingOuterStructures ;

    int tx,ty,tw,th;
    
    m_peer->GetSize( tw, th );
    m_peer->GetPosition( tx, ty );

    Rect r  = { ty,tx, ty+th, tx+tw };

    r.left -= MacGetLeftBorderSize() ;
    r.top -= MacGetTopBorderSize() ;
    r.bottom += MacGetBottomBorderSize() ;
    r.right += MacGetRightBorderSize() ;

    r.right -= r.left ;
    r.bottom -= r.top ;
    r.left = 0 ;
    r.top = 0 ;

    rIncludingOuterStructures = r ;
    InsetRect( &rIncludingOuterStructures , -4 , -4 ) ;

    wxRect cl = GetClientRect() ;
    Rect rClient = { cl.y , cl.x , cl.y + cl.height , cl.x + cl.width } ;

    int x , y ;
    wxSize size ;
    const wxWindow* child = (wxWindow*) this ;
    const wxWindow* parent = NULL ;

    while ( !child->IsTopLevel() && ( parent = child->GetParent() ) != NULL )
    {
        if ( parent->MacIsChildOfClientArea(child) )
        {
            size = parent->GetClientSize() ;
            wxPoint origin = parent->GetClientAreaOrigin() ;
            x = origin.x ;
            y = origin.y ;
        }
        else
        {
            // this will be true for scrollbars, toolbars etc.
            size = parent->GetSize() ;
            y = parent->MacGetTopBorderSize() ;
            x = parent->MacGetLeftBorderSize() ;
            size.x -= parent->MacGetLeftBorderSize() + parent->MacGetRightBorderSize() ;
            size.y -= parent->MacGetTopBorderSize() + parent->MacGetBottomBorderSize() ;
        }

        parent->MacWindowToRootWindow( &x, &y ) ;
        MacRootWindowToWindow( &x , &y ) ;

        Rect rparent = { y , x , y + size.y , x + size.x } ;

        // the wxwindow and client rects will always be clipped
        SectRect( &r , &rparent , &r ) ;
        SectRect( &rClient , &rparent , &rClient ) ;

        // the structure only at 'hard' borders
        if ( parent->MacClipChildren() ||
            ( parent->GetParent() && parent->GetParent()->MacClipGrandChildren() ) )
        {
            SectRect( &rIncludingOuterStructures , &rparent , &rIncludingOuterStructures ) ;
        }

        child = parent ;
    }

    m_cachedClippedRect = wxRect( r.left , r.top , r.right - r.left , r.bottom - r.top ) ;
    m_cachedClippedClientRect = wxRect( rClient.left , rClient.top ,
        rClient.right - rClient.left , rClient.bottom - rClient.top ) ;
    m_cachedClippedRectWithOuterStructure = wxRect(
        rIncludingOuterStructures.left , rIncludingOuterStructures.top ,
        rIncludingOuterStructures.right - rIncludingOuterStructures.left ,
        rIncludingOuterStructures.bottom - rIncludingOuterStructures.top ) ;

    m_cachedClippedRegionWithOuterStructure = wxRegion( m_cachedClippedRectWithOuterStructure ) ;
    m_cachedClippedRegion = wxRegion( m_cachedClippedRect ) ;
    m_cachedClippedClientRegion = wxRegion( m_cachedClippedClientRect ) ;

    m_cachedClippedRectValid = true ;
#endif
}

/*
    This function must not change the updatergn !
 */
bool wxWindowMac::MacDoRedraw( long time )
{
    bool handled = false ;
    
    wxRegion formerUpdateRgn = m_updateRegion;
    wxRegion clientUpdateRgn = formerUpdateRgn;

    wxSize sz = GetClientSize() ;
    wxPoint origin = GetClientAreaOrigin() ;
    
    clientUpdateRgn.Intersect(origin.x , origin.y , origin.x + sz.x , origin.y + sz.y);
    
    // first send an erase event to the entire update area
    {
        // for the toplevel window this really is the entire area
        // for all the others only their client area, otherwise they
        // might be drawing with full alpha and eg put blue into
        // the grow-box area of a scrolled window (scroll sample)
        wxDC* dc = new wxWindowDC(this);
        if ( IsTopLevel() )
            dc->SetDeviceClippingRegion(formerUpdateRgn);
        else
            dc->SetDeviceClippingRegion(clientUpdateRgn);

        wxEraseEvent eevent( GetId(), dc );
        eevent.SetEventObject( this );
        HandleWindowEvent( eevent );
        delete dc ;
    }

    MacPaintGrowBox();

    // calculate a client-origin version of the update rgn and set m_updateRegion to that
    clientUpdateRgn.Offset( -origin.x , -origin.y );
    m_updateRegion = clientUpdateRgn ;

    if ( !m_updateRegion.Empty() )
    {
        // paint the window itself

        wxPaintEvent event(GetId());
        event.SetTimestamp(time);
        event.SetEventObject(this);
        handled = HandleWindowEvent(event);
    }

    m_updateRegion = formerUpdateRgn;
    return handled;
}

void wxWindowMac::MacPaintChildrenBorders()
{
    // now we cannot rely on having its borders drawn by a window itself, as it does not
    // get the updateRgn wide enough to always do so, so we do it from the parent
    // this would also be the place to draw any custom backgrounds for native controls
    // in Composited windowing
    wxPoint clientOrigin = GetClientAreaOrigin() ;

    wxWindowMac *child;
    int x, y, w, h;
    for (wxWindowList::compatibility_iterator node = GetChildren().GetFirst(); node; node = node->GetNext())
    {
        child = node->GetData();
        if (child == NULL)
            continue;
        if (child == m_vScrollBar)
            continue;
        if (child == m_hScrollBar)
            continue;
        if (child->IsTopLevel())
            continue;
        if (!child->IsShown())
            continue;

        // only draw those in the update region (add a safety margin of 10 pixels for shadow effects

        child->GetPosition( &x, &y );
        child->GetSize( &w, &h );
        
        if ( m_updateRegion.Contains(clientOrigin.x+x-10, clientOrigin.y+y-10, w+20, h+20) )
        {
            // paint custom borders
            wxNcPaintEvent eventNc( child->GetId() );
            eventNc.SetEventObject( child );
            if ( !child->HandleWindowEvent( eventNc ) )
            {
                child->MacPaintBorders(0, 0) ;
            }
        }
    }
}


WXWindow wxWindowMac::MacGetTopLevelWindowRef() const
{
    wxNonOwnedWindow* tlw = MacGetTopLevelWindow(); 
    return tlw ? tlw->GetWXWindow() : NULL ;
}

bool wxWindowMac::MacHasScrollBarCorner() const
{
    /* Returns whether the scroll bars in a wxScrolledWindow should be
     * shortened. Scroll bars should be shortened if either:
     *
     * - both scroll bars are visible, or
     *
     * - there is a resize box in the parent frame's corner and this
     *   window shares the bottom and right edge with the parent
     *   frame.
     */

    if ( m_hScrollBar == NULL && m_vScrollBar == NULL )
        return false;

    if ( ( m_hScrollBar && m_hScrollBar->IsShown() )
         && ( m_vScrollBar && m_vScrollBar->IsShown() ) )
    {
        // Both scroll bars visible
        return true;
    }
    else
    {
        wxPoint thisWindowBottomRight = GetScreenRect().GetBottomRight();

        for ( const wxWindow *win = (wxWindow*)this; win; win = win->GetParent() )
        {
            const wxFrame *frame = wxDynamicCast( win, wxFrame ) ;
            if ( frame )
            {
                if ( frame->GetWindowStyleFlag() & wxRESIZE_BORDER )
                {
                    // Parent frame has resize handle
                    wxPoint frameBottomRight = frame->GetScreenRect().GetBottomRight();

                    // Note: allow for some wiggle room here as wxMac's
                    // window rect calculations seem to be imprecise
                    if ( abs( thisWindowBottomRight.x - frameBottomRight.x ) <= 2
                        && abs( thisWindowBottomRight.y - frameBottomRight.y ) <= 2 )
                    {
                        // Parent frame has resize handle and shares
                        // right bottom corner
                        return true ;
                    }
                    else
                    {
                        // Parent frame has resize handle but doesn't
                        // share right bottom corner
                        return false ;
                    }
                }
                else
                {
                    // Parent frame doesn't have resize handle
                    return false ;
                }
            }
        }

        // No parent frame found
        return false ;
    }
}

void wxWindowMac::MacCreateScrollBars( long style )
{
    wxASSERT_MSG( m_vScrollBar == NULL && m_hScrollBar == NULL , wxT("attempt to create window twice") ) ;

    if ( style & ( wxVSCROLL | wxHSCROLL ) )
    {
        int scrlsize = MAC_SCROLLBAR_SIZE ;
        if ( GetWindowVariant() == wxWINDOW_VARIANT_SMALL || GetWindowVariant() == wxWINDOW_VARIANT_MINI )
        {
            scrlsize = MAC_SMALL_SCROLLBAR_SIZE ;
        }

        int adjust = MacHasScrollBarCorner() ? scrlsize - 1: 0 ;
        int width, height ;
        GetClientSize( &width , &height ) ;

        wxPoint vPoint(width - scrlsize, 0) ;
        wxSize vSize(scrlsize, height - adjust) ;
        wxPoint hPoint(0, height - scrlsize) ;
        wxSize hSize(width - adjust, scrlsize) ;

        // we have to set the min size to a smaller value, otherwise they cannot get smaller (InitialSize sets MinSize)
        if ( style & wxVSCROLL )
        {
            m_vScrollBar = new wxScrollBar((wxWindow*)this, wxID_ANY, vPoint, vSize , wxVERTICAL);
            m_vScrollBar->SetMinSize( wxDefaultSize );
        }

        if ( style  & wxHSCROLL )
        {
            m_hScrollBar = new wxScrollBar((wxWindow*)this, wxID_ANY, hPoint, hSize , wxHORIZONTAL);
            m_hScrollBar->SetMinSize( wxDefaultSize );
        }
    }

    // because the create does not take into account the client area origin
    // we might have a real position shift
    MacRepositionScrollBars() ;
}

bool wxWindowMac::MacIsChildOfClientArea( const wxWindow* child ) const
{
    bool result = ((child == NULL) || ((child != m_hScrollBar) && (child != m_vScrollBar)));

    return result ;
}

void wxWindowMac::MacRepositionScrollBars()
{
    if ( !m_hScrollBar && !m_vScrollBar )
        return ;

    int scrlsize = m_hScrollBar ? m_hScrollBar->GetSize().y : ( m_vScrollBar ? m_vScrollBar->GetSize().x : MAC_SCROLLBAR_SIZE ) ;
    int adjust = MacHasScrollBarCorner() ? scrlsize - 1 : 0 ;

    // get real client area
    int width, height ;
    GetSize( &width , &height );

    width -= MacGetLeftBorderSize() + MacGetRightBorderSize();
    height -= MacGetTopBorderSize() + MacGetBottomBorderSize();

    wxPoint vPoint( width - scrlsize, 0 ) ;
    wxSize vSize( scrlsize, height - adjust ) ;
    wxPoint hPoint( 0 , height - scrlsize ) ;
    wxSize hSize( width - adjust, scrlsize ) ;

    if ( m_vScrollBar )
        m_vScrollBar->SetSize( vPoint.x , vPoint.y, vSize.x, vSize.y , wxSIZE_ALLOW_MINUS_ONE );
    if ( m_hScrollBar )
        m_hScrollBar->SetSize( hPoint.x , hPoint.y, hSize.x, hSize.y, wxSIZE_ALLOW_MINUS_ONE );
}

bool wxWindowMac::AcceptsFocus() const
{
    if ( MacIsUserPane() )
        return wxWindowBase::AcceptsFocus();
    else
        return m_peer->CanFocus();
}

void wxWindowMac::MacSuperChangedPosition()
{
    // only window-absolute structures have to be moved i.e. controls

    m_cachedClippedRectValid = false ;

    wxWindowMac *child;
    wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
    while ( node )
    {
        child = node->GetData();
        child->MacSuperChangedPosition() ;

        node = node->GetNext();
    }
}

void wxWindowMac::MacTopLevelWindowChangedPosition()
{
    // only screen-absolute structures have to be moved i.e. glcanvas

    wxWindowMac *child;
    wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
    while ( node )
    {
        child = node->GetData();
        child->MacTopLevelWindowChangedPosition() ;

        node = node->GetNext();
    }
}

long wxWindowMac::MacGetLeftBorderSize() const
{
    if ( IsTopLevel() )
        return 0 ;

    SInt32 border = 0 ;

    if ( m_peer && m_peer->NeedsFrame() )
    {
        if (HasFlag(wxRAISED_BORDER) || HasFlag( wxSUNKEN_BORDER) || HasFlag(wxDOUBLE_BORDER))
        {
#if wxOSX_USE_COCOA_OR_CARBON
            // this metric is only the 'outset' outside the simple frame rect
            GetThemeMetric( kThemeMetricEditTextFrameOutset , &border ) ;
            border += 1;
#else
            border += 2;
#endif
        }
        else if (HasFlag(wxSIMPLE_BORDER))
        {
#if wxOSX_USE_COCOA_OR_CARBON
            // this metric is only the 'outset' outside the simple frame rect
            GetThemeMetric( kThemeMetricListBoxFrameOutset , &border ) ;
            border += 1;
#else
            border += 1;
#endif
        }
    }

    return border ;
}

long wxWindowMac::MacGetRightBorderSize() const
{
    // they are all symmetric in mac themes
    return MacGetLeftBorderSize() ;
}

long wxWindowMac::MacGetTopBorderSize() const
{
    // they are all symmetric in mac themes
    return MacGetLeftBorderSize() ;
}

long wxWindowMac::MacGetBottomBorderSize() const
{
    // they are all symmetric in mac themes
    return MacGetLeftBorderSize() ;
}

long wxWindowMac::MacRemoveBordersFromStyle( long style )
{
    return style & ~wxBORDER_MASK ;
}

// Find the wxWindowMac at the current mouse position, returning the mouse
// position.
wxWindow * wxFindWindowAtPointer( wxPoint& pt )
{
    pt = wxGetMousePosition();
    wxWindowMac* found = wxFindWindowAtPoint(pt);

    return (wxWindow*) found;
}

// Get the current mouse position.
wxPoint wxGetMousePosition()
{
    int x, y;

    wxGetMousePosition( &x, &y );

    return wxPoint(x, y);
}

void wxWindowMac::OnMouseEvent( wxMouseEvent &event )
{
    if ( event.GetEventType() == wxEVT_RIGHT_DOWN )
    {
        // copied from wxGTK : CS
        // VZ: shouldn't we move this to base class then?

        // generate a "context menu" event: this is similar to wxEVT_RIGHT_DOWN
        // except that:
        //
        // (a) it's a command event and so is propagated to the parent
        // (b) under MSW it can be generated from kbd too
        // (c) it uses screen coords (because of (a))
        wxContextMenuEvent evtCtx(wxEVT_CONTEXT_MENU,
                                  this->GetId(),
                                  this->ClientToScreen(event.GetPosition()));
        evtCtx.SetEventObject(this);
        if ( ! HandleWindowEvent(evtCtx) )
            event.Skip() ;
    }
    else
    {
        event.Skip() ;
    }
}

void wxWindowMac::TriggerScrollEvent( wxEventType WXUNUSED(scrollEvent) )
{
}

Rect wxMacGetBoundsForControl( wxWindowMac* window , const wxPoint& pos , const wxSize &size , bool adjustForOrigin )
{
    int x, y, w, h ;

    window->MacGetBoundsForControl( pos , size , x , y, w, h , adjustForOrigin ) ;
    Rect bounds = { y, x, y + h, x + w };

    return bounds ;
}

bool wxWindowMac::OSXHandleClicked( double WXUNUSED(timestampsec) )
{
    return false;
}

wxInt32 wxWindowMac::MacControlHit(WXEVENTHANDLERREF WXUNUSED(handler) , WXEVENTREF event )
{
#if wxOSX_USE_COCOA_OR_CARBON
    if ( OSXHandleClicked( GetEventTime((EventRef)event) ) )
        return noErr;
        
    return eventNotHandledErr ;
#else
    return 0;
#endif
}

bool wxWindowMac::Reparent(wxWindowBase *newParentBase)
{
    wxWindowMac *newParent = (wxWindowMac *)newParentBase;
    if ( !wxWindowBase::Reparent(newParent) )
        return false;

    m_peer->RemoveFromParent();
    m_peer->Embed( GetParent()->GetPeer() );
    return true;
}

bool wxWindowMac::SetTransparent(wxByte alpha)
{
    SetBackgroundStyle(wxBG_STYLE_TRANSPARENT);

    if ( alpha != m_macAlpha )
    {
        m_macAlpha = alpha ;
        Refresh() ;
    }
    return true ;
}


bool wxWindowMac::CanSetTransparent()
{
    return true ;
}

wxByte wxWindowMac::GetTransparent() const
{
    return m_macAlpha ;
}

bool wxWindowMac::IsShownOnScreen() const
{
    if ( m_peer && m_peer->IsOk() )
    {
        bool peerVis = m_peer->IsVisible();
        bool wxVis = wxWindowBase::IsShownOnScreen();
        if( peerVis != wxVis )
        {
            // CS : put a breakpoint here to investigate differences
            // between native an wx visibilities
            // the only place where I've encountered them until now
            // are the hiding/showing sequences where the vis-changed event is
            // first sent to the innermost control, while wx does things
            // from the outmost control
            wxVis = wxWindowBase::IsShownOnScreen();
            return wxVis;
        }

        return m_peer->IsVisible();
    }
    return wxWindowBase::IsShownOnScreen();
}

bool wxWindowMac::OSXHandleKeyEvent( wxKeyEvent& event )
{
    bool handled = HandleWindowEvent( event ) ;
    if ( handled && event.GetSkipped() )
        handled = false ;

#if wxUSE_ACCEL
    if ( !handled && event.GetEventType() == wxEVT_KEY_DOWN)
    {
        wxWindow *ancestor = this;
        while (ancestor)
        {
            int command = ancestor->GetAcceleratorTable()->GetCommand( event );
            if (command != -1)
            {
                wxEvtHandler * const handler = ancestor->GetEventHandler();

                wxCommandEvent command_event( wxEVT_COMMAND_MENU_SELECTED, command );
                handled = handler->ProcessEvent( command_event );

                if ( !handled )
                {
                    // accelerators can also be used with buttons, try them too
                    command_event.SetEventType(wxEVT_COMMAND_BUTTON_CLICKED);
                    handled = handler->ProcessEvent( command_event );
                }

                break;
            }

            if (ancestor->IsTopLevel())
                break;

            ancestor = ancestor->GetParent();
        }
    }
#endif // wxUSE_ACCEL

    return handled ;
}

//
// wxWidgetImpl 
//

WX_DECLARE_HASH_MAP(WXWidget, wxWidgetImpl*, wxPointerHash, wxPointerEqual, MacControlMap);

static MacControlMap wxWinMacControlList;

wxWindowMac *wxFindWindowFromWXWidget(WXWidget inControl )
{
    wxWidgetImpl* impl = wxWidgetImpl::FindFromWXWidget( inControl );
    if ( impl )
        return impl->GetWXPeer();
    
    return NULL;
}

wxWidgetImpl *wxWidgetImpl::FindFromWXWidget(WXWidget inControl )
{
    MacControlMap::iterator node = wxWinMacControlList.find(inControl);

    return (node == wxWinMacControlList.end()) ? NULL : node->second;
}

void wxWidgetImpl::Associate(WXWidget inControl, wxWidgetImpl *impl)
{
    // adding NULL ControlRef is (first) surely a result of an error and
    // (secondly) breaks native event processing
    wxCHECK_RET( inControl != (WXWidget) NULL, wxT("attempt to add a NULL WXWidget to control map") );

    wxWinMacControlList[inControl] = impl;
}

void wxWidgetImpl::RemoveAssociations(wxWidgetImpl* impl)
{
   // iterate over all the elements in the class
    // is the iterator stable ? as we might have two associations pointing to the same wxWindow
    // we should go on...

    bool found = true ;
    while ( found )
    {
        found = false ;
        MacControlMap::iterator it;
        for ( it = wxWinMacControlList.begin(); it != wxWinMacControlList.end(); ++it )
        {
            if ( it->second == impl )
            {
                wxWinMacControlList.erase(it);
                found = true ;
                break;
            }
        }
    }
}

IMPLEMENT_ABSTRACT_CLASS( wxWidgetImpl , wxObject )

wxWidgetImpl::wxWidgetImpl( wxWindowMac* peer , bool isRootControl )
{
    Init();
    m_isRootControl = isRootControl;
    m_wxPeer = peer;
}

wxWidgetImpl::wxWidgetImpl()
{
    Init();
}

wxWidgetImpl::~wxWidgetImpl()
{
}

void wxWidgetImpl::Init()
{
    m_isRootControl = false;
    m_wxPeer = NULL;
    m_needsFocusRect = false;
    m_needsFrame = true;
}

void wxWidgetImpl::SetNeedsFocusRect( bool needs )
{
    m_needsFocusRect = needs;
}

bool wxWidgetImpl::NeedsFocusRect() const
{
    return m_needsFocusRect;
}

void wxWidgetImpl::SetNeedsFrame( bool needs )
{
    m_needsFrame = needs;
}

bool wxWidgetImpl::NeedsFrame() const
{
    return m_needsFrame;
}
