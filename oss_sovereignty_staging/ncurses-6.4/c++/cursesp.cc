
 

 

#include "internal.h"
#include "cursesp.h"

MODULE_ID("$Id: cursesp.cc,v 1.27 2020/02/02 23:34:34 tom Exp $")

NCursesPanel* NCursesPanel::dummy = static_cast<NCursesPanel*>(0);

void NCursesPanel::init()
{
  p = ::new_panel(w);
  if (!p)
    OnError(ERR);

  UserHook* hook = new UserHook;
  hook->m_user  = NULL;
  hook->m_back  = this;
  hook->m_owner = p;
  ::set_panel_userptr(p, reinterpret_cast<void *>(hook));
}

NCursesPanel::~NCursesPanel() THROWS(NCursesException)
{
  UserHook* hook = UserPointer();
  assert(hook != 0 && hook->m_back==this && hook->m_owner==p);
  delete hook;
  ::del_panel(p);
  ::update_panels();
}

void
NCursesPanel::redraw()
{
  PANEL *pan;

  pan = ::panel_above(NULL);
  while (pan) {
    ::touchwin(panel_window(pan));
    pan = ::panel_above(pan);
  }
  ::update_panels();
  ::doupdate();
}

int
NCursesPanel::refresh()
{
  ::update_panels();
  return ::doupdate();
}

int
NCursesPanel::noutrefresh()
{
  ::update_panels();
  return OK;
}

void
NCursesPanel::boldframe(const char *title, const char* btitle)
{
  standout();
  frame(title, btitle);
  standend();
}

void
NCursesPanel::frame(const char *title,const char *btitle)
{
  int err = OK;
  if (!title && !btitle) {
    err = box();
  }
  else {
    err = box();
    if (err==OK)
      label(title,btitle);
  }
  OnError(err);
}

void
NCursesPanel::label(const char *tLabel, const char *bLabel)
{
  if (tLabel)
    centertext(0,tLabel);
  if (bLabel)
    centertext(maxy(),bLabel);
}

void
NCursesPanel::centertext(int row,const char *labelText)
{
  if (labelText) {
    int x = (maxx() - ::strlen(labelText)) / 2;
    if (x<0)
      x=0;
    OnError(addstr(row, x, labelText, width()));
  }
}

int
NCursesPanel::getKey(void)
{
  return getch();
}
