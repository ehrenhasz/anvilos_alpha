
 

 



#ifndef NCURSES_CURSESM_H_incl
#define NCURSES_CURSESM_H_incl 1

#include <cursesp.h>

extern "C" {
#  include <menu.h>
}





class NCURSES_CXX_IMPEXP NCursesMenuItem
{
  friend class NCursesMenu;

protected:
  ITEM *item;

  inline void OnError (int err) const THROW2(NCursesException const, NCursesMenuException) {
    if (err != E_OK)
      THROW(new NCursesMenuException (err));
  }

public:
  NCursesMenuItem (const char* p_name     = NULL,
		   const char* p_descript = NULL)
    : item(0)
  {
    item = p_name ? ::new_item (p_name, p_descript) : STATIC_CAST(ITEM*)(0);
    if (p_name && !item)
      OnError (E_SYSTEM_ERROR);
  }
  
  
  

  NCursesMenuItem& operator=(const NCursesMenuItem& rhs)
  {
    if (this != &rhs) {
      *this = rhs;
    }
    return *this;
  }

  NCursesMenuItem(const NCursesMenuItem& rhs)
    : item(0)
  {
    (void) rhs;
  }

  virtual ~NCursesMenuItem () THROWS(NCursesException);
  

  inline const char* name () const {
    return ::item_name (item);
  }
  

  inline const char* description () const {
    return ::item_description (item);
  }
  

  inline int (index) (void) const {
    return ::item_index (item);
  }
  

  inline void options_on (Item_Options opts) {
    OnError (::item_opts_on (item, opts));
  }
  

  inline void options_off (Item_Options opts) {
    OnError (::item_opts_off (item, opts));
  }
  

  inline Item_Options options () const {
    return ::item_opts (item);
  }
  

  inline void set_options (Item_Options opts) {
    OnError (::set_item_opts (item, opts));
  }
  

  inline void set_value (bool f) {
    OnError (::set_item_value (item,f));
  }
  

  inline bool value () const {
    return ::item_value (item);
  }
  

  inline bool visible () const {
    return ::item_visible (item);
  }
  

  virtual bool action();
  
  
  
  
  
};


typedef bool ITEMCALLBACK(NCursesMenuItem&);




class NCURSES_CXX_IMPEXP NCursesMenuCallbackItem : public NCursesMenuItem
{
private:
  ITEMCALLBACK* p_fct;

public:
  NCursesMenuCallbackItem(ITEMCALLBACK* fct       = NULL,
			  const char* p_name      = NULL,
			  const char* p_descript  = NULL )
    : NCursesMenuItem (p_name, p_descript),
      p_fct (fct) {
  }

  NCursesMenuCallbackItem& operator=(const NCursesMenuCallbackItem& rhs)
  {
    if (this != &rhs) {
      *this = rhs;
    }
    return *this;
  }

  NCursesMenuCallbackItem(const NCursesMenuCallbackItem& rhs)
    : NCursesMenuItem(rhs),
      p_fct(0)
  {
  }

  virtual ~NCursesMenuCallbackItem() THROWS(NCursesException);

  bool action() NCURSES_OVERRIDE;
};

  
  
  
extern "C" {
  void _nc_xx_mnu_init(MENU *);
  void _nc_xx_mnu_term(MENU *);
  void _nc_xx_itm_init(MENU *);
  void _nc_xx_itm_term(MENU *);
}






class NCURSES_CXX_IMPEXP NCursesMenu : public NCursesPanel
{
protected:
  MENU *menu;

private:
  NCursesWindow* sub;   
  bool b_sub_owner;     
  bool b_framed;        
  bool b_autoDelete;    

  NCursesMenuItem** my_items; 

  
  
  typedef struct {
    void*              m_user;      
    const NCursesMenu* m_back;      
    const MENU*        m_owner;
  } UserHook;

  
  static inline NCursesMenu* getHook(const MENU *m) {
    UserHook* hook = STATIC_CAST(UserHook*)(::menu_userptr(m));
    assert(hook != 0 && hook->m_owner==m);
    return const_cast<NCursesMenu*>(hook->m_back);
  }

  friend void _nc_xx_mnu_init(MENU *);
  friend void _nc_xx_mnu_term(MENU *);
  friend void _nc_xx_itm_init(MENU *);
  friend void _nc_xx_itm_term(MENU *);

  
  ITEM** mapItems(NCursesMenuItem* nitems[]);

protected:
  
  inline void set_user(void *user) {
    UserHook* uptr = STATIC_CAST(UserHook*)(::menu_userptr (menu));
    assert (uptr != 0 && uptr->m_back==this && uptr->m_owner==menu);
    uptr->m_user = user;
  }

  inline void *get_user() {
    UserHook* uptr = STATIC_CAST(UserHook*)(::menu_userptr (menu));
    assert (uptr != 0 && uptr->m_back==this && uptr->m_owner==menu);
    return uptr->m_user;
  }

  void InitMenu (NCursesMenuItem* menu[],
		 bool with_frame,
		 bool autoDeleteItems);

  inline void OnError (int err) const THROW2(NCursesException const, NCursesMenuException) {
    if (err != E_OK)
      THROW(new NCursesMenuException (this, err));
  }

  
  virtual int driver (int c) ;

  
  
  NCursesMenu( int  nlines,
	       int  ncols,
	       int  begin_y = 0,
	       int  begin_x = 0)
    : NCursesPanel(nlines,ncols,begin_y,begin_x),
      menu (STATIC_CAST(MENU*)(0)),
      sub(0),
      b_sub_owner(0),
      b_framed(0),
      b_autoDelete(0),
      my_items(0)
  {
  }

public:
  
  NCursesMenu (NCursesMenuItem* Items[],
	       bool with_frame=FALSE,        
	       bool autoDelete_Items=FALSE)  
    : NCursesPanel(),
      menu(0),
      sub(0),
      b_sub_owner(0),
      b_framed(0),
      b_autoDelete(0),
      my_items(0)
  {
      InitMenu(Items, with_frame, autoDelete_Items);
  }

  
  NCursesMenu (NCursesMenuItem* Items[],
	       int  nlines,
	       int  ncols,
	       int  begin_y = 0,
	       int  begin_x = 0,
	       bool with_frame=FALSE,        
	       bool autoDelete_Items=FALSE)  
    : NCursesPanel(nlines, ncols, begin_y, begin_x),
      menu(0),
      sub(0),
      b_sub_owner(0),
      b_framed(0),
      b_autoDelete(0),
      my_items(0)
  {
      InitMenu(Items, with_frame, autoDelete_Items);
  }

  NCursesMenu& operator=(const NCursesMenu& rhs)
  {
    if (this != &rhs) {
      *this = rhs;
      NCursesPanel::operator=(rhs);
    }
    return *this;
  }

  NCursesMenu(const NCursesMenu& rhs)
    : NCursesPanel(rhs),
      menu(rhs.menu),
      sub(rhs.sub),
      b_sub_owner(rhs.b_sub_owner),
      b_framed(rhs.b_framed),
      b_autoDelete(rhs.b_autoDelete),
      my_items(rhs.my_items)
  {
  }

  virtual ~NCursesMenu () THROWS(NCursesException);

  
  inline NCursesWindow& subWindow() const {
    assert(sub!=NULL);
    return *sub;
  }

  
  void setSubWindow(NCursesWindow& sub);

  
  inline void setItems(NCursesMenuItem* Items[]) {
    OnError(::set_menu_items(menu,mapItems(Items)));
  }

  
  inline void unpost (void) {
    OnError (::unpost_menu (menu));
  }

  
  inline void post(bool flag = TRUE) {
    flag ? OnError (::post_menu(menu)) : OnError (::unpost_menu (menu));
  }

  
  inline void scale (int& mrows, int& mcols) const  {
    OnError (::scale_menu (menu, &mrows, &mcols));
  }

  
  inline void set_format(int mrows, int mcols) {
    OnError (::set_menu_format(menu, mrows, mcols));
  }

  
  inline void menu_format(int& rows,int& ncols) {
    ::menu_format(menu,&rows,&ncols);
  }

  
  inline NCursesMenuItem* items() const {
    return *my_items;
  }

  
  inline int count() const {
    return ::item_count(menu);
  }

  
  inline NCursesMenuItem* current_item() const {
    return my_items[::item_index(::current_item(menu))];
  }

  
  inline const char* mark() const {
    return ::menu_mark(menu);
  }

  
  inline void set_mark(const char *marker) {
    OnError (::set_menu_mark (menu, marker));
  }

  
  inline static const char* request_name(int c) {
    return ::menu_request_name(c);
  }

  
  inline char* pattern() const {
    return ::menu_pattern(menu);
  }

  
  bool set_pattern (const char *pat);

  
  
  virtual void setDefaultAttributes();

  
  inline chtype back() const {
    return ::menu_back(menu);
  }

  
  inline chtype fore() const {
    return ::menu_fore(menu);
  }

  
  inline chtype grey() const {
    return ::menu_grey(menu);
  }

  
  inline chtype set_background(chtype a) {
    return ::set_menu_back(menu,a);
  }

  
  inline chtype set_foreground(chtype a) {
    return ::set_menu_fore(menu,a);
  }

  
  inline chtype set_grey(chtype a) {
    return ::set_menu_grey(menu,a);
  }

  inline void options_on (Menu_Options opts) {
    OnError (::menu_opts_on (menu,opts));
  }

  inline void options_off(Menu_Options opts) {
    OnError (::menu_opts_off(menu,opts));
  }

  inline Menu_Options options() const {
    return ::menu_opts(menu);
  }

  inline void set_options (Menu_Options opts) {
    OnError (::set_menu_opts (menu,opts));
  }

  inline int pad() const {
    return ::menu_pad(menu);
  }

  inline void set_pad (int padch) {
    OnError (::set_menu_pad (menu, padch));
  }

  
  inline void position_cursor () const {
    OnError (::pos_menu_cursor (menu));
  }

  
  inline void set_current(NCursesMenuItem& I) {
    OnError (::set_current_item(menu, I.item));
  }

  
  inline int top_row (void) const {
    return ::top_row (menu);
  }

  
  inline void set_top_row (int row) {
    OnError (::set_top_row (menu, row));
  }

  
  
  inline void setSpacing(int spc_description,
			 int spc_rows,
			 int spc_columns) {
    OnError(::set_menu_spacing(menu,
			       spc_description,
			       spc_rows,
			       spc_columns));
  }

  
  inline void Spacing(int& spc_description,
		      int& spc_rows,
		      int& spc_columns) const {
    OnError(::menu_spacing(menu,
			   &spc_description,
			   &spc_rows,
			   &spc_columns));
  }

  
  inline void frame(const char *title=NULL, const char* btitle=NULL) NCURSES_OVERRIDE {
    if (b_framed)
      NCursesPanel::frame(title,btitle);
    else
      OnError(E_SYSTEM_ERROR);
  }

  inline void boldframe(const char *title=NULL, const char* btitle=NULL) NCURSES_OVERRIDE {
    if (b_framed)
      NCursesPanel::boldframe(title,btitle);
    else
      OnError(E_SYSTEM_ERROR);
  }

  inline void label(const char *topLabel, const char *bottomLabel) NCURSES_OVERRIDE {
    if (b_framed)
      NCursesPanel::label(topLabel,bottomLabel);
    else
      OnError(E_SYSTEM_ERROR);
  }

  
  
  

  
  
  virtual void On_Menu_Init();

  
  
  virtual void On_Menu_Termination();

  
  virtual void On_Item_Init(NCursesMenuItem& item);

  
  virtual void On_Item_Termination(NCursesMenuItem& item);

  
  
  
  
  virtual int virtualize(int c);


  
  inline NCursesMenuItem* operator[](int i) const {
    if ( (i < 0) || (i >= ::item_count (menu)) )
      OnError (E_BAD_ARGUMENT);
    return (my_items[i]);
  }

  
  
  
  virtual NCursesMenuItem* operator()(void);

  
  
  
  

  
  virtual void On_Request_Denied(int c) const;

  
  virtual void On_Not_Selectable(int c) const;

  
  virtual void On_No_Match(int c) const;

  
  virtual void On_Unknown_Command(int c) const;

};








template<class T> class NCURSES_CXX_IMPEXP NCursesUserItem : public NCursesMenuItem
{
public:
  NCursesUserItem (const char* p_name,
		   const char* p_descript = NULL,
		   const T* p_UserData    = STATIC_CAST(T*)(0))
    : NCursesMenuItem (p_name, p_descript) {
      if (item)
	OnError (::set_item_userptr (item, const_cast<void *>(reinterpret_cast<const void*>(p_UserData))));
  }

  virtual ~NCursesUserItem() THROWS(NCursesException) {}

  inline const T* UserData (void) const {
    return reinterpret_cast<const T*>(::item_userptr (item));
  };

  inline virtual void setUserData(const T* p_UserData) {
    if (item)
      OnError (::set_item_userptr (item, const_cast<void *>(reinterpret_cast<const void *>(p_UserData))));
  }
};





template<class T> class NCURSES_CXX_IMPEXP NCursesUserMenu : public NCursesMenu
{
protected:
  NCursesUserMenu( int  nlines,
		   int  ncols,
		   int  begin_y = 0,
		   int  begin_x = 0,
		   const T* p_UserData = STATIC_CAST(T*)(0))
    : NCursesMenu(nlines,ncols,begin_y,begin_x) {
      if (menu)
	set_user (const_cast<void *>(reinterpret_cast<const void*>(p_UserData)));
  }

public:
  NCursesUserMenu (NCursesMenuItem* Items[],
		   const T* p_UserData = STATIC_CAST(T*)(0),
		   bool with_frame=FALSE,
		   bool autoDelete_Items=FALSE)
    : NCursesMenu (Items, with_frame, autoDelete_Items) {
      if (menu)
	set_user (const_cast<void *>(reinterpret_cast<const void*>(p_UserData)));
  };

  NCursesUserMenu (NCursesMenuItem* Items[],
		   int nlines,
		   int ncols,
		   int begin_y = 0,
		   int begin_x = 0,
		   const T* p_UserData = STATIC_CAST(T*)(0),
		   bool with_frame=FALSE)
    : NCursesMenu (Items, nlines, ncols, begin_y, begin_x, with_frame) {
      if (menu)
	set_user (const_cast<void *>(reinterpret_cast<const void*>(p_UserData)));
  };

  virtual ~NCursesUserMenu() THROWS(NCursesException) {
  };

  inline T* UserData (void) {
    return reinterpret_cast<T*>(get_user ());
  };

  inline virtual void setUserData (const T* p_UserData) {
    if (menu)
      set_user (const_cast<void *>(reinterpret_cast<const void*>(p_UserData)));
  }
};

#endif  
