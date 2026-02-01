

 

 



#ifndef NCURSES_CURSSLK_H_incl
#define NCURSES_CURSSLK_H_incl

#include <cursesw.h>

class NCURSES_CXX_IMPEXP Soft_Label_Key_Set {
public:
  
  class NCURSES_CXX_IMPEXP Soft_Label_Key {
    friend class Soft_Label_Key_Set;
  public:
    typedef enum { Left=0, Center=1, Right=2 } Justification;

  private:
    char *label;           
    Justification format;  
    int num;               

    Soft_Label_Key() : label(NULL), format(Left), num(-1) {
    }

    virtual ~Soft_Label_Key() {
      delete[] label;
    };

  public:
    
    Soft_Label_Key& operator=(char *text);

    
    Soft_Label_Key& operator=(Justification just) {
      format = just;
      return *this;
    }

    
    inline char* operator()(void) const {
      return label;
    }

    Soft_Label_Key& operator=(const Soft_Label_Key& rhs)
    {
      if (this != &rhs) {
        *this = rhs;
      }
      return *this;
    }

    Soft_Label_Key(const Soft_Label_Key& rhs)
      : label(NULL),
        format(rhs.format),
        num(rhs.num)
    {
      *this = rhs.label;
    }
  };

public:
  typedef enum {
    None                = -1,
    Three_Two_Three     = 0,
    Four_Four           = 1,
    PC_Style            = 2,
    PC_Style_With_Index = 3
  } Label_Layout;

private:
  static long count;               
  static Label_Layout  format;     
  static int  num_labels;          
  bool b_attrInit;                 

  Soft_Label_Key *slk_array;       

  
  void init();

  
  void activate_label(int i, bool bf=TRUE);

  
  void activate_labels(bool bf);

protected:
  inline void Error (const char* msg) const THROWS(NCursesException) {
    THROW(new NCursesException (msg));
  }

  
  void clear() {
    if (ERR==::slk_clear())
      Error("slk_clear");
  }

  
  void restore() {
    if (ERR==::slk_restore())
      Error("slk_restore");
  }

public:

  
  
  
  
  explicit Soft_Label_Key_Set(Label_Layout fmt);

  
  
  Soft_Label_Key_Set();

  Soft_Label_Key_Set& operator=(const Soft_Label_Key_Set& rhs)
  {
    if (this != &rhs) {
      *this = rhs;
      init();		
    }
    return *this;
  }

  Soft_Label_Key_Set(const Soft_Label_Key_Set& rhs)
    : b_attrInit(rhs.b_attrInit),
      slk_array(NULL)
  {
    init();		
  }

  virtual ~Soft_Label_Key_Set() THROWS(NCursesException);

  
  Soft_Label_Key& operator[](int i);

  
  int labels() const;

  
  inline void refresh() {
    if (ERR==::slk_refresh())
      Error("slk_refresh");
  }

  
  
  inline void noutrefresh() {
    if (ERR==::slk_noutrefresh())
      Error("slk_noutrefresh");
  }

  
  inline void touch() {
    if (ERR==::slk_touch())
      Error("slk_touch");
  }

  
  inline void show(int i) {
    activate_label(i,FALSE);
    activate_label(i,TRUE);
  }

  
  inline void hide(int i) {
    activate_label(i,FALSE);
  }

  
  inline void show() {
    activate_labels(FALSE);
    activate_labels(TRUE);
  }

  
  inline void hide() {
    activate_labels(FALSE);
  }

  inline void attron(attr_t attrs) {
    if (ERR==::slk_attron(attrs))
      Error("slk_attron");
  }

  inline void attroff(attr_t attrs) {
    if (ERR==::slk_attroff(attrs))
      Error("slk_attroff");
  }

  inline void attrset(attr_t attrs) {
    if (ERR==::slk_attrset(attrs))
      Error("slk_attrset");
  }

  inline void color(short color_pair_number) {
    if (ERR==::slk_color(color_pair_number))
      Error("slk_color");
  }

  inline attr_t attr() const {
    return ::slk_attr();
  }
};

#endif  
