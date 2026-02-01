
 

 

#include "internal.h"
#include "cursesapp.h"

MODULE_ID("$Id: cursesapp.cc,v 1.18 2020/07/18 19:57:11 anonymous.maarten Exp $")

void
NCursesApplication::init(bool bColors)
{
  if (bColors)
    NCursesWindow::useColors();

  if (Root_Window->colors() > 1) {
    b_Colors = TRUE;
    Root_Window->setcolor(1);
    Root_Window->setpalette(COLOR_YELLOW,COLOR_BLUE);
    Root_Window->setcolor(2);
    Root_Window->setpalette(COLOR_CYAN,COLOR_BLUE);
    Root_Window->setcolor(3);
    Root_Window->setpalette(COLOR_BLACK,COLOR_BLUE);
    Root_Window->setcolor(4);
    Root_Window->setpalette(COLOR_BLACK,COLOR_CYAN);
    Root_Window->setcolor(5);
    Root_Window->setpalette(COLOR_BLUE,COLOR_YELLOW);
    Root_Window->setcolor(6);
    Root_Window->setpalette(COLOR_BLACK,COLOR_GREEN);
  }
  else
    b_Colors = FALSE;

  Root_Window->bkgd(' '|window_backgrounds());
}

NCursesApplication* NCursesApplication::theApp = 0;
NCursesWindow* NCursesApplication::titleWindow = 0;
NCursesApplication::SLK_Link* NCursesApplication::slk_stack = 0;


NCursesWindow *&NCursesApplication::getTitleWindow() {
  return titleWindow;
}

NCursesApplication::~NCursesApplication() THROWS(NCursesException)
{
  Soft_Label_Key_Set* S;

  delete titleWindow;
  titleWindow = 0;

  while( (S=top()) ) {
    pop();
    delete S;
  }

  delete Root_Window;
  Root_Window = 0;

  ::endwin();
}

NCursesApplication* NCursesApplication::getApplication() {
  return theApp;
}

int NCursesApplication::rinit(NCursesWindow& w)
{
  titleWindow = &w;
  return OK;
}

void NCursesApplication::push(Soft_Label_Key_Set& S)
{
  SLK_Link* L = new SLK_Link;
  assert(L != 0);
  L->prev = slk_stack;
  L->SLKs = &S;
  slk_stack = L;
  if (Root_Window)
    S.show();
}

bool NCursesApplication::pop()
{
  if (slk_stack) {
    SLK_Link* L = slk_stack;
    slk_stack = slk_stack->prev;
    delete L;
    if (Root_Window) {
      Soft_Label_Key_Set* xx = top();
      if (xx != 0)
        xx->show();
    }
  }
  return (slk_stack ? FALSE : TRUE);
}

Soft_Label_Key_Set* NCursesApplication::top() const
{
  if (slk_stack)
    return slk_stack->SLKs;
  else
    return static_cast<Soft_Label_Key_Set*>(0);
}

int NCursesApplication::operator()(void)
{
  bool bColors = b_Colors;
  Soft_Label_Key_Set* S = 0;

  int ts = titlesize();
  if (ts>0)
    NCursesWindow::ripoffline(ts,rinit);
  Soft_Label_Key_Set::Label_Layout fmt = useSLKs();
  if (fmt!=Soft_Label_Key_Set::None) {
    S = new Soft_Label_Key_Set(fmt);
    assert(S != 0);
    init_labels(*S);
  }

  Root_Window = new NCursesWindow(::stdscr);
  init(bColors);

  if (ts>0)
    title();
  if (fmt!=Soft_Label_Key_Set::None) {
    push(*S);
  }

  return run();
}

NCursesApplication::NCursesApplication(bool bColors)
  : b_Colors(bColors),
    Root_Window(NULL)
{
  if (theApp)
    THROW(new NCursesException("Application object already created."));
  else
    theApp = this;
}
