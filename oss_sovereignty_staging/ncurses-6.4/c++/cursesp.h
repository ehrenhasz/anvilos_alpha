

 

 

#ifndef NCURSES_CURSESP_H_incl
#define NCURSES_CURSESP_H_incl 1



#include <cursesw.h>

extern "C" {
#  include <panel.h>
}

class NCURSES_CXX_IMPEXP NCursesPanel
  : public NCursesWindow
{
protected:
  PANEL *p;
  static NCursesPanel *dummy;

private:
  
  
  typedef struct {
    void*               m_user;      
    const NCursesPanel* m_back;      
    const PANEL*        m_owner;     
  } UserHook;

  inline UserHook *UserPointer()
  {
    UserHook* uptr = reinterpret_cast<UserHook*>(
                           const_cast<void *>(::panel_userptr (p)));
    return uptr;
  }

  void init();                       

protected:
  void set_user(void *user)
  {
    UserHook* uptr = UserPointer();
    if (uptr != 0 && uptr->m_back==this && uptr->m_owner==p) {
      uptr->m_user = user;
    }
  }
  

  void *get_user()
  {
    UserHook* uptr = UserPointer();
    void *result = 0;
    if (uptr != 0 && uptr->m_back==this && uptr->m_owner==p)
      result = uptr->m_user;
    return result;
  }

  void OnError (int err) const THROW2(NCursesException const, NCursesPanelException)
  {
    if (err==ERR)
      THROW(new NCursesPanelException (this, err));
  }
  
  

  
  virtual int getKey(void);

public:
  NCursesPanel(int nlines,
	       int ncols,
	       int begin_y = 0,
	       int begin_x = 0)
    : NCursesWindow(nlines,ncols,begin_y,begin_x), p(0)
  {
    init();
  }
  

  NCursesPanel()
    : NCursesWindow(::stdscr), p(0)
  {
    init();
  }
  
  

  NCursesPanel& operator=(const NCursesPanel& rhs)
  {
    if (this != &rhs) {
      *this = rhs;
      NCursesWindow::operator=(rhs);
    }
    return *this;
  }

  NCursesPanel(const NCursesPanel& rhs)
    : NCursesWindow(rhs),
      p(rhs.p)
  {
  }

  virtual ~NCursesPanel() THROWS(NCursesException);

  
  inline void hide()
  {
    OnError (::hide_panel(p));
  }
  

  inline void show()
  {
    OnError (::show_panel(p));
  }
  

  inline void top()
  {
    OnError (::top_panel(p));
  }
  

  inline void bottom()
  {
    OnError (::bottom_panel(p));
  }
  
  
  

  virtual int mvwin(int y, int x) NCURSES_OVERRIDE
  {
    OnError(::move_panel(p, y, x));
    return OK;
  }

  inline bool hidden() const
  {
    return (::panel_hidden (p) ? TRUE : FALSE);
  }
  

 
  inline NCursesPanel& above() const
  {
    OnError(ERR);
    return *dummy;
  }

  inline NCursesPanel& below() const
  {
    OnError(ERR);
    return *dummy;
  }

  
  
  virtual int refresh() NCURSES_OVERRIDE;
  
  

  virtual int noutrefresh() NCURSES_OVERRIDE;
  

  static void redraw();
  

  
  virtual void frame(const char* title=NULL,
		     const char* btitle=NULL);
  
  

  virtual void boldframe(const char* title=NULL,
			 const char* btitle=NULL);
  

  virtual void label(const char* topLabel,
		     const char* bottomLabel);
  

  virtual void centertext(int row,const char* label);
  
};

 
template<class T> class NCursesUserPanel : public NCursesPanel
{
public:
  NCursesUserPanel (int nlines,
		    int ncols,
		    int begin_y = 0,
		    int begin_x = 0,
		    const T* p_UserData = STATIC_CAST(T*)(0))
    : NCursesPanel (nlines, ncols, begin_y, begin_x)
  {
      if (p)
	set_user (const_cast<void *>(reinterpret_cast<const void*>
				     (p_UserData)));
  };
  
  

  explicit NCursesUserPanel(const T* p_UserData = STATIC_CAST(T*)(0)) : NCursesPanel()
  {
    if (p)
      set_user(const_cast<void *>(reinterpret_cast<const void*>(p_UserData)));
  };
  
  

  virtual ~NCursesUserPanel() THROWS(NCursesException) {};

  T* UserData (void)
  {
    return reinterpret_cast<T*>(get_user ());
  };
  

  virtual void setUserData (const T* p_UserData)
  {
    if (p)
      set_user (const_cast<void *>(reinterpret_cast<const void*>(p_UserData)));
  }
  
};

#endif  
