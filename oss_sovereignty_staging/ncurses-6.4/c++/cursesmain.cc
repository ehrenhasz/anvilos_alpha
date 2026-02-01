
 

 

#include "internal.h"
#include "cursesapp.h"

#if CPP_HAS_TRY_CATCH && HAVE_IOSTREAM
#pragma GCC diagnostic ignored "-Weffc++"
#include <iostream>
#pragma GCC diagnostic warning "-Weffc++"
#else
#undef CPP_HAS_TRY_CATCH
#define CPP_HAS_TRY_CATCH 0
#endif

MODULE_ID("$Id: cursesmain.cc,v 1.20 2020/07/18 19:57:11 anonymous.maarten Exp $")

#if HAVE_LOCALE_H
#include <locale.h>
#else
#define setlocale(name,string)  
#endif

#if NO_LEAKS
#include <nc_alloc.h>
#endif

 
int NCURSES_CXX_MAIN_NAME(int argc, char* argv[])
{
  setlocale(LC_ALL, "");

  NCursesApplication* A = NCursesApplication::getApplication();
  if (!A)
    return(1);
  else {
    int res;

    A->handleArgs(argc,argv);
    ::endwin();
#if CPP_HAS_TRY_CATCH
    try {
      res = (*A)();
      ::endwin();
    }
    catch(const NCursesException &e) {
      ::endwin();
      std::cerr << e.message << std::endl;
      res = e.errorno;
    }
#else
    res = (*A)();
    ::endwin();
#endif
#if NO_LEAKS
    delete A;
    exit_curses(res);
#else
    return(res);
#endif
  }
}
