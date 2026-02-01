

 

 



#ifndef NCURSES_CURSESF_H_incl
#define NCURSES_CURSESF_H_incl 1

#include <cursesp.h>

#ifndef __EXT_QNX
#include <string.h>
#endif

extern "C" {
#  include <form.h>
}





class NCURSES_CXX_IMPEXP NCursesFormField; 



class NCURSES_CXX_IMPEXP NCursesFieldType
{
  friend class NCursesFormField;

protected:
  FIELDTYPE* fieldtype;

  inline void OnError(int err) const THROW2(NCursesException const, NCursesFormException) {
    if (err!=E_OK)
      THROW(new NCursesFormException (err));
  }

  NCursesFieldType(FIELDTYPE *f) : fieldtype(f) {
  }

  virtual ~NCursesFieldType() {}

  
  virtual void set(NCursesFormField& f) = 0;

public:
  NCursesFieldType()
    : fieldtype(STATIC_CAST(FIELDTYPE*)(0))
  {
  }

  NCursesFieldType& operator=(const NCursesFieldType& rhs)
  {
    if (this != &rhs) {
      *this = rhs;
    }
    return *this;
  }

  NCursesFieldType(const NCursesFieldType& rhs)
    : fieldtype(rhs.fieldtype)
  {
  }

};






class NCURSES_CXX_IMPEXP NCursesFormField
{
  friend class NCursesForm;

protected:
  FIELD *field;		     
  NCursesFieldType* ftype;   

  
  inline void OnError (int err) const THROW2(NCursesException const, NCursesFormException) {
    if (err != E_OK)
      THROW(new NCursesFormException (err));
  }

public:
  
  NCursesFormField()
    : field(STATIC_CAST(FIELD*)(0)),
      ftype(STATIC_CAST(NCursesFieldType*)(0))
  {
  }

  
  NCursesFormField (int rows,
		    int ncols,
		    int first_row = 0,
		    int first_col = 0,
		    int offscreen_rows = 0,
		    int additional_buffers = 0)
    : field(0),
      ftype(STATIC_CAST(NCursesFieldType*)(0))
  {
      field = ::new_field(rows, ncols, first_row, first_col,
			  offscreen_rows, additional_buffers);
      if (!field)
	OnError(errno);
  }

  NCursesFormField& operator=(const NCursesFormField& rhs)
  {
    if (this != &rhs) {
      *this = rhs;
    }
    return *this;
  }

  NCursesFormField(const NCursesFormField& rhs)
    : field(rhs.field), ftype(rhs.ftype)
  {
  }

  virtual ~NCursesFormField () THROWS(NCursesException);

  
  inline NCursesFormField* dup(int first_row, int first_col)
  {
    NCursesFormField* f = new NCursesFormField();
    if (!f)
      OnError(E_SYSTEM_ERROR);
    else {
      f->ftype = ftype;
      f->field = ::dup_field(field,first_row,first_col);
      if (!f->field)
	OnError(errno);
    }
    return f;
  }

  
  inline NCursesFormField* link(int first_row, int first_col) {
    NCursesFormField* f = new NCursesFormField();
    if (!f)
      OnError(E_SYSTEM_ERROR);
    else {
      f->ftype = ftype;
      f->field = ::link_field(field,first_row,first_col);
      if (!f->field)
	OnError(errno);
    }
    return f;
  }

  
  inline FIELD* get_field() const {
    return field;
  }

  
  inline void info(int& rows, int& ncols,
		   int& first_row, int& first_col,
		   int& offscreen_rows, int& additional_buffers) const {
    OnError(::field_info(field, &rows, &ncols,
			 &first_row, &first_col,
			 &offscreen_rows, &additional_buffers));
  }

  
  inline void dynamic_info(int& dynamic_rows, int& dynamic_cols,
			   int& max_growth) const {
    OnError(::dynamic_field_info(field, &dynamic_rows, &dynamic_cols,
				 &max_growth));
  }

  
  
  inline void set_maximum_growth(int growth = 0) {
    OnError(::set_max_field(field,growth));
  }

  
  inline void move(int row, int col) {
    OnError(::move_field(field,row,col));
  }

  
  inline void new_page(bool pageFlag = FALSE) {
    OnError(::set_new_page(field,pageFlag));
  }

  
  inline bool is_new_page() const {
    return ::new_page(field);
  }

  
  inline void set_justification(int just) {
    OnError(::set_field_just(field,just));
  }

  
  inline int justification() const {
    return ::field_just(field);
  }
  
  inline void set_foreground(chtype foreground) {
    OnError(::set_field_fore(field,foreground));
  }

  
  inline chtype fore() const {
    return ::field_fore(field);
  }

  
  inline void set_background(chtype background) {
    OnError(::set_field_back(field,background));
  }

  
  inline chtype back() const {
    return ::field_back(field);
  }

  
  inline void set_pad_character(int padding) {
    OnError(::set_field_pad(field, padding));
  }

  
  inline int pad() const {
    return ::field_pad(field);
  }

  
  inline void options_on (Field_Options opts) {
    OnError (::field_opts_on (field, opts));
  }

  
  inline void options_off (Field_Options opts) {
    OnError (::field_opts_off (field, opts));
  }

  
  inline Field_Options options () const {
    return ::field_opts (field);
  }

  
  inline void set_options (Field_Options opts) {
    OnError (::set_field_opts (field, opts));
  }

  
  inline void set_changed(bool changeFlag = TRUE) {
    OnError(::set_field_status(field,changeFlag));
  }

  
  inline bool changed() const {
    return ::field_status(field);
  }

  
  
  inline int (index)() const {
    return ::field_index(field);
  }

  
  inline void set_value(const char *val, int buffer = 0) {
    OnError(::set_field_buffer(field,buffer,val));
  }

  
  inline char* value(int buffer = 0) const {
    return ::field_buffer(field,buffer);
  }

  
  inline void set_fieldtype(NCursesFieldType& f) {
    ftype = &f;
    f.set(*this); 
  }

  
  inline NCursesFieldType* fieldtype() const {
    return ftype;
  }

};

  
  
  
extern "C" {
  void _nc_xx_frm_init(FORM *);
  void _nc_xx_frm_term(FORM *);
  void _nc_xx_fld_init(FORM *);
  void _nc_xx_fld_term(FORM *);
}






class NCURSES_CXX_IMPEXP NCursesForm : public NCursesPanel
{
protected:
  FORM* form;  

private:
  NCursesWindow* sub;   
  bool b_sub_owner;     
  bool b_framed;	
  bool b_autoDelete;    

  NCursesFormField** my_fields; 

  
  
  typedef struct {
    void*	       m_user;	    
    const NCursesForm* m_back;      
    const FORM*	       m_owner;
  } UserHook;

  
  static inline NCursesForm* getHook(const FORM *f) {
    UserHook* hook = reinterpret_cast<UserHook*>(::form_userptr(f));
    assert(hook != 0 && hook->m_owner==f);
    return const_cast<NCursesForm*>(hook->m_back);
  }

  friend void _nc_xx_frm_init(FORM *);
  friend void _nc_xx_frm_term(FORM *);
  friend void _nc_xx_fld_init(FORM *);
  friend void _nc_xx_fld_term(FORM *);

  
  FIELD** mapFields(NCursesFormField* nfields[]);

protected:
  
  inline void set_user(void *user) {
    UserHook* uptr = reinterpret_cast<UserHook*>(::form_userptr (form));
    assert (uptr != 0 && uptr->m_back==this && uptr->m_owner==form);
    uptr->m_user = user;
  }

  inline void *get_user() {
    UserHook* uptr = reinterpret_cast<UserHook*>(::form_userptr (form));
    assert (uptr != 0 && uptr->m_back==this && uptr->m_owner==form);
    return uptr->m_user;
  }

  void InitForm (NCursesFormField* Fields[],
		 bool with_frame,
		 bool autoDeleteFields);

  inline void OnError (int err) const THROW2(NCursesException const, NCursesFormException) {
    if (err != E_OK)
      THROW(new NCursesFormException (err));
  }

  
  virtual int driver (int c) ;

  
  
  NCursesForm( int  nlines,
	       int  ncols,
	       int  begin_y = 0,
	       int  begin_x = 0)
    : NCursesPanel(nlines, ncols, begin_y, begin_x),
      form (STATIC_CAST(FORM*)(0)),
      sub(0),
      b_sub_owner(0),
      b_framed(0),
      b_autoDelete(0),
      my_fields(0)
  {
  }

public:
  
  NCursesForm (NCursesFormField* Fields[],
	       bool with_frame=FALSE,	      
	       bool autoDelete_Fields=FALSE)  
    : NCursesPanel(),
      form(0),
      sub(0),
      b_sub_owner(0),
      b_framed(0),
      b_autoDelete(0),
      my_fields(0)
  {
    InitForm(Fields, with_frame, autoDelete_Fields);
  }

  
  NCursesForm (NCursesFormField* Fields[],
	       int  nlines,
	       int  ncols,
	       int  begin_y,
	       int  begin_x,
	       bool with_frame=FALSE,	     
	       bool autoDelete_Fields=FALSE) 
    : NCursesPanel(nlines, ncols, begin_y, begin_x),
      form(0),
      sub(0),
      b_sub_owner(0),
      b_framed(0),
      b_autoDelete(0),
      my_fields(0)
  {
      InitForm(Fields, with_frame, autoDelete_Fields);
  }

  NCursesForm& operator=(const NCursesForm& rhs)
  {
    if (this != &rhs) {
      *this = rhs;
      NCursesPanel::operator=(rhs);
    }
    return *this;
  }

  NCursesForm(const NCursesForm& rhs)
    : NCursesPanel(rhs),
      form(rhs.form),
      sub(rhs.sub),
      b_sub_owner(rhs.b_sub_owner),
      b_framed(rhs.b_framed),
      b_autoDelete(rhs.b_autoDelete),
      my_fields(rhs.my_fields)
  {
  }

  virtual ~NCursesForm() THROWS(NCursesException);

  
  virtual void setDefaultAttributes();

  
  inline NCursesFormField* current_field() const {
    return my_fields[::field_index(::current_field(form))];
  }

  
  void setSubWindow(NCursesWindow& sub);

  
  inline void setFields(NCursesFormField* Fields[]) {
    OnError(::set_form_fields(form,mapFields(Fields)));
  }

  
  inline void unpost (void) {
    OnError (::unpost_form (form));
  }

  
  inline void post(bool flag = TRUE) {
    OnError (flag ? ::post_form(form) : ::unpost_form (form));
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

  
  
  

  
  
  virtual void On_Form_Init();

  
  
  virtual void On_Form_Termination();

  
  virtual void On_Field_Init(NCursesFormField& field);

  
  virtual void On_Field_Termination(NCursesFormField& field);

  
  void scale(int& rows, int& ncols) const {
    OnError(::scale_form(form,&rows,&ncols));
  }

  
  int count() const {
    return ::field_count(form);
  }

  
  void set_page(int pageNum) {
    OnError(::set_form_page(form, pageNum));
  }

  
  int page() const {
    return ::form_page(form);
  }

  
  inline void options_on (Form_Options opts) {
    OnError (::form_opts_on (form, opts));
  }

  
  inline void options_off (Form_Options opts) {
    OnError (::form_opts_off (form, opts));
  }

  
  inline Form_Options options () const {
    return ::form_opts (form);
  }

  
  inline void set_options (Form_Options opts) {
    OnError (::set_form_opts (form, opts));
  }

  
  inline bool data_ahead() const {
    return ::data_ahead(form);
  }

  
  inline bool data_behind() const {
    return ::data_behind(form);
  }

  
  inline void position_cursor () {
    OnError (::pos_form_cursor (form));
  }
  
  inline void set_current(NCursesFormField& F) {
    OnError (::set_current_field(form, F.field));
  }

  
  
  
  
  virtual int virtualize(int c);

  
  inline NCursesFormField* operator[](int i) const {
    if ( (i < 0) || (i >= ::field_count (form)) )
      OnError (E_BAD_ARGUMENT);
    return my_fields[i];
  }

  
  
  virtual NCursesFormField* operator()(void);

  
  virtual void On_Request_Denied(int c) const;
  virtual void On_Invalid_Field(int c) const;
  virtual void On_Unknown_Command(int c) const;

};








template<class T> class NCURSES_CXX_IMPEXP NCursesUserField : public NCursesFormField
{
public:
  NCursesUserField (int rows,
		    int ncols,
		    int first_row = 0,
		    int first_col = 0,
		    const T* p_UserData = STATIC_CAST(T*)(0),
		    int offscreen_rows = 0,
		    int additional_buffers = 0)
    : NCursesFormField (rows, ncols,
			first_row, first_col,
			offscreen_rows, additional_buffers) {
      if (field)
	OnError(::set_field_userptr(field, STATIC_CAST(void *)(p_UserData)));
  }

  virtual ~NCursesUserField() THROWS(NCursesException) {};

  inline const T* UserData (void) const {
    return reinterpret_cast<const T*>(::field_userptr (field));
  }

  inline virtual void setUserData(const T* p_UserData) {
    if (field)
      OnError (::set_field_userptr (field, STATIC_CAST(void *)(p_UserData)));
  }
};





template<class T> class NCURSES_CXX_IMPEXP NCursesUserForm : public NCursesForm
{
protected:
  
  
  NCursesUserForm( int  nlines,
		   int  ncols,
		   int  begin_y = 0,
		   int  begin_x = 0,
		   const T* p_UserData = STATIC_CAST(T*)(0))
    : NCursesForm(nlines,ncols,begin_y,begin_x) {
      if (form)
	set_user (const_cast<void *>(reinterpret_cast<const void*>
				     (p_UserData)));
  }

public:
  NCursesUserForm (NCursesFormField* Fields[],
		   const T* p_UserData = STATIC_CAST(T*)(0),
		   bool with_frame=FALSE,
		   bool autoDelete_Fields=FALSE)
    : NCursesForm (Fields, with_frame, autoDelete_Fields) {
      if (form)
	set_user (const_cast<void *>(reinterpret_cast<const void*>(p_UserData)));
  };

  NCursesUserForm (NCursesFormField* Fields[],
		   int nlines,
		   int ncols,
		   int begin_y = 0,
		   int begin_x = 0,
		   const T* p_UserData = STATIC_CAST(T*)(0),
		   bool with_frame=FALSE,
		   bool autoDelete_Fields=FALSE)
    : NCursesForm (Fields, nlines, ncols, begin_y, begin_x,
		   with_frame, autoDelete_Fields) {
      if (form)
	set_user (const_cast<void *>(reinterpret_cast<const void*>
				     (p_UserData)));
  };

  virtual ~NCursesUserForm() THROWS(NCursesException) {
  };

  inline T* UserData (void) {
    return reinterpret_cast<T*>(get_user ());
  };

  inline virtual void setUserData (const T* p_UserData) {
    if (form)
      set_user (const_cast<void *>(reinterpret_cast<const void*>(p_UserData)));
  }

};





class NCURSES_CXX_IMPEXP Alpha_Field : public NCursesFieldType
{
private:
  int min_field_width;

  void set(NCursesFormField& f) NCURSES_OVERRIDE {
    OnError(::set_field_type(f.get_field(),fieldtype,min_field_width));
  }

public:
  explicit Alpha_Field(int width)
    : NCursesFieldType(TYPE_ALPHA),
      min_field_width(width) {
  }
};

class NCURSES_CXX_IMPEXP Alphanumeric_Field : public NCursesFieldType
{
private:
  int min_field_width;

  void set(NCursesFormField& f) NCURSES_OVERRIDE {
    OnError(::set_field_type(f.get_field(),fieldtype,min_field_width));
  }

public:
  explicit Alphanumeric_Field(int width)
    : NCursesFieldType(TYPE_ALNUM),
      min_field_width(width) {
  }
};

class NCURSES_CXX_IMPEXP Integer_Field : public NCursesFieldType
{
private:
  int precision;
  long lower_limit, upper_limit;

  void set(NCursesFormField& f) NCURSES_OVERRIDE {
    OnError(::set_field_type(f.get_field(),fieldtype,
			     precision,lower_limit,upper_limit));
  }

public:
  Integer_Field(int prec, long low=0L, long high=0L)
    : NCursesFieldType(TYPE_INTEGER),
      precision(prec), lower_limit(low), upper_limit(high) {
  }
};

class NCURSES_CXX_IMPEXP Numeric_Field : public NCursesFieldType
{
private:
  int precision;
  double lower_limit, upper_limit;

  void set(NCursesFormField& f) NCURSES_OVERRIDE {
    OnError(::set_field_type(f.get_field(),fieldtype,
			     precision,lower_limit,upper_limit));
  }

public:
  Numeric_Field(int prec, double low=0.0, double high=0.0)
    : NCursesFieldType(TYPE_NUMERIC),
      precision(prec), lower_limit(low), upper_limit(high) {
  }
};

class NCURSES_CXX_IMPEXP Regular_Expression_Field : public NCursesFieldType
{
private:
  char* regex;

  void set(NCursesFormField& f) NCURSES_OVERRIDE {
    OnError(::set_field_type(f.get_field(),fieldtype,regex));
  }

  void copy_regex(const char *source)
  {
    regex = new char[1 + ::strlen(source)];
    (::strcpy)(regex, source);
  }

public:
  explicit Regular_Expression_Field(const char *expr)
    : NCursesFieldType(TYPE_REGEXP),
      regex(NULL)
  {
    copy_regex(expr);
  }

  Regular_Expression_Field& operator=(const Regular_Expression_Field& rhs)
  {
    if (this != &rhs) {
      *this = rhs;
      copy_regex(rhs.regex);
      NCursesFieldType::operator=(rhs);
    }
    return *this;
  }

  Regular_Expression_Field(const Regular_Expression_Field& rhs)
    : NCursesFieldType(rhs),
      regex(NULL)
  {
    copy_regex(rhs.regex);
  }

  ~Regular_Expression_Field() {
    delete[] regex;
  }
};

class NCURSES_CXX_IMPEXP Enumeration_Field : public NCursesFieldType
{
private:
  const char** list;
  int case_sensitive;
  int non_unique_matches;

  void set(NCursesFormField& f) NCURSES_OVERRIDE {
    OnError(::set_field_type(f.get_field(),fieldtype,
			     list,case_sensitive,non_unique_matches));
  }
public:
  Enumeration_Field(const char* enums[],
		    bool case_sens=FALSE,
		    bool non_unique=FALSE)
    : NCursesFieldType(TYPE_ENUM),
      list(enums),
      case_sensitive(case_sens ? -1 : 0),
      non_unique_matches(non_unique ? -1 : 0) {
  }

  Enumeration_Field& operator=(const Enumeration_Field& rhs)
  {
    if (this != &rhs) {
      *this = rhs;
      NCursesFieldType::operator=(rhs);
    }
    return *this;
  }

  Enumeration_Field(const Enumeration_Field& rhs)
    : NCursesFieldType(rhs),
      list(rhs.list),
      case_sensitive(rhs.case_sensitive),
      non_unique_matches(rhs.non_unique_matches)
  {
  }
};

class NCURSES_CXX_IMPEXP IPV4_Address_Field : public NCursesFieldType
{
private:
  void set(NCursesFormField& f) NCURSES_OVERRIDE {
    OnError(::set_field_type(f.get_field(),fieldtype));
  }

public:
  IPV4_Address_Field() : NCursesFieldType(TYPE_IPV4) {
  }
};

extern "C" {
  bool _nc_xx_fld_fcheck(FIELD *, const void*);
  bool _nc_xx_fld_ccheck(int c, const void *);
  void* _nc_xx_fld_makearg(va_list*);
}






class NCURSES_CXX_IMPEXP UserDefinedFieldType : public NCursesFieldType
{
  friend class UDF_Init; 
private:
  
  
  static FIELDTYPE* generic_fieldtype;

protected:
  
  
  friend bool _nc_xx_fld_fcheck(FIELD *, const void*);
  friend bool _nc_xx_fld_ccheck(int c, const void *);
  friend void* _nc_xx_fld_makearg(va_list*);

  void set(NCursesFormField& f) NCURSES_OVERRIDE {
    OnError(::set_field_type(f.get_field(),fieldtype,&f));
  }

protected:
  
  
  virtual bool field_check(NCursesFormField& f) = 0;

  
  
  virtual bool char_check (int c) = 0;

public:
  UserDefinedFieldType();
};

extern "C" {
  bool _nc_xx_next_choice(FIELD*, const void *);
  bool _nc_xx_prev_choice(FIELD*, const void *);
}






class NCURSES_CXX_IMPEXP UserDefinedFieldType_With_Choice : public UserDefinedFieldType
{
  friend class UDF_Init; 
private:
  
  
  static FIELDTYPE* generic_fieldtype_with_choice;

  
  
  friend bool _nc_xx_next_choice(FIELD*, const void *);
  friend bool _nc_xx_prev_choice(FIELD*, const void *);

protected:
  
  
  virtual bool next    (NCursesFormField& f) = 0;

  
  
  virtual bool previous(NCursesFormField& f) = 0;

public:
  UserDefinedFieldType_With_Choice();
};

#endif  
