
 

 



#ifndef NCURSES_CURSESAPP_H_incl
#define NCURSES_CURSESAPP_H_incl

#include <cursslk.h>

#if (defined(_WIN32) || defined(_WIN64))
# define NCURSES_CXX_MAIN_NAME cursespp_main
# define NCURSES_CXX_MAIN \
  int main(int argc, char *argv[]) { \
  	return NCURSES_CXX_MAIN_NAME(argc, argv); \
  }
#else
# define NCURSES_CXX_MAIN_NAME main
#endif
NCURSES_CXX_IMPEXP int NCURSES_CXX_MAIN_NAME(int argc, char *argv[]);

class NCURSES_CXX_IMPEXP NCursesApplication {
public:
  typedef struct _slk_link {          
    struct _slk_link* prev;           
    Soft_Label_Key_Set* SLKs;
  } SLK_Link;
private:
  static int rinit(NCursesWindow& w); 
  static NCursesApplication* theApp;  

  static SLK_Link* slk_stack;

protected:
  static NCursesWindow* titleWindow;  

  bool b_Colors;                      
  NCursesWindow* Root_Window;         

  
  
  virtual void init(bool bColors);

  
  
  virtual int titlesize() const {
    return 0;
  }

  
  
  virtual void title() {
  }

  
  
  virtual Soft_Label_Key_Set::Label_Layout useSLKs() const {
    return Soft_Label_Key_Set::None;
  }

  
  
  virtual void init_labels(Soft_Label_Key_Set& S) const {
    (void) S;
  }

  
  
  virtual int run() = 0;

  
  
  NCursesApplication(bool wantColors = FALSE);

  NCursesApplication& operator=(const NCursesApplication& rhs)
  {
    if (this != &rhs) {
      *this = rhs;
    }
    return *this;
  }

  NCursesApplication(const NCursesApplication& rhs)
    : b_Colors(rhs.b_Colors),
      Root_Window(rhs.Root_Window)
  {
  }

  static NCursesWindow *&getTitleWindow();

public:
  virtual ~NCursesApplication() THROWS(NCursesException);

  
  static NCursesApplication* getApplication();

  
  int operator()(void);

  
  
  virtual void handleArgs(int argc, char* argv[]) {
    (void) argc;
    (void) argv;
  }

  
  inline bool useColors() const {
    return b_Colors;
  }

  
  
  void push(Soft_Label_Key_Set& S);

  
  
  bool pop();

  
  Soft_Label_Key_Set* top() const;

  
  virtual chtype foregrounds() const {
    return b_Colors ? static_cast<chtype>(COLOR_PAIR(1)) : A_BOLD;
  }

  
  virtual chtype backgrounds() const {
    return b_Colors ? static_cast<chtype>(COLOR_PAIR(2)) : A_NORMAL;
  }

  
  virtual chtype inactives() const {
    return b_Colors ? static_cast<chtype>(COLOR_PAIR(3)|A_DIM) : A_DIM;
  }

  
  virtual chtype labels() const {
    return b_Colors ? static_cast<chtype>(COLOR_PAIR(4)) : A_NORMAL;
  }

  
  virtual chtype dialog_backgrounds() const {
    return b_Colors ? static_cast<chtype>(COLOR_PAIR(4)) : A_NORMAL;
  }

  
  virtual chtype window_backgrounds() const {
    return b_Colors ? static_cast<chtype>(COLOR_PAIR(5)) : A_NORMAL;
  }

  
  virtual chtype screen_titles() const {
    return b_Colors ? static_cast<chtype>(COLOR_PAIR(6)) : A_BOLD;
  }

};

#endif  
