
 

 

#include "internal.h"
#include "cursslk.h"
#include "cursesapp.h"

MODULE_ID("$Id: cursslk.cc,v 1.20 2022/02/26 17:57:23 tom Exp $")

Soft_Label_Key_Set::Soft_Label_Key&
  Soft_Label_Key_Set::Soft_Label_Key::operator=(char *text)
{
  delete[] label;
  size_t need = 1 + ::strlen(text);
  label = new char[need];
  ::_nc_STRCPY(label,text,need);
  return *this;
}

long Soft_Label_Key_Set::count      = 0L;
int  Soft_Label_Key_Set::num_labels = 0;

Soft_Label_Key_Set::Label_Layout
  Soft_Label_Key_Set::format = None;

void Soft_Label_Key_Set::init()
{
  if (num_labels > 12)
      num_labels = 12;
  slk_array = new Soft_Label_Key[num_labels];
  for(int i=0; i < num_labels; i++) {
    slk_array[i].num = i+1;
  }
  b_attrInit = FALSE;
}

Soft_Label_Key_Set::Soft_Label_Key_Set()
  : b_attrInit(FALSE),
    slk_array(NULL)
{
  if (format==None)
    Error("No default SLK layout");
  init();
}

Soft_Label_Key_Set::Soft_Label_Key_Set(Soft_Label_Key_Set::Label_Layout fmt)
  : b_attrInit(FALSE),
    slk_array(NULL)
{
  if (fmt==None)
    Error("Invalid SLK Layout");
  if (count++==0) {
    format = fmt;
    if (ERR == ::slk_init(static_cast<int>(fmt)))
      Error("slk_init");
    num_labels = (fmt>=PC_Style?12:8);
  }
  else if (fmt!=format)
    Error("All SLKs must have same layout");
  init();
}

Soft_Label_Key_Set::~Soft_Label_Key_Set() THROWS(NCursesException) {
  if (!::isendwin())
    clear();
  delete[] slk_array;
  count--;
}

Soft_Label_Key_Set::Soft_Label_Key& Soft_Label_Key_Set::operator[](int i) {
  if (i<1 || i>num_labels)
    Error("Invalid Label index");
  return slk_array[i-1];
}

int Soft_Label_Key_Set::labels() const {
  return num_labels;
}

void Soft_Label_Key_Set::activate_label(int i, bool bf) {
  if (!b_attrInit) {
    NCursesApplication* A = NCursesApplication::getApplication();
    if (A) attrset(A->labels());
    b_attrInit = TRUE;
  }
  Soft_Label_Key& K = (*this)[i];
  if (ERR==::slk_set(K.num,bf?K.label:"",K.format))
    Error("slk_set");
  noutrefresh();
}

void Soft_Label_Key_Set::activate_labels(bool bf)
{
  if (!b_attrInit) {
    NCursesApplication* A = NCursesApplication::getApplication();
    if (A) attrset(A->labels());
    b_attrInit = TRUE;
  }
  for(int i=1; i <= num_labels; i++) {
    Soft_Label_Key& K = (*this)[i];
    if (ERR==::slk_set(K.num,bf?K.label:"",K.format))
      Error("slk_set");
  }
  if (bf)
    restore();
  else
    clear();
  noutrefresh();
}
