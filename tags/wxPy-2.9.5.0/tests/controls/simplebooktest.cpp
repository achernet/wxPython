///////////////////////////////////////////////////////////////////////////////
// Name:        tests/controls/simplebooktest.cpp
// Purpose:     wxSimplebook unit test
// Author:      Vadim Zeitlin
// Created:     2013-06-23
// RCS-ID:      $Id$
// Copyright:   (c) 2013 Vadim Zeitlin <vadim@wxwidgets.org>
///////////////////////////////////////////////////////////////////////////////

#include "testprec.h"

#if wxUSE_BOOKCTRL

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/panel.h"
#endif // WX_PRECOMP

#include "wx/simplebook.h"
#include "bookctrlbasetest.h"

class SimplebookTestCase : public BookCtrlBaseTestCase, public CppUnit::TestCase
{
public:
    SimplebookTestCase() { }

    virtual void setUp();
    virtual void tearDown();

private:
    virtual wxBookCtrlBase *GetBase() const { return m_simplebook; }

    virtual wxEventType GetChangedEvent() const
        { return wxEVT_BOOKCTRL_PAGE_CHANGED; }

    virtual wxEventType GetChangingEvent() const
        { return wxEVT_BOOKCTRL_PAGE_CHANGING; }

    CPPUNIT_TEST_SUITE( SimplebookTestCase );
        wxBOOK_CTRL_BASE_TESTS();
    CPPUNIT_TEST_SUITE_END();

    wxSimplebook *m_simplebook;

    DECLARE_NO_COPY_CLASS(SimplebookTestCase)
};

// register in the unnamed registry so that these tests are run by default
CPPUNIT_TEST_SUITE_REGISTRATION( SimplebookTestCase );

// also include in its own registry so that these tests can be run alone
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( SimplebookTestCase, "SimplebookTestCase" );

void SimplebookTestCase::setUp()
{
    m_simplebook = new wxSimplebook(wxTheApp->GetTopWindow(), wxID_ANY);
    AddPanels();
}

void SimplebookTestCase::tearDown()
{
    wxDELETE(m_simplebook);
}

#endif // wxUSE_BOOKCTRL

